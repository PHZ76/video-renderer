#include "dxva2_renderer.h"

DXVA2Renderer::DXVA2Renderer()
{

}

DXVA2Renderer::~DXVA2Renderer()
{

}

void DXVA2Renderer::RenderFrame(AVFrame* frame)
{
	if (!d3d9_device_) {
		return;
	}

	LPDIRECT3DSURFACE9 surface = (LPDIRECT3DSURFACE9)frame->data[3];

	D3DSURFACE_DESC desc;
	surface->GetDesc(&desc);

	if (pixel_format_ != DX::PIXEL_FORMAT_NV12 ||
		width_ != desc.Width ||height_ != desc.Height) {
		if (!CreateTexture(desc.Width, desc.Height, DX::PIXEL_FORMAT_NV12)) {
			return;
		}
	}

	Begin();

	output_texture_ = input_texture_[DX::PIXEL_PLANE_ARGB].get();
	d3d9_device_->StretchRect(surface, NULL, output_texture_->GetSurface(), NULL, D3DTEXF_LINEAR);

	Process();
	End();
}
