#include "d3d11va_decoder.h"
#include "av_log.h"

//extern "C" {
#include "libavutil/hwcontext.h"
#include "libavutil/hwcontext_d3d11va.h"
//}

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

	LOG("Failed to get HW surface format.");
	return AV_PIX_FMT_NONE;
}

AVDecoder::AVDecoder()
{

}

AVDecoder::~AVDecoder()
{
	Destroy();
}


bool AVDecoder::Init(AVStream* stream, void* d3d11_device, bool hw)
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
	AVD3D11VADeviceContext* d3d11_device_context = nullptr;
	AVHWDeviceType hw_type = AV_HWDEVICE_TYPE_D3D11VA;

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

	if (hw)
	{
		if (d3d11_device) {
			device_buffer_ = av_hwdevice_ctx_alloc(hw_type);
			device_context = (AVHWDeviceContext*)device_buffer_->data;
			d3d11_device_context = (AVD3D11VADeviceContext*)device_context->hwctx;

			d3d11_device_context->device = (ID3D11Device*)d3d11_device;
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
		codec_context_->pkt_timebase = stream->time_base;
	}

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

