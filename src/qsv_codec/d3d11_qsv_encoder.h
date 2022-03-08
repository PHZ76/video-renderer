#pragma once

#include "qsv_encoder.h"
#include "common_directx11.h"
#include <cstdint>
#include <string>
#include <vector>
#include <memory>

class D3D11QSVEncoder : public QSVEncoder
{
public:
	D3D11QSVEncoder(ID3D11Device* d3d11_device);
	virtual ~D3D11QSVEncoder();

	virtual bool Init();
	virtual void Destroy();
	
	virtual int  Encode(HANDLE handle, std::vector<uint8_t>& out_frame);
	virtual int  Encode(ID3D11Texture2D* input_texture, std::vector<uint8_t>& out_frame);

private:
	bool InitEncoder();
	bool AllocBuffer();
	bool FreeBuffer();
	bool GetVideoParam();
	int  EncodeFrame(int index, std::vector<uint8_t>& out_frame);

	ID3D11Device* d3d11_device_ = NULL;
	ID3D11DeviceContext* d3d11_context_ = NULL;

	std::unique_ptr<MFXVideoENCODE> mfx_encoder_;

	MFXVideoSession        mfx_session_;
	mfxFrameAllocator      mfx_allocator_;
	mfxVideoParam          mfx_enc_params_;
	mfxVideoParam          mfx_video_params_;	
	mfxFrameAllocResponse  mfx_alloc_response_;
	mfxExtCodingOption     extended_coding_options_;
	mfxExtCodingOption2    extended_coding_options2_;
	mfxExtBuffer*          extended_buffers_[2];
	mfxEncodeCtrl          enc_ctrl_;

	mfxBitstream           mfx_enc_bs_;
	std::vector<mfxU8>     bst_enc_data_;
	std::vector<mfxFrameSurface1> mfx_surfaces_;
};
