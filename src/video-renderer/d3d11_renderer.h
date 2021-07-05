#pragma once

#include "renderer.h"

namespace xop {

class D3D11Renderer : public Renderer
{
public:
	D3D11Renderer();
	virtual ~D3D11Renderer();

	virtual bool Init(HWND hwnd);
	virtual void Destroy();

	virtual void Resize();

	virtual void Render(PixelFrame* frame);

private:

};

}

