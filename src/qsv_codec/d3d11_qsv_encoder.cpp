#include "d3d11_qsv_encoder.h"
#include "common_directx11.h"
#include <Windows.h>
#include <versionhelpers.h>

D3D11QSVEncoder::D3D11QSVEncoder(ID3D11Device* d3d11_device)
	: d3d11_device_(d3d11_device)
{
	if (d3d11_device_) {
		d3d11_device_->AddRef();
		d3d11_device_->GetImmediateContext(&d3d11_context_);
	}
	
	mfx_impl_ |= MFX_IMPL_VIA_D3D11;
}

D3D11QSVEncoder::~D3D11QSVEncoder()
{
	Destroy();

	if (d3d11_context_) {
		d3d11_context_->Release();
	}

	if (d3d11_device_) {
		d3d11_device_->Release();
	}
}

bool D3D11QSVEncoder::Init()
{
	mfxStatus sts = MFX_ERR_NONE;

	sts = d3d11::Initialize(mfx_impl_, mfx_ver_, &mfx_session_, &mfx_allocator_, d3d11_device_);
	if (sts != MFX_ERR_NONE) {
		return false;
	}

	if (!InitEncoder()) {
		return false;
	}

	if (!AllocBuffer()) {
		mfx_encoder_.reset();
		return false;
	}

	return true;
}

void D3D11QSVEncoder::Destroy()
{
	FreeBuffer();
	mfx_encoder_.reset();
}

