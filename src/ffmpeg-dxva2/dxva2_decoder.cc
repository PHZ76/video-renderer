#include "dxva2_decoder.h"
#include "av_log.h"

#include "libavcodec/dxva2.h"
#include "libavutil/hwcontext.h"
#include "libavutil/hwcontext_dxva2.h"


static AVPixelFormat get_dxva2_hw_format(AVCodecContext* avctx, const enum AVPixelFormat* pix_fmts)
{
	while (*pix_fmts != AV_PIX_FMT_NONE) {
		if (*pix_fmts == AV_PIX_FMT_DXVA2_VLD) {
			AVBufferRef* hw_device_ref = (AVBufferRef*)avctx->opaque;
			AVHWFramesContext* frames_ctx = nullptr;
			AVDXVA2FramesContext* frames_hwctx = nullptr;
			avctx->hw_frames_ctx = av_hwframe_ctx_alloc(hw_device_ref);
			if (!avctx->hw_frames_ctx) {
				return AV_PIX_FMT_NONE;
			}

			frames_ctx = (AVHWFramesContext*)avctx->hw_frames_ctx->data;
			frames_hwctx = (AVDXVA2FramesContext*)frames_ctx->hwctx;

			frames_ctx->format = AV_PIX_FMT_DXVA2_VLD;
			frames_ctx->sw_format = AV_PIX_FMT_NV12;
			frames_ctx->width = FFALIGN(avctx->coded_width, 32);
			frames_ctx->height = FFALIGN(avctx->coded_height, 32);
			frames_ctx->initial_pool_size = 10;

			int ret = av_hwframe_ctx_init(avctx->hw_frames_ctx);
			if (ret < 0) {
				return AV_PIX_FMT_NONE;
			}

			return AV_PIX_FMT_DXVA2_VLD;
		}

		pix_fmts++;
	}

	LOG("Failed to get HW surface format.");
	return AV_PIX_FMT_NONE;
}

typedef struct DXVA2DevicePriv {
	HMODULE d3dlib;
	HMODULE dxva2lib;

	HANDLE device_handle;

	IDirect3D9* d3d9;
	IDirect3DDevice9* d3d9device;
} DXVA2DevicePriv;

typedef HRESULT WINAPI pCreateDeviceManager9(UINT*, IDirect3DDeviceManager9**);

static void dxva2_device_free(AVHWDeviceContext* device_context)
{
	AVDXVA2DeviceContext* dxva2_device_context = (AVDXVA2DeviceContext*)device_context->hwctx;
	DXVA2DevicePriv* dxva2_device_priv = (DXVA2DevicePriv*)device_context->user_opaque;

	if (dxva2_device_context->devmgr && dxva2_device_priv->device_handle != INVALID_HANDLE_VALUE) {
		dxva2_device_context->devmgr->CloseDeviceHandle(dxva2_device_priv->device_handle);
	}
		
	if (dxva2_device_context->devmgr) {
		dxva2_device_context->devmgr->Release();
	}

	if (dxva2_device_priv->d3d9device) {
		dxva2_device_priv->d3d9device->Release();
	}
		
	if (dxva2_device_priv->d3d9) {
		dxva2_device_priv->d3d9->Release();
	}
		

	if (dxva2_device_priv->d3dlib) {
		FreeLibrary(dxva2_device_priv->d3dlib);
	}
		
	if (dxva2_device_priv->dxva2lib) {
		FreeLibrary(dxva2_device_priv->dxva2lib);
	}
		
	av_freep(&device_context->user_opaque);
}


static int dxva2_device_create2(AVHWDeviceContext* device_context, IDirect3DDevice9* d3d9_device)
{
	AVDXVA2DeviceContext* dxva2_device_context = (AVDXVA2DeviceContext*)device_context->hwctx;
	DXVA2DevicePriv* dxva2_device_priv = nullptr;
	pCreateDeviceManager9* createDeviceManager = NULL;

	unsigned reset_token = 0;
	UINT adapter = D3DADAPTER_DEFAULT;
	HRESULT hr = S_OK;
	int err = 0;

	dxva2_device_priv = (DXVA2DevicePriv*)av_mallocz(sizeof(*dxva2_device_priv));
	if (!dxva2_device_priv) {
		return AVERROR(ENOMEM);
	}

	device_context->user_opaque = dxva2_device_priv;
	device_context->free = dxva2_device_free;

	dxva2_device_priv->device_handle = INVALID_HANDLE_VALUE;

	dxva2_device_priv->d3dlib = LoadLibraryA("d3d9.dll");
	if (!dxva2_device_priv->d3dlib) {
		av_log(device_context, AV_LOG_ERROR, "Failed to load D3D9 library\n");
		return AVERROR_UNKNOWN;
	}

	dxva2_device_priv->dxva2lib = LoadLibraryA("dxva2.dll");
	if (!dxva2_device_priv->dxva2lib) {
		av_log(device_context, AV_LOG_ERROR, "Failed to load DXVA2 library\n");
		return AVERROR_UNKNOWN;
	}

	createDeviceManager = (pCreateDeviceManager9*)GetProcAddress(dxva2_device_priv->dxva2lib,"DXVA2CreateDirect3DDeviceManager9");
	if (!createDeviceManager) {
		av_log(device_context, AV_LOG_ERROR, "Failed to locate DXVA2CreateDirect3DDeviceManager9\n");
		return AVERROR_UNKNOWN;
	}

	d3d9_device->AddRef();
	dxva2_device_priv->d3d9device = d3d9_device;

	hr = createDeviceManager(&reset_token, &dxva2_device_context->devmgr);
	if (FAILED(hr)) {
		av_log(device_context, AV_LOG_ERROR, "Failed to create Direct3D device manager\n");
		return AVERROR_UNKNOWN;
	}

	dxva2_device_context->devmgr->ResetDevice(dxva2_device_priv->d3d9device, reset_token);
	if (FAILED(hr)) {
		av_log(device_context, AV_LOG_ERROR, "Failed to bind Direct3D device to device manager\n");
		return AVERROR_UNKNOWN;
	}

	hr = dxva2_device_context->devmgr->OpenDeviceHandle(&dxva2_device_priv->device_handle);
	if (FAILED(hr)) {
		av_log(device_context, AV_LOG_ERROR, "Failed to open device handle\n");
		return AVERROR_UNKNOWN;
	}

	return 0;
}

