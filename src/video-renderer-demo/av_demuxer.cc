#include "av_demuxer.h"
#include "av_log.h"

static int is_realtime(AVFormatContext* s)
{
	if (!strcmp(s->iformat->name, "rtp")
		|| !strcmp(s->iformat->name, "rtsp")
		|| !strcmp(s->iformat->name, "sdp"))
		return 1;

	if (s->pb && (!strncmp(s->url, "rtp:", 4)
		|| !strncmp(s->url, "udp:", 4)))
		return 1;
	return 0;
}

AVDemuxer::AVDemuxer()
{
	memset(st_index_, -1, sizeof(st_index_));
}

AVDemuxer::~AVDemuxer()
{
	Close();
}

static int demux_interrupt_cb(void* opaque)
{
	AVDemuxer* demuxer = (AVDemuxer*)opaque;
	return demuxer->IsOpened() ? 0 : 1;
}

bool AVDemuxer::Open(std::string url)
{
	std::lock_guard<std::mutex> locker(mutex_);

	if (format_context_ != nullptr) {
		LOG("AVDemuxer was opened.");
		return false;
	}

	AVDictionary* options = nullptr;
	//av_dict_set(&options, "buffer_size", "1024000", 0);
	//av_dict_set(&options, "max_delay", "0", 0);
	///av_dict_set(&options, "stimeout", "20000000", 0);
	//av_dict_set(&options, "rtsp_transport", "tcp", 0);
	av_dict_set(&options, "scan_all_pmts", "1", AV_DICT_DONT_OVERWRITE);

	format_context_ = avformat_alloc_context();
	if (!format_context_) {
		LOG("Could not allocate context.");
		return false;
	}

	format_context_->interrupt_callback.callback = demux_interrupt_cb;
	format_context_->interrupt_callback.opaque = this;
	is_opened_ = true;

	int ret = avformat_open_input(&format_context_, url.c_str(), 0, &options);
	if (ret != 0) {
		AV_LOG(ret, "open %s failed.", url.c_str());
		avformat_free_context(format_context_);
		is_opened_ = false;
		return false;
	}

	if (genpts_) {
		format_context_->flags |= AVFMT_FLAG_GENPTS;
	}

	av_format_inject_global_side_data(format_context_);

	if (format_context_->pb) {
		// FIXME hack, ffplay maybe should not use avio_feof() to test for the end
		format_context_->pb->eof_reached = 0;
	}

	is_realtime_ = is_realtime(format_context_);
	max_frame_duration_ = (format_context_->iformat->flags & AVFMT_TS_DISCONT) ? 10.0 : 3600.0;

	ret = avformat_find_stream_info(format_context_, 0);
	if (ret < 0) {
		AV_LOG(ret, "find stream info failed.");
		avformat_close_input(&format_context_);
		avformat_free_context(format_context_);
		return false;
	}

	if (format_context_->pb) {
		format_context_->pb->eof_reached = 0; // FIXME hack, ffplay maybe should not use avio_feof() to test for the end
	}

	st_index_[AVMEDIA_TYPE_VIDEO] = av_find_best_stream(format_context_, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
	if (st_index_[AVMEDIA_TYPE_VIDEO] >= 0) {
		video_stream_ = format_context_->streams[st_index_[AVMEDIA_TYPE_VIDEO]];
	}

	st_index_[AVMEDIA_TYPE_AUDIO] = av_find_best_stream(format_context_, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
	if (st_index_[AVMEDIA_TYPE_AUDIO] >= 0) {
		audio_stream_ = format_context_->streams[st_index_[AVMEDIA_TYPE_AUDIO]];
	}

	if (video_stream_) {
		st_index_[AVMEDIA_TYPE_SUBTITLE] = av_find_best_stream(format_context_, AVMEDIA_TYPE_SUBTITLE, -1, -1, NULL, 0);
		if (st_index_[AVMEDIA_TYPE_SUBTITLE] >= 0) {
			subtitle_stream_ = format_context_->streams[st_index_[AVMEDIA_TYPE_SUBTITLE]];
		}
	}

	if (infinite_buffer_ < 0 && is_realtime_) {
		infinite_buffer_ = 1;
	}

	eof_ = 0;
	url_ = url;
	return true;
}

void AVDemuxer::Close()
{
	std::lock_guard<std::mutex> locker(mutex_);

	is_opened_ = false;

	if (format_context_ != nullptr) {
		avformat_close_input(&format_context_);
		avformat_free_context(format_context_);
		format_context_ = nullptr;
	}

	if (options_) {
		av_dict_free(&options_);
		options_ = nullptr;
	}

	video_stream_ = nullptr;
	audio_stream_ = nullptr;
	subtitle_stream_ = nullptr;
	eof_ = 0;
	memset(st_index_, -1, sizeof(st_index_));
}

bool AVDemuxer::IsOpened()
{
	return is_opened_;
}

int AVDemuxer::Read(AVPacket* pkt)
{
	std::lock_guard<std::mutex> locker(mutex_);

	if (!format_context_) {
		return -1;
	}

	int ret = av_read_frame(format_context_, pkt);
	if (ret < 0) {
		if ((ret == AVERROR_EOF || avio_feof(format_context_->pb)) && !eof_) {
			eof_ = 1;
			return -1;
		}

		if (format_context_->pb && format_context_->pb->error) {
			return -2;
		}
	}
	else {
		eof_ = 0;
	}

	if (pkt->pts != AV_NOPTS_VALUE) {
		pkt->pts = (int64_t)(pkt->pts * (1000 * (av_q2d(format_context_->streams[pkt->stream_index]->time_base))));
		pkt->dts = (int64_t)(pkt->dts * (1000 * (av_q2d(format_context_->streams[pkt->stream_index]->time_base))));
	}
	else {
		pkt->pts = (int64_t)NAN;
		pkt->dts = (int64_t)NAN;
	}

	return 0;
}

bool AVDemuxer::IsEOF()
{
	return eof_ ? true : false;
}

AVFormatContext* AVDemuxer::GetFormatContext()
{
	std::lock_guard<std::mutex> locker(mutex_);
	return format_context_;
}

AVStream* AVDemuxer::GetVideoStream()
{
	std::lock_guard<std::mutex> locker(mutex_);
	return video_stream_;
}

AVStream* AVDemuxer::GetAudioStream()
{
	std::lock_guard<std::mutex> locker(mutex_);
	return audio_stream_;
}

AVStream* AVDemuxer::GetSubtitleStream()
{
	std::lock_guard<std::mutex> locker(mutex_);
	return subtitle_stream_;
}
