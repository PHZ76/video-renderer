#include "d3d9_render_texture.h"
#include "log.h"
#include <string>

using namespace xop;

#define DX_SAFE_RELEASE(p) { if(p) { (p)->Release(); (p)=NULL; } } 

static const DWORD FVF = D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1;

typedef struct
{
	FLOAT x, y, z;
	D3DCOLOR color;
	FLOAT u, v;
} Vertex;

static HRESULT CompileShaderFromFile(const WCHAR* file_name, LPCSTR entry_point, LPCSTR shader_model, ID3DBlob** blob_out)
{
	HRESULT hr = S_OK;

	DWORD shader_flags = D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY;
#ifdef _DEBUG
	// Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
	// Setting this flag improves the shader debugging experience, but still allows 
	// the shaders to be optimized and to run exactly the way they will run in 
	// the release configuration of this program.
	shader_flags |= D3DCOMPILE_DEBUG;

	// Disable optimizations to further improve shader debugging
	shader_flags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	ID3DBlob* error_blob = nullptr;
	hr = D3DCompileFromFile(file_name, nullptr, nullptr, entry_point, shader_model,
		shader_flags, 0, blob_out, &error_blob);
	if (FAILED(hr)) {
		if (error_blob) {
			OutputDebugStringA(reinterpret_cast<const char*>(error_blob->GetBufferPointer()));
			error_blob->Release();
		}
		LOG("D3DCompileFromFile() failed, %x", hr);
		return hr;
	}

	if (error_blob) {
		error_blob->Release();
	}
	return hr;
}

D3D9RenderTexture::D3D9RenderTexture(IDirect3DDevice9* d3d9_device)
	: d3d9_device_(d3d9_device)
{

}

D3D9RenderTexture::~D3D9RenderTexture()
{
	Cleanup();
}

bool D3D9RenderTexture::InitTexture(UINT width, UINT height, UINT levels, DWORD usage, D3DFORMAT format, D3DPOOL pool)
{
	if (!d3d9_device_) {
		return false;
	}

	DX_SAFE_RELEASE(vertex_buffer_);
	DX_SAFE_RELEASE(render_surface_);
	DX_SAFE_RELEASE(render_texture_);

	HRESULT hr = S_OK;
	Vertex* vertex = NULL;
	float w = static_cast<float>(width);
	float h = static_cast<float>(height);

	hr = d3d9_device_->CreateTexture(
		width,
		height,
		1, 
		usage,
		format,
		pool,
		&render_texture_,
		NULL);

	if (FAILED(hr)) {
		LOG("IDirect3DDevice9::CreateTexture() failed, %x", hr);
		goto failed;
	}

	hr = render_texture_->GetSurfaceLevel(0, &render_surface_);
	if (FAILED(hr)) {
		LOG("IDirect3DTexture9::GetSurfaceLevel() failed, %x", hr);
		goto failed;
	}

	hr = d3d9_device_->CreateVertexBuffer(4 * sizeof(Vertex), 0, FVF, D3DPOOL_DEFAULT, &vertex_buffer_, NULL);
	if (FAILED(hr)) {
		LOG("IDirect3DDevice9::CreateVertexBuffer() failed, %x", hr);
		goto failed;
	}

	vertex_buffer_->Lock(0, 4 * sizeof(Vertex), (void**)&vertex, 0);
	vertex[0] = { 0, 0, 0.0f, D3DCOLOR_ARGB(255, 255, 255, 255), 0.0f, 0.0f };
	vertex[1] = { w, 0, 0.0f, D3DCOLOR_ARGB(255, 255, 255, 255), 1.0f, 0.0f };
	vertex[2] = { w, h, 0.0f, D3DCOLOR_ARGB(255, 255, 255, 255), 1.0f, 1.0f };
	vertex[3] = { 0, h, 0.0f, D3DCOLOR_ARGB(255, 255, 255, 255), 0.0f, 1.0f };
	vertex_buffer_->Unlock();

	return true;

failed:
	DX_SAFE_RELEASE(vertex_buffer_);
	DX_SAFE_RELEASE(render_surface_);
	DX_SAFE_RELEASE(render_texture_);
	return false;
}

bool D3D9RenderTexture::InitTexture(IDirect3DSurface9* render_target)
{
	if (!d3d9_device_) {
		return false;
	}

	HRESULT hr = S_OK;
	Vertex* vertex = NULL;
	D3DSURFACE_DESC dsec;

	hr = render_target->GetDesc(&dsec);
	if (FAILED(hr)) {
		LOG("IDirect3DSurface9::GetDesc() failed, %x", hr);
		return false;
	}

	float w = static_cast<float>(dsec.Width);
	float h = static_cast<float>(dsec.Height);

	hr = d3d9_device_->CreateVertexBuffer(4 * sizeof(Vertex), 0, FVF, D3DPOOL_DEFAULT, &vertex_buffer_, NULL);
	if (FAILED(hr)) {
		LOG("IDirect3DDevice9::CreateVertexBuffer() failed, %x", hr);
		return false;
	}

	vertex_buffer_->Lock(0, 4 * sizeof(Vertex), (void**)&vertex, 0);
	vertex[0] = { 0, 0, 0.0f, D3DCOLOR_ARGB(255, 255, 255, 255), 0.0f, 0.0f };
	vertex[1] = { w, 0, 0.0f, D3DCOLOR_ARGB(255, 255, 255, 255), 1.0f, 0.0f };
	vertex[2] = { w, h, 0.0f, D3DCOLOR_ARGB(255, 255, 255, 255), 1.0f, 1.0f };
	vertex[3] = { 0, h, 0.0f, D3DCOLOR_ARGB(255, 255, 255, 255), 0.0f, 1.0f };
	vertex_buffer_->Unlock();

	render_surface_ = render_target;
	return true;
}

