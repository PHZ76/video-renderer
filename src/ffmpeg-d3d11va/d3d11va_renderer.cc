#include "d3d11va_renderer.h"

D3D11VARenderer::D3D11VARenderer()
{

}

D3D11VARenderer::~D3D11VARenderer()
{

}

void D3D11VARenderer::RenderFrame(AVFrame* frame, int videoWidth, int videoHeight)
{
	std::lock_guard<std::mutex> locker(mutex_);

	if (!d3d11_context_) {
		return;
	}

	ID3D11Texture2D* texture = (ID3D11Texture2D*)frame->data[0];
	int index = (int)frame->data[1];

	D3D11_TEXTURE2D_DESC desc;
	texture->GetDesc(&desc);

	if (pixel_format_ != DX::PIXEL_FORMAT_NV12 ||
		width_ != videoWidth || height_ != videoHeight) {
		if (!CreateTexture(videoWidth, videoHeight, DX::PIXEL_FORMAT_NV12)) {
			return;
		}
	}

	Begin();

	ID3D11Texture2D* nv12_texture = input_textures_[DX::PIXEL_PLANE_NV12]->GetTexture();
	ID3D11ShaderResourceView* nv12_texture_y_srv = input_textures_[DX::PIXEL_PLANE_NV12]->GetNV12YShaderResourceView();
	ID3D11ShaderResourceView* nv12_texture_uv_srv = input_textures_[DX::PIXEL_PLANE_NV12]->GetNV12UVShaderResourceView();

	d3d11_context_->CopySubresourceRegion(
		nv12_texture,
		0,
		0,
		0,
		0,
		(ID3D11Resource*)texture,
		index,
		NULL);

	DX::D3D11RenderTexture* render_target = render_targets_[DX::PIXEL_SHADER_NV12_BT601].get();
	if (render_target) {
		// 保持视频比例
		RECT rect;
		GetClientRect(wnd_, &rect);
		render_target->ResetCameraMatrix();
		render_target->UpdateScaling(width_, height_, rect.right, rect.bottom, 90);

		render_target->Begin();
		render_target->PSSetTexture(0, nv12_texture_y_srv);
		render_target->PSSetTexture(1, nv12_texture_uv_srv);
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
