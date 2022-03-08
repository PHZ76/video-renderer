#pragma once

#include "d3d11_render_texture.h"
#include <memory>

namespace xop {

class D3D11RGBToYUVConverter
{
public:
	D3D11RGBToYUVConverter(ID3D11Device* d3d11_device);
	virtual ~D3D11RGBToYUVConverter();

	bool Init(int width, int height);
	void Destroy();

	bool Convert(ID3D11Texture2D* rgba_texture);

	ID3D11Texture2D* GetYUV420Texture();
	ID3D11Texture2D* GetChroma420Texture();

private:
	bool CreateTexture(int width, int height);
	bool CreateSampler();
	bool CreateBuffer();

	int width_ = 0;
	int height_ = 0;

	ID3D11Device* d3d11_device_ = NULL;
	ID3D11DeviceContext* d3d11_context_ = NULL;
	ID3D11SamplerState* point_sampler_ = NULL;
	ID3D11Buffer* buffer_ = NULL;

	std::unique_ptr<D3D11RenderTexture> yuv420_texture_;
	std::unique_ptr<D3D11RenderTexture> chroma420_texture_;
};

}