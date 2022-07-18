#include "d3d11_renderer.h"

extern "C" {
#include "libavformat/avformat.h"
}

class D3D11VARenderer : public DX::D3D11Renderer
{
public:
	D3D11VARenderer();
	virtual ~D3D11VARenderer();

	virtual void RenderFrame(AVFrame* frame, int videoWidth, int videoHeight);

private:

};
