#pragma once

#include <string>
#include <mutex>
#include <memory>

extern "C" {
#include "libavutil/imgutils.h"
#include "libavformat/avformat.h"
}

class AVDemuxer
{
public:
	AVDemuxer& operator=(const AVDemuxer&) = delete;
	AVDemuxer(const AVDemuxer&) = delete;
	AVDemuxer();
	virtual ~AVDemuxer();

	virtual bool Open(std::string url);
	virtual void Close();
	virtual bool IsOpened();

	virtual int  Read(AVPacket* pkt);
	virtual bool IsEOF();

	AVFormatContext* GetFormatContext();
	AVStream* GetVideoStream();
	AVStream* GetAudioStream();
	AVStream* GetSubtitleStream();

private:
	std::mutex  mutex_;
	std::string url_;

	bool is_opened_ = false;

	AVFormatContext* format_context_ = nullptr;
	AVDictionary* options_ = nullptr;

	int st_index_[AVMEDIA_TYPE_NB];
	AVStream* video_stream_ = nullptr;
	AVStream* audio_stream_ = nullptr;
	AVStream* subtitle_stream_ = nullptr;

	int    is_realtime_ = 0;
	int    genpts_ = 0;
	int    infinite_buffer_ = -1;
	double max_frame_duration_ = 0.0; 
	int    eof_ = 0;

	uint64_t pts_[AVMEDIA_TYPE_NB];
};