bool D3D11QSVEncoder::InitEncoder()
{
	mfx_encoder_.reset(new MFXVideoENCODE(mfx_session_));
	memset(&mfx_enc_params_, 0, sizeof(mfx_enc_params_));

	mfx_enc_params_.mfx.CodecId = MFX_CODEC_AVC;
	mfx_enc_params_.mfx.CodecProfile = MFX_PROFILE_AVC_HIGH;
	mfx_enc_params_.mfx.CodecLevel = MFX_LEVEL_AVC_41;
	if (enc_type_ == 265) {
		mfx_enc_params_.mfx.CodecId = MFX_CODEC_HEVC;
		mfx_enc_params_.mfx.CodecProfile = MFX_PROFILE_HEVC_MAIN;
	}
	
	mfx_enc_params_.mfx.GopOptFlag = MFX_GOP_STRICT;
	mfx_enc_params_.mfx.NumSlice = 1;
	mfx_enc_params_.mfx.TargetUsage = MFX_TARGETUSAGE_BEST_SPEED; //; MFX_TARGETUSAGE_BALANCED
	mfx_enc_params_.mfx.FrameInfo.FrameRateExtN = enc_framerate_;
	mfx_enc_params_.mfx.FrameInfo.FrameRateExtD = 1;
	mfx_enc_params_.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
	mfx_enc_params_.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
	mfx_enc_params_.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
	mfx_enc_params_.mfx.FrameInfo.CropX = 0;
	mfx_enc_params_.mfx.FrameInfo.CropY = 0;
	mfx_enc_params_.mfx.FrameInfo.CropW = enc_width_;
	mfx_enc_params_.mfx.FrameInfo.CropH = enc_height_;
	mfx_enc_params_.mfx.RateControlMethod = MFX_RATECONTROL_CBR;
	mfx_enc_params_.mfx.TargetKbps = enc_bitrate_kbps_;
	//mfx_enc_params_.mfx.MaxKbps = param.bitrate_kbps;
	mfx_enc_params_.mfx.GopPicSize = (mfxU16)enc_gop_;
	mfx_enc_params_.mfx.IdrInterval = (mfxU16)enc_gop_;

	// Width must be a multiple of 16
	// Height must be a multiple of 16 in case of frame picture and a
	// multiple of 32 in case of field picture
	mfx_enc_params_.mfx.FrameInfo.Width = MSDK_ALIGN16(enc_width_);
	mfx_enc_params_.mfx.FrameInfo.Height = MSDK_ALIGN16(enc_height_);
	if (MFX_PICSTRUCT_PROGRESSIVE != mfx_enc_params_.mfx.FrameInfo.PicStruct) {
		MSDK_ALIGN32(enc_height_);
	}

	// d3d11
	mfx_enc_params_.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;

	// Configuration for low latency
	mfx_enc_params_.AsyncDepth = 1;  //1 is best for low latency
	mfx_enc_params_.mfx.GopRefDist = 1; //1 is best for low latency, I and P frames only

	memset(&extended_coding_options_, 0, sizeof(mfxExtCodingOption));
	extended_coding_options_.Header.BufferId = MFX_EXTBUFF_CODING_OPTION;
	extended_coding_options_.Header.BufferSz = sizeof(mfxExtCodingOption);

	//extended_coding_options_.RefPicMarkRep = MFX_CODINGOPTION_ON;
	//extended_coding_options_.VuiNalHrdParameters = MFX_CODINGOPTION_OFF;
	//extended_coding_options_.VuiVclHrdParameters = MFX_CODINGOPTION_OFF;

	extended_coding_options_.NalHrdConformance = MFX_CODINGOPTION_OFF;
	extended_coding_options_.PicTimingSEI = MFX_CODINGOPTION_OFF;
	extended_coding_options_.AUDelimiter = MFX_CODINGOPTION_OFF;
	extended_coding_options_.MaxDecFrameBuffering = 1;

	memset(&extended_coding_options2_, 0, sizeof(mfxExtCodingOption2));
	extended_coding_options2_.Header.BufferId = MFX_EXTBUFF_CODING_OPTION2;
	extended_coding_options2_.Header.BufferSz = sizeof(mfxExtCodingOption2);
	extended_coding_options2_.RepeatPPS = MFX_CODINGOPTION_OFF;

	extended_buffers_[0] = (mfxExtBuffer*)(&extended_coding_options_);
	extended_buffers_[1] = (mfxExtBuffer*)(&extended_coding_options2_);
	mfx_enc_params_.ExtParam = extended_buffers_;
	mfx_enc_params_.NumExtParam = 2;

	mfxStatus sts = MFX_ERR_NONE;

	sts = mfx_encoder_->Query(&mfx_enc_params_, &mfx_enc_params_);
	MSDK_IGNORE_MFX_STS(sts, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
	if (sts != MFX_ERR_NONE) {
		return false;
	}

	sts = mfx_encoder_->Init(&mfx_enc_params_);
	if (sts != MFX_ERR_NONE) {
		mfx_encoder_.reset();
		return false;
	}

	return true;
}

bool D3D11QSVEncoder::AllocBuffer()
{
	mfxStatus sts = MFX_ERR_NONE;

	// Query number of required surfaces for encoder
	mfxFrameAllocRequest enc_request;
	memset(&enc_request, 0, sizeof(mfxFrameAllocRequest));
	sts = mfx_encoder_->QueryIOSurf(&mfx_enc_params_, &enc_request);
	if (sts != MFX_ERR_NONE) {
		return false;
	}

	// This line is only required for Windows DirectX11 to ensure that surfaces can be written to by the application
	enc_request.Type |= WILL_WRITE;

	// Allocate required surfaces
	sts = mfx_allocator_.Alloc(mfx_allocator_.pthis, &enc_request, &mfx_alloc_response_);
	if (sts != MFX_ERR_NONE) {
		return false;
	}

	mfxU16 num_surfaces = mfx_alloc_response_.NumFrameActual;

	// Allocate surface headers (mfxFrameSurface1) for encoder
	mfx_surfaces_.resize(num_surfaces);
	for (int i = 0; i < num_surfaces; i++) {
		memset(&mfx_surfaces_[i], 0, sizeof(mfxFrameSurface1));
		mfx_surfaces_[i].Info = mfx_enc_params_.mfx.FrameInfo;
		mfx_surfaces_[i].Data.MemId = mfx_alloc_response_.mids[i];
	}

	mfxVideoParam param;
	memset(&param, 0, sizeof(mfxVideoParam));
	sts = mfx_encoder_->GetVideoParam(&param);
	if (sts != MFX_ERR_NONE) {
		return false;
	}

	memset(&mfx_enc_bs_, 0, sizeof(mfxBitstream));
	mfx_enc_bs_.MaxLength = param.mfx.BufferSizeInKB * 1000;
	bst_enc_data_.resize(mfx_enc_bs_.MaxLength);
	mfx_enc_bs_.Data = bst_enc_data_.data();

	return true;
}

bool D3D11QSVEncoder::FreeBuffer()
{
	if (mfx_alloc_response_.NumFrameActual > 0) {
		mfx_allocator_.Free(mfx_allocator_.pthis, &mfx_alloc_response_);
		memset(&mfx_alloc_response_, 0, sizeof(mfxFrameAllocResponse));
	}

	memset(&mfx_enc_bs_, 0, sizeof(mfxBitstream));
	bst_enc_data_.clear();

	return true;
}

bool D3D11QSVEncoder::GetVideoParam()
{
	mfxExtCodingOptionSPSPPS opt;
	memset(&mfx_video_params_, 0, sizeof(mfxVideoParam));
	opt.Header.BufferId = MFX_EXTBUFF_CODING_OPTION_SPSPPS;
	opt.Header.BufferSz = sizeof(mfxExtCodingOptionSPSPPS);

	static mfxExtBuffer *extendedBuffers[1];
	extendedBuffers[0] = (mfxExtBuffer *)&opt;
	mfx_video_params_.ExtParam = extendedBuffers;
	mfx_video_params_.NumExtParam = 1;

	opt.SPSBuffer = sps_buffer_.get();
	opt.PPSBuffer = pps_buffer_.get();
	opt.SPSBufSize = 1024;
	opt.PPSBufSize = 1024;

	mfxStatus sts = mfx_encoder_->GetVideoParam(&mfx_video_params_);
	if (sts != MFX_ERR_NONE) {
		return false;
	}

	sps_size_ = opt.SPSBufSize;
	pps_size_ = opt.PPSBufSize;

	//printf("\n");
	//for (uint32_t i = 0; i < 150 && i < sps_size_; i++) {
	//	printf("%x ", sps_buffer_.get()[i]);
	//}
	//printf("\n");

	return true;
}

int D3D11QSVEncoder::Encode(HANDLE handle, std::vector<uint8_t>& out_frame)
{
	if (!mfx_encoder_) {
		return MFX_ERR_NULL_PTR;
	}

	ID3D11Texture2D* input_texture = NULL;

	HRESULT hr = d3d11_device_->OpenSharedResource(handle, __uuidof(ID3D11Texture2D), (void**)&input_texture);
	if (FAILED(hr)) {
		return MFX_ERR_INVALID_HANDLE;
	}

	int index = GetFreeSurfaceIndex(mfx_surfaces_); 
	MSDK_CHECK_ERROR(MFX_ERR_NOT_FOUND, index, MFX_ERR_MEMORY_ALLOC);

	d3d11::CustomMemId* memId = (d3d11::CustomMemId*)mfx_surfaces_[index].Data.MemId;
	ID3D11Texture2D* enc_texture = (ID3D11Texture2D*)memId->memId;

	D3D11_TEXTURE2D_DESC desc = { 0 };
	input_texture->GetDesc(&desc);

	D3D11_BOX src_box = { 0, 0, 0, desc.Width, desc.Height, 1 };
	d3d11_context_->CopySubresourceRegion(enc_texture, 0, 0, 0, 0, input_texture, 0, &src_box);
	input_texture->Release();

	int ret = EncodeFrame(index, out_frame);
	if (ret < 0) {

	}

	return ret;
}

int D3D11QSVEncoder::Encode(ID3D11Texture2D* input_texture, std::vector<uint8_t>& out_frame)
{
	if (!mfx_encoder_) {
		return MFX_ERR_NULL_PTR;
	}

	int index = GetFreeSurfaceIndex(mfx_surfaces_);
	MSDK_CHECK_ERROR(MFX_ERR_NOT_FOUND, index, MFX_ERR_MEMORY_ALLOC);

	d3d11::CustomMemId* memId = (d3d11::CustomMemId*)mfx_surfaces_[index].Data.MemId;
	ID3D11Texture2D* enc_texture = (ID3D11Texture2D*)memId->memId;

	D3D11_TEXTURE2D_DESC desc = { 0 };
	input_texture->GetDesc(&desc);

	D3D11_BOX src_box = { 0, 0, 0, desc.Width, desc.Height, 1 };
	d3d11_context_->CopySubresourceRegion(enc_texture, 0, 0, 0, 0, input_texture, 0, &src_box);

	int ret = EncodeFrame(index, out_frame);
	if (ret < 0) {

	}

	return ret;
}

int D3D11QSVEncoder::EncodeFrame(int index, std::vector<uint8_t>& out_frame)
{
	mfxSyncPoint syncp;
	mfxStatus sts = MFX_ERR_NONE;
	int frame_size = 0;

	for (;;) {
		// Encode a frame asychronously (returns immediately)
		mfxEncodeCtrl* enc_ctrl = nullptr;

		if (force_idr_ > 0) {
			enc_ctrl_.FrameType = MFX_FRAMETYPE_I | MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_REF;
			force_idr_ -= 1;
		}
		
		if (enc_ctrl_.FrameType) {
			enc_ctrl = &enc_ctrl_;
		}

		sts = mfx_encoder_->EncodeFrameAsync(enc_ctrl, &mfx_surfaces_[index], &mfx_enc_bs_, &syncp);
		enc_ctrl_.FrameType = 0;

		if (MFX_ERR_NONE < sts && !syncp) {  // Repeat the call if warning and no output
			if (MFX_WRN_DEVICE_BUSY == sts)
				MSDK_SLEEP(1);  // Wait if device is busy, then repeat the same call
		}
		else if (MFX_ERR_NONE < sts && syncp) {
			sts = MFX_ERR_NONE;     // Ignore warnings if output is available
			break;
		}
		else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts) {
			// Allocate more bitstream buffer memory here if needed...
			break;
		}
		else {
			break;
		}
	}

	if (MFX_ERR_NONE == sts) {
		sts = mfx_session_.SyncOperation(syncp, 60000);   // Synchronize. Wait until encoded frame is ready
		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

		if (mfx_enc_bs_.DataLength > 0) {
			//printf("encoder output frame: %u \n", mfx_enc_bs_.DataLength);
			out_frame.clear();
			out_frame.resize(mfx_enc_bs_.DataLength);
			memcpy(out_frame.data(), mfx_enc_bs_.Data, mfx_enc_bs_.DataLength);
			frame_size = mfx_enc_bs_.DataLength;
			mfx_enc_bs_.DataLength = 0;
		}
	}

	return frame_size;
}

#if 0
void D3D11QSVEncoder::SetBitrate(uint32_t bitrate_kbps)
{
	if (mfx_encoder_) {
		mfxVideoParam old_param;
		memset(&old_param, 0, sizeof(mfxVideoParam));
		mfxStatus status = mfx_encoder_->GetVideoParam(&old_param);
		MSDK_IGNORE_MFX_STS(status, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);

		if (status == MFX_ERR_NONE) {
			uint32_t old_bitrate = old_param.mfx.TargetKbps;
			old_param.mfx.TargetKbps = bitrate_kbps;
			status = mfx_encoder_->Reset(&old_param);
			MSDK_IGNORE_MFX_STS(status, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
			if (status == MFX_ERR_NONE) {
				printf("Reset bitrate:%u, old bitrate:%u \n", bitrate_kbps, old_bitrate);
			}
			else {
				printf("[Reset bitrate failed, bitrate:%u, status:%d \n", old_param.mfx.TargetKbps, status);
			}
		}
		else {
			printf("GetVideoParam() failed \n");
		}
	}
}
#endif