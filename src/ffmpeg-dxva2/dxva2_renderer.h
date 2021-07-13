#include "d3d9_renderer.h"

extern "C" {
#include "libavformat/avformat.h"
}

class DXVA2Renderer : public xop::D3D9Renderer
{
public:
	DXVA2Renderer();
	virtual ~DXVA2Renderer();

	virtual void RenderFrame(AVFrame* frame);

private:

};
