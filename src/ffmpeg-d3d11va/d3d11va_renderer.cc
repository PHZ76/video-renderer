#include "d3d11va_renderer.h"

D3D11VARenderer::D3D11VARenderer()
{

}

D3D11VARenderer::~D3D11VARenderer()
{

}

void D3D11VARenderer::RenderFrame(AVFrame* frame)
{
	std::lock_guard<std::mutex> locker(mutex_);

	if (!d3d11_context_) {
		return;
	}

	ID3D11Texture2D* texture = (ID3D11Texture2D*)frame->data[0];
	int index = (int)frame->data[1];

	D3D11_TEXTURE2D_DESC desc;
	texture->GetDesc(&desc);

	if (pixel_format_ != xop::PIXEL_FORMAT_NV12 ||
		width_!=desc.Width ||height_!=desc.Height) {
		if (!CreateTexture(desc.Width, desc.Height, xop::PIXEL_FORMAT_NV12)) {
			return;
		}
	}

	Begin();

	ID3D11Texture2D* nv12_texture = input_texture_[xop::PIXEL_PLANE_NV12]->GetTexture();
	ID3D11ShaderResourceView* luminance_view = input_texture_[xop::PIXEL_PLANE_NV12]->GetLuminanceView();
	ID3D11ShaderResourceView* chrominance_view = input_texture_[xop::PIXEL_PLANE_NV12]->GetChrominanceView();

	d3d11_context_->CopySubresourceRegion(
		nv12_texture,
		0,
		0,
		0,
		0,
		(ID3D11Resource*)texture,
		index,
		NULL);

	xop::D3D11RenderTexture* render_target = render_target_[xop::PIXEL_SHADER_NV12_BT601].get();
	if (render_target) {
		render_target->Begin();
		render_target->PSSetTexture(0, luminance_view);
		render_target->PSSetTexture(1, chrominance_view);
		render_target->PSSetSamplers(0, linear_sampler_);
		render_target->PSSetSamplers(1, point_sampler_);
		render_target->Draw();
		render_target->End();
		render_target->PSSetTexture(0, NULL);
		render_target->PSSetTexture(1, NULL);
		output_texture_ = render_target;
	}

	Process();
	End();
}
