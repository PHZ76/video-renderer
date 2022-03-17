#pragma once

#include "d3d11_renderer.h"
#include "screen_capture.h"
#include "d3d11va_decoder.h"
#include "d3d11_yuv_to_rgb_converter.h"
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

	virtual void RenderFrame(DX::Image& image);
	virtual void RenderNV12(std::vector<std::vector<uint8_t>>& compressed_frame);
	virtual void RenderARGB(std::vector<std::vector<uint8_t>>& compressed_frame);

private:
	virtual void End();

	std::shared_ptr<D3D11VADecoder> yuv420_decoder_;
	std::shared_ptr<D3D11VADecoder> chroma420_decoder_;
	std::shared_ptr<DX::D3D11YUVToRGBConverter> color_converter_;
};
