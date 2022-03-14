#pragma once

#include "d3d11_renderer.h"
#include "screen_capture.h"

extern "C" {
#include "libavformat/avformat.h"
}

class VideoSink : public DX::D3D11Renderer
{
public:
	VideoSink();
	virtual ~VideoSink();

	bool Init();
	void Destroy();



private:

};
