#pragma once

#include "mfxvideo++.h"
#include <cstdint>
#include <memory>

enum QSVEncoderOption
{
	QSV_ENCODER_OPTION_UNKNOW,

	QSV_ENCODER_OPTION_WIDTH,
	QSV_ENCODER_OPTION_HEIGHT,
	QSV_ENCODER_OPTION_CODEC,
	QSV_ENCODER_OPTION_BITRATE_KBPS,
	QSV_ENCODER_OPTION_FRAME_RATE,
	QSV_ENCODER_OPTION_GOP,

	QSV_ENCODER_OPTION_FORCE_IDR,
};

class QSVEncoder
{
public:
	QSVEncoder()
		: mfx_impl_(MFX_IMPL_AUTO_ANY)
		, mfx_ver_({ {0, 1} })
		, sps_buffer_(new mfxU8[1024])
		, pps_buffer_(new mfxU8[1024])
	{

	}

	virtual~QSVEncoder() {}
	QSVEncoder& operator=(const QSVEncoder&) = delete;
	QSVEncoder(const QSVEncoder&) = delete;


	static bool IsSupported()
	{
		mfxVersion             mfx_ver;
		MFXVideoSession        mfx_session;
		mfxIMPL                mfx_impl;

		mfx_impl = MFX_IMPL_AUTO_ANY;
		mfx_ver = { {0, 1} };

		mfxStatus sts = MFX_ERR_NONE;
		sts = mfx_session.Init(mfx_impl, &mfx_ver);
		mfx_session.Close();
		return sts == MFX_ERR_NONE;
	}

	void SetOption(QSVEncoderOption optopn, int value)
	{
		switch (optopn)
		{
		case QSV_ENCODER_OPTION_WIDTH:
			enc_width_ = value;
			break;
		case QSV_ENCODER_OPTION_HEIGHT:
			enc_height_ = value;
			break;
		case QSV_ENCODER_OPTION_CODEC:
			enc_type_ = value;
			break;
		case QSV_ENCODER_OPTION_BITRATE_KBPS:
			enc_bitrate_kbps_ = value;
			break;
		case QSV_ENCODER_OPTION_FRAME_RATE:
			enc_framerate_ = value;
			break;
		case QSV_ENCODER_OPTION_GOP:
			enc_gop_ = value;
			break;

		case QSV_ENCODER_OPTION_FORCE_IDR:
			force_idr_ += 1;
			break;

		default:
			break;
		}
	}


protected:
	mfxIMPL     mfx_impl_;
	mfxVersion  mfx_ver_;

	int enc_width_         = 1280;
	int enc_height_        = 720;
	int enc_type_          = 264;
	int enc_bitrate_kbps_  = 8000;
	int enc_framerate_     = 30;
	int enc_gop_           = 300;

	int force_idr_         = 0;

	std::unique_ptr<mfxU8> sps_buffer_;
	std::unique_ptr<mfxU8> pps_buffer_;
	mfxU16 sps_size_ = 0;
	mfxU16 pps_size_ = 0;
};