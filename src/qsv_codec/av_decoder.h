#pragma once


#include <cstdint>
#include <memory>

extern "C" {
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
}

enum AVDecoderOption
{
	AV_DECODER_OPTION_UNKNOW,

	AV_DECODER_OPTION_WIDTH,
	AV_DECODER_OPTION_HEIGHT,
	AV_DECODER_OPTION_CODEC,
};

class AVDecoder
{
public:
	AVDecoder()
	{

	}

	virtual~AVDecoder() {}
	AVDecoder& operator=(const AVDecoder&) = delete;
	AVDecoder(const AVDecoder&) = delete;

	void SetOption(AVDecoderOption optopn, int value)
	{
		switch (optopn)
		{
		case AV_DECODER_OPTION_WIDTH:
			dec_width_ = value;
			break;
		case AV_DECODER_OPTION_HEIGHT:
			dec_height_ = value;
			break;
		case AV_DECODER_OPTION_CODEC:
			dec_type_ = value;
			break;

		default:
			break;
		}
	}


protected:

	int dec_width_  = 1280;
	int dec_height_ = 720;
	int dec_type_   = AV_CODEC_ID_H264;
};