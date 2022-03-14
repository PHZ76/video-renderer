#pragma once

#include <vector>
#include <mutex>
#include <memory>
#include <d3d11.h>
#include "av_decoder.h"

class D3D11VADecoder : public AVDecoder
{
public:
	D3D11VADecoder(ID3D11Device* d3d11_device);
	virtual ~D3D11VADecoder();

	virtual bool Init();
	virtual void Destroy();

	virtual int  Send(std::vector<uint8_t>& frame);
	virtual int  Recv(std::shared_ptr<AVFrame>& frame);

private:
	std::mutex mutex_;

	ID3D11Device* d3d11_device_ = nullptr;

	AVPacket* av_packet_ = nullptr;
	AVCodecContext* codec_context_ = nullptr;

	AVBufferRef* device_buffer_ = nullptr;
};