AVDecoder::AVDecoder()
{

}

AVDecoder::~AVDecoder()
{
	Destroy();
}


bool AVDecoder::Init(AVStream* stream, void* d3d9_device)
{
	if (codec_context_ != nullptr) {
		LOG("codec was opened.");
		return false;
	}

	if (!stream) {
		return false;
	}

	if (stream->codecpar->codec_type != AVMEDIA_TYPE_VIDEO) {
		return false;
	}

	AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
	if (!codec) {
		LOG("decoder(%s) not found.", avcodec_get_name(stream->codecpar->codec_id));
		return false;
	}

	AVHWDeviceContext* device_context = nullptr;
	AVDXVA2DeviceContext* d3d9_device_context = nullptr;
	AVHWDeviceType hw_type = AV_HWDEVICE_TYPE_DXVA2;

	codec_context_ = avcodec_alloc_context3(codec);
	if (avcodec_parameters_to_context(codec_context_, stream->codecpar) < 0) {
		LOG("avcodec_parameters_to_context() failed.");
		goto failed;
	}

	for (int i = 0;; i++) {
		const AVCodecHWConfig* config = avcodec_get_hw_config(codec, i);
		if (!config) {
			LOG("Decoder %s does not support device type %s.\n",
				codec->name, av_hwdevice_get_type_name(hw_type));
			goto failed;
		}
		if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
			config->device_type == hw_type) {
			break;
		}
	}

	if (d3d9_device) {
		device_buffer_ = av_hwdevice_ctx_alloc(hw_type);
		device_context = (AVHWDeviceContext*)device_buffer_->data;
		d3d9_device_context = (AVDXVA2DeviceContext*)device_context->hwctx;
		codec_context_->opaque = device_buffer_;
		dxva2_device_create2(device_context, (IDirect3DDevice9*)d3d9_device);
	}
	else {
		 av_hwdevice_ctx_create(&device_buffer_, hw_type, NULL, NULL, 0);
	}

	codec_context_->hw_device_ctx = av_buffer_ref(device_buffer_);
	codec_context_->get_format = get_dxva2_hw_format;
	codec_context_->thread_count = 1;
	codec_context_->pkt_timebase = stream->time_base;

	if (avcodec_open2(codec_context_, codec, NULL) != 0) {
		LOG("Open decoder(%d) failed.", (int)stream->codecpar->codec_id);
		goto failed;
	}

	stream->discard = AVDISCARD_DEFAULT;
	start_pts_ = stream->start_time;
	next_pts_ = AV_NOPTS_VALUE;
	start_pts_tb_ = stream->time_base;

	finished_ = 0;
	next_pts_ = start_pts_;
	next_pts_tb_ = start_pts_tb_;
	stream_ = stream;

	return true;

failed:
	if (codec_context_) {
		avcodec_free_context(&codec_context_);
		codec_context_ = nullptr;
	}
	return false;
}

void AVDecoder::Destroy()
{
	if (codec_context_ != nullptr) {
		avcodec_close(codec_context_);
		avcodec_free_context(&codec_context_);
		codec_context_ = nullptr;
		stream_ = nullptr;
	}

	if (device_buffer_) {
		av_buffer_unref(&device_buffer_);
		device_buffer_ = nullptr;
	}

	start_pts_ = AV_NOPTS_VALUE;
	next_pts_ = AV_NOPTS_VALUE;
}

int AVDecoder::Send(AVPacket* packet)
{
	std::lock_guard<std::mutex> locker(mutex_);

	if (codec_context_ == NULL) {
		return -1;
	}

	int ret = avcodec_send_packet(codec_context_, packet);
	return ret;
}

int AVDecoder::Recv(AVFrame* frame)
{
	int ret = -1;

	if (codec_context_ == NULL) {
		return ret;
	}

	switch (codec_context_->codec_type)
	{
	case AVMEDIA_TYPE_VIDEO:
		ret = avcodec_receive_frame(codec_context_, frame);
		if (ret >= 0) {
			if (decoder_reorder_pts_ == -1) {
				frame->pts = frame->best_effort_timestamp;
			}
			else if (!decoder_reorder_pts_) {
				frame->pts = frame->pkt_dts;
			}
		}
		break;

	default:
		break;
	}

	if (ret == AVERROR_EOF) {
		avcodec_flush_buffers(codec_context_);
	}

	return ret;
}