bool D3D9RenderTexture::InitSurface(UINT width, UINT height, D3DFORMAT format, D3DPOOL pool)
{
	if (!d3d9_device_) {
		return false;
	}

	HRESULT hr = d3d9_device_->CreateOffscreenPlainSurface(width, height, format, pool, &render_surface_, NULL);
	if (FAILED(hr)) {
		LOG("IDirect3DDevice9::CreateOffscreenPlainSurface() failed, %x", hr);
		return false;
	}

	return true;
}

bool D3D9RenderTexture::InitPixelShader(CONST WCHAR* pathname, const BYTE* pixel_shader, size_t pixel_shader_size)
{
	DX_SAFE_RELEASE(pixel_shader_);

	HRESULT hr = S_OK;
	size_t converted = 0;
	char str_pathname[100] = { 0 };
	if (pathname) {
		wcstombs_s(&converted, str_pathname, sizeof(str_pathname), pathname, sizeof(str_pathname));
	}
	
	if (pixel_shader != NULL && pixel_shader_size > 0) {
		hr = d3d9_device_->CreatePixelShader((DWORD*)pixel_shader, &pixel_shader_);
		if (FAILED(hr)) {
			LOG("IDirect3DDevice9::CreatePixelShader(%s) failed, %x", str_pathname, hr);
			return false;
		}
	}	
	else if (pathname != NULL) {
		ID3DBlob* blob = nullptr;
		hr = CompileShaderFromFile(pathname, "main", "ps_2_0", &blob);
		if (FAILED(hr)) {
			
			LOG("CompileShaderFromFile(%s) failed, %x", str_pathname, hr);
			return false;
		}

		hr = d3d9_device_->CreatePixelShader((DWORD*)blob->GetBufferPointer(), &pixel_shader_);
		DX_SAFE_RELEASE(blob);
		if (FAILED(hr)) {
			LOG("IDirect3DDevice9::CreatePixelShader(%s) failed, %x", str_pathname, hr);
			return false;
		}
	}

	return true;
}

void D3D9RenderTexture::Begin()
{
	if (!d3d9_device_ || !render_surface_) {
		return;
	}

	HRESULT hr = S_OK;

	DX_SAFE_RELEASE(cache_surface_);
	hr = d3d9_device_->GetRenderTarget(0, &cache_surface_);

	D3DSURFACE_DESC dsec;
	hr = render_surface_->GetDesc(&dsec);
	if (FAILED(hr)) {
		LOG("IDirect3DSurface9::GetDesc() failed, %x", hr);
		return;
	}

	D3DMATRIX d3dmatrix;
	memset(&d3dmatrix, 0, sizeof(D3DMATRIX));
	d3dmatrix.m[0][0] =  2.0f / dsec.Width;
	d3dmatrix.m[1][1] = -2.0f / dsec.Height;
	d3dmatrix.m[2][2] =  1.0f;
	d3dmatrix.m[3][0] = -1.0f;
	d3dmatrix.m[3][1] =  1.0f;
	d3dmatrix.m[3][3] =  1.0f;
	//D3DXMatrixOrthoOffCenterLH(&projection, 0.0f, frame->width, frame->height, 0.0f, 0.0f, 1.0f);
	d3d9_device_->SetTransform(D3DTS_PROJECTION, &d3dmatrix);

	d3d9_device_->SetRenderTarget(0, render_surface_);
	d3d9_device_->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);

	d3d9_device_->SetFVF(FVF);
	d3d9_device_->SetStreamSource(0, vertex_buffer_, 0, sizeof(Vertex));
	if (pixel_shader_) {
		d3d9_device_->SetPixelShader(pixel_shader_);
	}
}

void D3D9RenderTexture::SetTexture(DWORD stage, IDirect3DBaseTexture9* texture)
{
	if (!d3d9_device_ || !render_surface_) {
		return;
	}

	d3d9_device_->SetTexture(stage, texture);
}

void D3D9RenderTexture::SetConstant(UINT start_register, CONST float* constant, UINT count)
{
	if (!d3d9_device_ || !render_surface_) {
		return;
	}

	d3d9_device_->SetPixelShaderConstantF(start_register, constant, count);
}

void D3D9RenderTexture::Draw()
{
	if (!d3d9_device_ || !render_surface_) {
		return;
	}

	d3d9_device_->DrawPrimitive(D3DPT_TRIANGLEFAN, 0, 2);
}

void D3D9RenderTexture::End()
{
	if (!d3d9_device_ || !render_surface_) {
		return;
	}

	d3d9_device_->SetRenderTarget(0, cache_surface_);
	DX_SAFE_RELEASE(cache_surface_);
}

void D3D9RenderTexture::Cleanup()
{
	DX_SAFE_RELEASE(cache_surface_);
	DX_SAFE_RELEASE(pixel_shader_);
	DX_SAFE_RELEASE(vertex_buffer_);
	DX_SAFE_RELEASE(render_surface_);
	DX_SAFE_RELEASE(render_texture_);
}

IDirect3DTexture9* D3D9RenderTexture::GetTexture()
{
	return render_texture_;
}

IDirect3DSurface9* D3D9RenderTexture::GetSurface()
{
	return render_surface_;
}
