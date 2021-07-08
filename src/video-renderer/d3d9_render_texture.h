#pragma once

#include <windows.h>
#include <d3d9.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <memory>
#include <cstdio>

namespace xop {

class D3D9RenderTexture
{
public:
	D3D9RenderTexture(IDirect3DDevice9* device);
	virtual ~D3D9RenderTexture();

	// init
	bool InitTexture(UINT width, UINT height, UINT levels, DWORD usage, D3DFORMAT format, D3DPOOL pool);
	bool InitSurface(UINT width, UINT height, D3DFORMAT format, D3DPOOL pool);
	bool InitVertexShader();
	bool InitPixelShader(CONST WCHAR* pathname, const BYTE* pixel_shader=NULL, size_t pixel_shader_size=0);

	// render
	void Begin();
	void SetTexture(DWORD stage, IDirect3DBaseTexture9* texture);
	void SetConstant(UINT start_register, CONST float* pixel_shader_constant, UINT count);
	void Draw();
	void End();

	// release
	void Cleanup();

	IDirect3DTexture9* GetTexture();
	IDirect3DSurface9* GetSurface();

private:
	IDirect3DDevice9*  d3d9_device_    = NULL;
	IDirect3DTexture9* render_texture_ = NULL;
	IDirect3DSurface9* render_surface_ = NULL;
	IDirect3DSurface9* cache_surface_  = NULL;

	IDirect3DVertexBuffer9* vertex_buffer_ = NULL;
	IDirect3DPixelShader9*  pixel_shader_  = NULL;
};

}
