#include "d3d11va_decoder.h"

extern "C" {
#include "libavutil/hwcontext.h"
#include "libavutil/hwcontext_d3d11va.h"
}

static enum AVPixelFormat get_d3d11va_hw_format(AVCodecContext* avctx, const enum AVPixelFormat* pix_fmts)
{
	while (*pix_fmts != AV_PIX_FMT_NONE) {
		if (*pix_fmts == AV_PIX_FMT_D3D11) {
			AVBufferRef* hw_device_ref = (AVBufferRef*)avctx->opaque;
			AVHWFramesContext* frames_ctx = nullptr;
			AVD3D11VAFramesContext* frames_hwctx = nullptr;
			avctx->hw_frames_ctx = av_hwframe_ctx_alloc(hw_device_ref);
			if (!avctx->hw_frames_ctx) {
				return AV_PIX_FMT_NONE;
			}

			//AVHWDeviceContext* device_context = reinterpret_cast<AVHWDeviceContext*>(avctx->hw_frames_ctx->data);
			//AVD3D11VADeviceContext* d3d11va_device_context = reinterpret_cast<AVD3D11VADeviceContext*>(device_context->hwctx);

			frames_ctx = (AVHWFramesContext*)avctx->hw_frames_ctx->data;
			frames_hwctx = (AVD3D11VAFramesContext*)frames_ctx->hwctx;

			frames_ctx->format = AV_PIX_FMT_D3D11;
			frames_ctx->sw_format = AV_PIX_FMT_NV12;
			frames_ctx->width = FFALIGN(avctx->coded_width, 32);
			frames_ctx->height = FFALIGN(avctx->coded_height, 32);
			frames_ctx->initial_pool_size = 10;

			frames_hwctx->BindFlags |= D3D11_BIND_DECODER;
			frames_hwctx->MiscFlags |= D3D11_RESOURCE_MISC_SHARED;
			//frames_hwctx->BindFlags D3D11_BIND_SHADER_RESOURCE;

			int ret = av_hwframe_ctx_init(avctx->hw_frames_ctx);
			if (ret < 0) {
				return AV_PIX_FMT_NONE;
			}

			return AV_PIX_FMT_D3D11;
		}

		pix_fmts++;
	}

	printf("[D3D11VADecoder]  Failed to get HW surface format. \n");
	return AV_PIX_FMT_NONE;
}

D3D11VADecoder::D3D11VADecoder(ID3D11Device* d3d11_device)
	: d3d11_device_(d3d11_device)
{
	av_packet_ = av_packet_alloc();
}

D3D11VADecoder::~D3D11VADecoder()
{
	Destroy();
	av_packet_free(&av_packet_);
}


bool D3D11VADecoder::Init()
{
	if (codec_context_ != nullptr) {
		printf("[D3D11VADecoder] codec was opened. \n");
		return false;
	}

	AVCodec* codec = avcodec_find_decoder((AVCodecID)dec_type_);
	if (!codec) {
		printf("[D3D11VADecoder] Decoder(%s) not found. \n", avcodec_get_name((AVCodecID)dec_type_));
		return false;
	}

	AVHWDeviceContext* device_context = nullptr;
	AVD3D11VADeviceContext* d3d11_device_context = nullptr;
	AVHWDeviceType hw_type = AV_HWDEVICE_TYPE_D3D11VA;

	codec_context_ = avcodec_alloc_context3(codec);

	codec_context_->flags |= AV_CODEC_FLAG_LOW_DELAY;

	// Allow display of corrupt frames and frames missing references
	codec_context_->flags |= AV_CODEC_FLAG_OUTPUT_CORRUPT;
	codec_context_->flags2 |= AV_CODEC_FLAG2_SHOW_ALL;

	codec_context_->err_recognition = AV_EF_EXPLODE;
	codec_context_->codec_type = AVMEDIA_TYPE_VIDEO;
	codec_context_->width = dec_width_;
	codec_context_->height = dec_height_;

	for (int i = 0;; i++) {
		const AVCodecHWConfig* config = avcodec_get_hw_config(codec, i);
		if (!config) {
			printf("[D3D11VADecoder] Decoder %s does not support device type %s. \n",
				codec->name, av_hwdevice_get_type_name(hw_type));
			goto failed;
		}
		if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
			config->device_type == hw_type) {
			break;
		}
	}

	if (d3d11_device_) {
		device_buffer_ = av_hwdevice_ctx_alloc(hw_type);
		device_context = (AVHWDeviceContext*)device_buffer_->data;
		d3d11_device_context = (AVD3D11VADeviceContext*)device_context->hwctx;

		d3d11_device_context->device = (ID3D11Device*)d3d11_device_;
		d3d11_device_context->device->AddRef();
		av_hwdevice_ctx_init(device_buffer_);

		codec_context_->hw_device_ctx = av_buffer_ref(device_buffer_);
		codec_context_->opaque = device_buffer_;
	}
	else {
		 av_hwdevice_ctx_create(&device_buffer_, hw_type, NULL, NULL, 0);
		 codec_context_->hw_device_ctx = av_buffer_ref(device_buffer_);
	}

	codec_context_->get_format = get_d3d11va_hw_format;
	codec_context_->thread_count = 1;

	if (avcodec_open2(codec_context_, codec, NULL) != 0) {
		printf("[D3D11VADecoder] Open d3d11va decoder failed. \n");
		goto failed;
	}

	return true;

failed:
	if (codec_context_) {
		avcodec_free_context(&codec_context_);
		codec_context_ = nullptr;
	}

	return false;
}

void D3D11VADecoder::Destroy()
{
	if (codec_context_ != nullptr) {
		avcodec_close(codec_context_);
		avcodec_free_context(&codec_context_);
		codec_context_ = nullptr;
	}

	if (device_buffer_) {
		av_buffer_unref(&device_buffer_);
		device_buffer_ = nullptr;
	}
}

int D3D11VADecoder::Send(std::vector<uint8_t>& frame)
{
	std::lock_guard<std::mutex> locker(mutex_);

	if (codec_context_ == nullptr) {
		return -1;
	}

	av_packet_->data = &frame[0];
	av_packet_->size = frame.size();
	int ret = avcodec_send_packet(codec_context_, av_packet_);
	return ret;
}

int D3D11VADecoder::Recv(std::shared_ptr<AVFrame>& frame)
{
	std::lock_guard<std::mutex> locker(mutex_);

	int ret = -1;

	if (codec_context_ == nullptr) {
		return ret;
	}

	frame.reset(av_frame_alloc(), [](AVFrame* frame) { av_frame_free(&frame); });

	switch (codec_context_->codec_type)
	{
	case AVMEDIA_TYPE_VIDEO:
		ret = avcodec_receive_frame(codec_context_, frame.get());
		if (ret >= 0) {
			frame->pts = frame->best_effort_timestamp;
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

