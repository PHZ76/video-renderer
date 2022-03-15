#pragma once

#include "d3d11_renderer.h"
#include "screen_capture.h"
#include "d3d11va_decoder.h"

extern "C" {
#include "libavformat/avformat.h"
}

class VideoSink : public DX::D3D11Renderer
{
public:
	VideoSink();
	virtual ~VideoSink();

	virtual bool Init(HWND hwnd, int width, int height);
	virtual void Destroy();

	virtual void RenderFrame(DX::Image image);
	virtual void RenderFrame(std::vector<uint8_t> yuv420_frame);
	virtual void RenderFrame(std::vector<uint8_t> yuv420_frame, std::vector<uint8_t> chroma420_frame);

private:
	std::shared_ptr<D3D11VADecoder> yuv420_decoder_;
	std::shared_ptr<D3D11VADecoder> chroma420_decoder_;

	int width_ = 0;
	int height_ = 0;
};
