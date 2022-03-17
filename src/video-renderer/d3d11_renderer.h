#pragma once

#include "renderer.h"
#include "d3d11_render_texture.h"
#include <mutex>

namespace DX {

class D3D11Renderer : public Renderer
{
public:
	D3D11Renderer();
	virtual ~D3D11Renderer();

	virtual bool Init(HWND hwnd);
	virtual void Destroy();

	virtual bool Resize();

	virtual void Render(PixelFrame* frame);

	IDXGISwapChain* GetDXGISwapChain();
	ID3D11Device*   GetD3D11Device();

	// sharpness: 0.0 to 10.0
	virtual void SetSharpen(float unsharp);

protected:
	bool InitDevice();
	bool CreateRenderer();
	bool CreateTexture(int width, int height, PixelFormat format);
	virtual void Begin();
	virtual void Copy(PixelFrame* frame);
	virtual void Process();
	virtual void End();

	void UpdateARGB(PixelFrame* frame);
	void UpdateI444(PixelFrame* frame);
	void UpdateI420(PixelFrame* frame);
	void UpdateNV12(PixelFrame* frame);

	std::mutex mutex_;

	HWND wnd_ = NULL;
	D3D_DRIVER_TYPE   driver_type_   = D3D_DRIVER_TYPE_UNKNOWN;
	D3D_FEATURE_LEVEL feature_level_ = D3D_FEATURE_LEVEL_11_0;

	ID3D11Device*        d3d11_device_      = NULL;
	IDXGISwapChain*      dxgi_swap_chain_   = NULL;
	ID3D11DeviceContext* d3d11_context_     = NULL;

	PixelFormat pixel_format_ = PIXEL_FORMAT_UNKNOW;
	int width_  = 0;
	int height_ = 0;

	ID3D11RenderTargetView* main_render_target_view_ = NULL;
	ID3D11SamplerState* point_sampler_  = NULL;
	ID3D11SamplerState* linear_sampler_ = NULL;

	D3D11RenderTexture* output_texture_ = NULL;
	std::shared_ptr<D3D11RenderTexture> input_textures_[PIXEL_PLANE_MAX];
	std::shared_ptr<D3D11RenderTexture> render_targets_[PIXEL_SHADER_MAX];

	float unsharp_ = 0.0;
	ID3D11Buffer* sharpen_constants_ = NULL;
};

}
