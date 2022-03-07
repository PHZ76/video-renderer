#pragma once

#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <directxcolors.h>

namespace xop {

class D3D11RenderTexture
{
public:
	D3D11RenderTexture(IDXGISwapChain* swap_chain);
	virtual ~D3D11RenderTexture();

	bool InitTexture(UINT width, UINT height, DXGI_FORMAT format, D3D11_USAGE usage, UINT bind_flags, UINT cpu_flags, UINT misc_flags);
	bool InitVertexShader();
	bool InitPixelShader(CONST WCHAR* pathname, const BYTE* pixel_shader = NULL, size_t pixel_shader_size = 0);
	bool InitRasterizerState();

	void Cleanup();

	void Begin();
	void PSSetTexture(UINT slot, ID3D11ShaderResourceView* shader_resource_view);
	void PSSetConstant(UINT slot, ID3D11Buffer* buffer);
	void PSSetSamplers(UINT slot, ID3D11SamplerState* sampler);
	void Draw();
	void End();

	ID3D11Texture2D* GetTexture();

	ID3D11RenderTargetView* GetRenderTargetView();
	ID3D11ShaderResourceView* GetShaderResourceView();

	ID3D11RenderTargetView* GetNV12YRenderTargetView();
	ID3D11RenderTargetView* GetNV12UVRenderTargetView();
	ID3D11ShaderResourceView* GetNV12YShaderResourceView();
	ID3D11ShaderResourceView* GetNV12UVShaderResourceView();

private:
	ID3D11Device*               d3d11_device_     = NULL;
	IDXGISwapChain*             dxgi_swap_chain_  = NULL;
	ID3D11DeviceContext*        d3d11_context_    = NULL;

	ID3D11Texture2D*            texture_          = NULL;
	ID3D11ShaderResourceView*   texture_srv_      = NULL;
	ID3D11RenderTargetView*     texture_rtv_      = NULL;

	ID3D11VertexShader*         vertex_shader_    = NULL;
	ID3D11InputLayout*          vertex_layout_    = NULL;
	ID3D11Buffer*               vertex_constants_ = NULL;
	ID3D11Buffer*               vertex_buffer_    = NULL;
	ID3D11PixelShader*          pixel_shader_     = NULL;
	ID3D11RasterizerState*      rasterizer_state_ = NULL;

	ID3D11RenderTargetView*     cache_rtv_        = NULL;
	ID3D11DepthStencilView*     cache_dsv_        = NULL;

	ID3D11RenderTargetView*		nv12_y_rtv_       = NULL;
	ID3D11RenderTargetView*     nv12_uv_rtv_      = NULL;
	ID3D11ShaderResourceView*   nv12_y_srv_       = NULL;
	ID3D11ShaderResourceView*   nv12_uv_srv_      = NULL;
};

}
