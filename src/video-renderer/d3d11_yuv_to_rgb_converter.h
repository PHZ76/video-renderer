#pragma once

#include "d3d11_render_texture.h"
#include <memory>

namespace xop {

class D3D11YUVToRGBConverter
{
public:
	D3D11YUVToRGBConverter(IDXGISwapChain* swap_chain);
	virtual ~D3D11YUVToRGBConverter();

	bool Init(int width, int height);
	void Destroy();

	bool Combine(ID3D11Texture2D* yuv420_texture, ID3D11Texture2D* chroma420_texture);

	ID3D11Texture2D* GetRGBATexture();

private:
	bool CreateTexture(int width, int height);
	bool CreateSampler();
	bool CreateBuffer();

	int width_ = 0;
	int height_ = 0;

	IDXGISwapChain* swap_chain_ = NULL;
	ID3D11Device* d3d11_device_ = NULL;
	ID3D11DeviceContext* d3d11_context_ = NULL;
	ID3D11SamplerState* point_sampler_ = NULL;
	ID3D11Buffer* buffer_ = NULL;

	std::unique_ptr<D3D11RenderTexture> rgba_texture_;
};

}