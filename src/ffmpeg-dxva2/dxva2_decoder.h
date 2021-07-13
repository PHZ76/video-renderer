#pragma once

#include <string>
#include <mutex>
#include <memory>

extern "C" {
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavutil/avutil.h"
#include "libavutil/hwcontext.h"
#include "libavutil/pixdesc.h"
}

class AVDecoder
{
public:
	AVDecoder& operator=(const AVDecoder&) = delete;
	AVDecoder(const AVDecoder&) = delete;
	AVDecoder();
	virtual ~AVDecoder();

	virtual bool Init(AVStream* stream, void* d3d9_device);
	virtual void Destroy();

	virtual int  Send(AVPacket* packet);
	virtual int  Recv(AVFrame* frame);

private:
	std::mutex mutex_;

	AVStream* stream_ = nullptr;
	AVCodecContext* codec_context_ = nullptr;
	AVDictionary* options_ = nullptr;
	AVBufferRef* device_buffer_ = nullptr;

	int decoder_reorder_pts_ = -1;

	int64_t next_pts_ = AV_NOPTS_VALUE;
	int64_t start_pts_ = AV_NOPTS_VALUE;
	int finished_ = -1;
	AVRational start_pts_tb_;
	AVRational next_pts_tb_;
};

