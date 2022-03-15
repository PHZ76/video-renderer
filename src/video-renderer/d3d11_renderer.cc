#include "d3d11_renderer.h"
#include "log.h"

#include "shader/d3d11/shader_d3d11_pixel.h"
#include "shader/d3d11/shader_d3d11_nv12_bt601.h"
#include "shader/d3d11/shader_d3d11_nv12_bt709.h"
#include "shader/d3d11/shader_d3d11_yuv_bt601.h"
#include "shader/d3d11/shader_d3d11_yuv_bt709.h"
#include "shader/d3d11/shader_d3d11_sharpen.h"

#define DX_SAFE_RELEASE(p) { if(p) { (p)->Release(); (p) = NULL; } } 

using namespace DX;

struct SharpenShaderConstants
{
	float width;
	float height;
	float unsharp;
	float align_;
};

D3D11Renderer::D3D11Renderer()
{

}

D3D11Renderer::~D3D11Renderer()
{
	Destroy();
}

bool D3D11Renderer::Init(HWND hwnd)
{
	std::lock_guard<std::mutex> locker(mutex_);

	wnd_ = hwnd;

	if (!InitDevice()) {
		return false;
	}

	if (!CreateRenderer()) {
		DX_SAFE_RELEASE(dxgi_swap_chain_);
		DX_SAFE_RELEASE(d3d11_context_);
		DX_SAFE_RELEASE(d3d11_device_);
		return false;
	}

	return true;
}

void D3D11Renderer::Destroy()
{
	std::lock_guard<std::mutex> locker(mutex_);

	for (int i = 0; i < PIXEL_PLANE_MAX; i++) {
		input_textures_[i].reset();
	}

	for (int i = 0; i < PIXEL_SHADER_MAX; i++) {
		render_targets_[i].reset();
	}

	if (d3d11_context_) {
		d3d11_context_->ClearState();
	}

	DX_SAFE_RELEASE(sharpen_constants_);
	DX_SAFE_RELEASE(point_sampler_);
	DX_SAFE_RELEASE(linear_sampler_);
	DX_SAFE_RELEASE(main_render_target_view_);
	DX_SAFE_RELEASE(dxgi_swap_chain_);
	DX_SAFE_RELEASE(d3d11_context_);
	DX_SAFE_RELEASE(d3d11_device_);

	output_texture_ = NULL;
	pixel_format_ = PIXEL_FORMAT_UNKNOW;
}

bool D3D11Renderer::Resize()
{
	std::lock_guard<std::mutex> locker(mutex_);

	if (!dxgi_swap_chain_) {
		return false;
	}

	RECT rect;
	if (!GetClientRect(wnd_, &rect)) {
		return false;
	}

	UINT width = static_cast<UINT>(rect.right - rect.left);
	UINT height = static_cast<UINT>(rect.bottom - rect.top);

	if (width == 0 && height == 0) {
		return false;
	}

	d3d11_context_->OMSetRenderTargets(0, NULL, NULL);

	DX_SAFE_RELEASE(sharpen_constants_);
	DX_SAFE_RELEASE(main_render_target_view_);
	DX_SAFE_RELEASE(point_sampler_);
	DX_SAFE_RELEASE(linear_sampler_);
	pixel_format_ = PIXEL_FORMAT_UNKNOW;
	width_ = 0;
	height_ = 0;

	for (int i = 0; i < PIXEL_PLANE_MAX; i++) {
		input_textures_[i].reset();
	}

	for (int i = 0; i < PIXEL_SHADER_MAX; i++) {
		render_targets_[i].reset();
	}

	HRESULT hr = dxgi_swap_chain_->ResizeBuffers(
		0, 
		width,
		height,
		DXGI_FORMAT_UNKNOWN,
		0
	);
	if (hr == DXGI_ERROR_DEVICE_REMOVED) {
		Destroy();
		return Init(wnd_);
	}
	else if (FAILED(hr)) {
		return false;
	}

	if (SUCCEEDED(hr)) {
		CreateRenderer();
	}

	return true;
}

void D3D11Renderer::Render(PixelFrame* frame)
{
	std::lock_guard<std::mutex> locker(mutex_);

	if (!d3d11_device_) {
		return;
	}

	if (pixel_format_ != frame->format ||
		width_ != frame->width ||
		height_ != frame->height) {
		if (!CreateTexture(frame->width, frame->height, frame->format)) {
			return;
		}
	}

	Begin();
	Copy(frame);
	Process();
	End();
}

IDXGISwapChain* D3D11Renderer::GetDXGISwapChain()
{
	return dxgi_swap_chain_;
}

ID3D11Device* D3D11Renderer::GetD3D11Device()
{
	return d3d11_device_;
}

void D3D11Renderer::SetSharpen(float unsharp)
{
	unsharp_ = unsharp;
}

bool D3D11Renderer::InitDevice()
{
	RECT rect;
	if (!GetClientRect(wnd_, &rect)) {
		return false;
	}

	UINT device_flags = 0;
#ifdef _DEBUG
	//device_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	D3D_DRIVER_TYPE driver_types[] =
	{
		D3D_DRIVER_TYPE_HARDWARE,
		D3D_DRIVER_TYPE_WARP,
		D3D_DRIVER_TYPE_REFERENCE,
	};

	D3D_FEATURE_LEVEL feature_levels[] =
	{
		//D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
	};

	HRESULT hr = S_OK;
	UINT num_driver_types = ARRAYSIZE(driver_types);
	UINT num_feature_levels = ARRAYSIZE(feature_levels);

	DXGI_SWAP_CHAIN_DESC swap_chain_desc = {};
	IDXGIFactory1* dxgi_factory = NULL;
	ID3D10Multithread* multithread = NULL;

	for (UINT index = 0; index < num_driver_types; index++) {
		driver_type_ = driver_types[index];
		hr = D3D11CreateDevice(
			nullptr, 
			driver_type_, 
			nullptr,
			device_flags,
			feature_levels, 
			num_feature_levels,
			D3D11_SDK_VERSION, 
			&d3d11_device_, 
			&feature_level_, 
			&d3d11_context_
		);

		if (SUCCEEDED(hr)) {
			break;
		}
	}

	if (FAILED(hr)) {
		LOG("D3D11CreateDevice() failed, %x \n", hr);
		goto failed;
	}

	{
		IDXGIDevice* dxgi_device = nullptr;
		hr = d3d11_device_->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgi_device));
		if (SUCCEEDED(hr)) {
			IDXGIAdapter* adapter = nullptr;
			hr = dxgi_device->GetAdapter(&adapter);
			if (SUCCEEDED(hr)) {
				hr = adapter->GetParent(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&dxgi_factory));
				adapter->Release();
			}
			dxgi_device->Release();
		}
	}

	if (FAILED(hr)) {
		LOG("Get DXGI factory failed, %x \n", hr);
		goto failed;
	}

	hr = d3d11_device_->QueryInterface(IID_PPV_ARGS(&multithread));
	if (SUCCEEDED(hr)) {
		if (!multithread->GetMultithreadProtected()) {
			multithread->SetMultithreadProtected(TRUE);
		}
		multithread->Release();
	}

	swap_chain_desc.BufferCount = 1;
	swap_chain_desc.BufferDesc.Width = static_cast<UINT>(rect.right - rect.left);
	swap_chain_desc.BufferDesc.Height = static_cast<UINT>(rect.bottom - rect.top);
	swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swap_chain_desc.BufferDesc.RefreshRate.Numerator = 60;
	swap_chain_desc.BufferDesc.RefreshRate.Denominator = 1;
	swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swap_chain_desc.OutputWindow = wnd_;
	swap_chain_desc.SampleDesc.Count = 1;
	swap_chain_desc.SampleDesc.Quality = 0;
	swap_chain_desc.Windowed = TRUE;

	hr = dxgi_factory->CreateSwapChain(d3d11_device_, &swap_chain_desc, &dxgi_swap_chain_);
	DX_SAFE_RELEASE(dxgi_factory);
	if (FAILED(hr)) {
		LOG("Create swap chain failed, %x \n", hr);
		goto failed;
	}

	return true;

failed:
	DX_SAFE_RELEASE(dxgi_swap_chain_);
	DX_SAFE_RELEASE(d3d11_device_);
	DX_SAFE_RELEASE(d3d11_context_);
	return false;
}

bool D3D11Renderer::CreateRenderer()
{
	if (!d3d11_device_) {
		return false;
	}

	for (int i = 0; i < PIXEL_SHADER_MAX; i++) {
		render_targets_[i].reset(new D3D11RenderTexture(d3d11_device_));
	}

	ID3D11Texture2D* back_buffer = NULL;
	HRESULT hr = dxgi_swap_chain_->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&back_buffer));
	if (FAILED(hr)) {
		LOG("IDXGISwapChain::GetBuffer() failed, %x ", hr);
		return false;
	}

	D3D11_TEXTURE2D_DESC desc;
	back_buffer->GetDesc(&desc);

	hr = d3d11_device_->CreateRenderTargetView(back_buffer, NULL, &main_render_target_view_);
	back_buffer->Release();
	if (FAILED(hr)) {
		LOG("ID3D11Device::CreateRenderTargetView() failed, %x ", hr);
		return false;
	}

	UINT bind_flags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	UINT cpu_flags = 0;
	UINT misc_flags = 0;
	D3D11_USAGE usage = D3D11_USAGE_DEFAULT;

	for (int i = 0; i < PIXEL_SHADER_MAX; i++) {
		auto render_target = render_targets_[i].get();
		render_target->InitTexture(desc.Width, desc.Height, desc.Format, usage, bind_flags, cpu_flags, misc_flags);
		render_target->InitVertexShader();
		render_target->InitRasterizerState();
		if (i == PIXEL_SHADER_ARGB) {
			render_target->InitPixelShader(NULL, shader_d3d11_pixel, sizeof(shader_d3d11_pixel));
		}
		else if (i == PIXEL_SHADER_YUV_BT601) {
			render_target->InitPixelShader(NULL, shader_d3d11_yuv_bt601, sizeof(shader_d3d11_yuv_bt601));
		}
		else if (i == PIXEL_SHADER_YUV_BT709) {
			render_target->InitPixelShader(NULL, shader_d3d11_yuv_bt709, sizeof(shader_d3d11_yuv_bt709));
		}
		else if (i == PIXEL_SHADER_NV12_BT601) {
			render_target->InitPixelShader(NULL, shader_d3d11_nv12_bt601, sizeof(shader_d3d11_nv12_bt601));
		}
		else if (i == PIXEL_SHADER_NV12_BT709) {
			render_target->InitPixelShader(NULL, shader_d3d11_nv12_bt709, sizeof(shader_d3d11_nv12_bt709));
		}
		else if (i == PIXEL_SHADER_SHARPEN) {
			render_target->InitPixelShader(NULL, shader_d3d11_sharpen, sizeof(shader_d3d11_sharpen));
			D3D11_BUFFER_DESC buffer_desc;
			memset(&buffer_desc, 0, sizeof(D3D11_BUFFER_DESC));
			buffer_desc.Usage = D3D11_USAGE_DEFAULT;
			buffer_desc.ByteWidth = sizeof(SharpenShaderConstants);
			buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			buffer_desc.CPUAccessFlags = 0;
			hr = d3d11_device_->CreateBuffer(&buffer_desc, NULL, &sharpen_constants_);
			if (FAILED(hr)) {
				fprintf(stderr, "[D3D11Renderer] CreateBuffer(CONSTANT_BUFFER) failed, %x \n", hr);
				return false;
			}
		}
	}

	D3D11_SAMPLER_DESC sampler_desc;
	memset(&sampler_desc, 0, sizeof(D3D11_SAMPLER_DESC));
	sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_desc.MipLODBias = 0.0f;
	sampler_desc.MaxAnisotropy = 1;
	sampler_desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	sampler_desc.MinLOD = 0.0f;
	sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;

	hr = d3d11_device_->CreateSamplerState(&sampler_desc, &point_sampler_);
	if (FAILED(hr)) {
		LOG("ID3D11Device::CreateSamplerState(POINT) failed, %x ", hr);
		return false;
	}

	sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;// D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR;
	hr = d3d11_device_->CreateSamplerState(&sampler_desc, &linear_sampler_);
	if (FAILED(hr)) {
		LOG("ID3D11Device::CreateSamplerState(LINEAR) failed, %x ", hr);
		return false;
	}

	return true;
}

bool D3D11Renderer::CreateTexture(int width, int height, PixelFormat format)
{
	if (!d3d11_device_) {
		return false;
	}

	for (int i = 0; i < PIXEL_PLANE_MAX; i++) {
		input_textures_[i].reset(new D3D11RenderTexture(d3d11_device_));
	}

	D3D11_USAGE usage = D3D11_USAGE_DYNAMIC;
	UINT bind_flags   = D3D11_BIND_SHADER_RESOURCE;
	UINT cpu_flags    = D3D11_CPU_ACCESS_WRITE;
	UINT half_width   = (width + 1) / 2;
	UINT half_height  = (height + 1) / 2;

	if (format == PIXEL_FORMAT_I420) {
		DXGI_FORMAT dxgi_format = DXGI_FORMAT_R8_UNORM;
		input_textures_[PIXEL_PLANE_Y]->InitTexture(width, height, dxgi_format, usage, bind_flags, cpu_flags, 0);
		input_textures_[PIXEL_PLANE_U]->InitTexture(half_width, half_height, dxgi_format, usage, bind_flags, cpu_flags, 0);
		input_textures_[PIXEL_PLANE_V]->InitTexture(half_width, half_height, dxgi_format, usage, bind_flags, cpu_flags, 0);
	}
	else if (format == PIXEL_FORMAT_I444) {
		DXGI_FORMAT dxgi_format = DXGI_FORMAT_R8_UNORM;
		input_textures_[PIXEL_PLANE_Y]->InitTexture(width, height, dxgi_format, usage, bind_flags, cpu_flags, 0);
		input_textures_[PIXEL_PLANE_U]->InitTexture(width, height, dxgi_format, usage, bind_flags, cpu_flags, 0);
		input_textures_[PIXEL_PLANE_V]->InitTexture(width, height, dxgi_format, usage, bind_flags, cpu_flags, 0);
	}
	else if (format == PIXEL_FORMAT_ARGB) {
		DXGI_FORMAT dxgi_format = DXGI_FORMAT_B8G8R8A8_UNORM;
		input_textures_[PIXEL_PLANE_ARGB]->InitTexture(width, height, dxgi_format, usage, bind_flags, cpu_flags, 0);
	}
	else if (format == PIXEL_FORMAT_NV12) {
		DXGI_FORMAT dxgi_format = DXGI_FORMAT_NV12;
		input_textures_[PIXEL_PLANE_NV12]->InitTexture(width, height, dxgi_format, usage, bind_flags, cpu_flags, 0);
	}

	width_ = width;
	height_ = height;
	pixel_format_ = format;

	return true;
}

void D3D11Renderer::D3D11Renderer::Begin()
{
	if (main_render_target_view_) {
		d3d11_context_->OMSetRenderTargets(1, &main_render_target_view_, NULL);
		d3d11_context_->ClearRenderTargetView(main_render_target_view_, DirectX::Colors::Black);
	}
}

void D3D11Renderer::Copy(PixelFrame* frame)
{
	if (!d3d11_device_) {
		return;
	}

	if (frame->format == PIXEL_FORMAT_I420) {
		if (input_textures_[PIXEL_PLANE_Y] &&
			input_textures_[PIXEL_PLANE_U] &&
			input_textures_[PIXEL_PLANE_V]) {
			UpdateI420(frame);
		}
	}
	else if (frame->format == PIXEL_FORMAT_I444) {
		if (input_textures_[PIXEL_PLANE_Y] &&
			input_textures_[PIXEL_PLANE_U] &&
			input_textures_[PIXEL_PLANE_V]) {
			UpdateI444(frame);
		}
	}
	else if (frame->format == PIXEL_FORMAT_ARGB) {
		if (input_textures_[PIXEL_PLANE_ARGB]) {
			UpdateARGB(frame);
		}
	}
	else if (frame->format == PIXEL_FORMAT_NV12) {
		if (input_textures_[PIXEL_PLANE_NV12]) {
			UpdateNV12(frame);
		}
	}
}

void D3D11Renderer::Process()
{
	if (!output_texture_) {
		return;
	}

	if (unsharp_ > 0) {
		float width = static_cast<float>(width_);
		float height = static_cast<float>(height_);
		D3D11RenderTexture* render_target = render_targets_[PIXEL_SHADER_SHARPEN].get();

		SharpenShaderConstants sharpen_shader_constants;
		sharpen_shader_constants.width = static_cast<float>(width);
		sharpen_shader_constants.height = static_cast<float>(height);
		sharpen_shader_constants.unsharp = unsharp_;
		d3d11_context_->UpdateSubresource((ID3D11Resource*)sharpen_constants_, 0, NULL, &sharpen_shader_constants, 0, 0);

		render_target->Begin();
		render_target->PSSetTexture(0, output_texture_->GetShaderResourceView());
		render_target->PSSetConstant(0, sharpen_constants_);
		render_target->PSSetSamplers(0, linear_sampler_);
		render_target->PSSetSamplers(1, point_sampler_);
		render_target->Draw();
		render_target->End();
		render_target->PSSetTexture(0, NULL);
		output_texture_ = render_target;
	}
}

void D3D11Renderer::End()
{
	if (!dxgi_swap_chain_) {
		return ;
	}

	if (output_texture_) {
		ID3D11Texture2D* texture = output_texture_->GetTexture();
		ID3D11Texture2D* back_buffer = NULL;
		HRESULT hr = dxgi_swap_chain_->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&back_buffer));
		if (SUCCEEDED(hr)) {
			d3d11_context_->CopyResource(back_buffer, texture);
			back_buffer->Release();
		}
		output_texture_ = NULL;
	}

	HRESULT hr = dxgi_swap_chain_->Present(0, 0);

	if (FAILED(hr) && hr != DXGI_ERROR_WAS_STILL_DRAWING) {
		if (hr == DXGI_ERROR_DEVICE_REMOVED) {
			Destroy();
			Init(wnd_);
		}
		else {
			return ;
		}
	}
}

void D3D11Renderer::UpdateARGB(PixelFrame* frame)
{
	HRESULT hr = S_OK;
	D3D11_MAPPED_SUBRESOURCE map;

	ID3D11Texture2D* texture = input_textures_[PIXEL_PLANE_ARGB]->GetTexture();
	ID3D11ShaderResourceView*  shader_resource_view = input_textures_[PIXEL_PLANE_ARGB]->GetShaderResourceView();
	UINT sub_resource = ::D3D11CalcSubresource(0, 0, 1);

	if (texture) {
		hr = d3d11_context_->Map(
			(ID3D11Resource*)texture,
			sub_resource,
			D3D11_MAP_WRITE_DISCARD,
			0,
			&map
		);

		if (SUCCEEDED(hr)) {
			int src_pitch = frame->pitch[0];
			int dst_pitch = map.RowPitch;
			int pitch = (src_pitch <= dst_pitch) ? src_pitch : dst_pitch;
			uint8_t* src_data = frame->plane[0];
			uint8_t* dst_data = (uint8_t*)map.pData;
			for (int i = 0; i < frame->height; i++) {
				memcpy(dst_data, src_data, pitch);
				dst_data += dst_pitch;
				src_data += src_pitch;
			}
		}

		d3d11_context_->Unmap((ID3D11Resource*)texture, sub_resource);

		D3D11RenderTexture* render_target = render_targets_[PIXEL_SHADER_ARGB].get();
		render_target->Begin();
		render_target->PSSetTexture(0, shader_resource_view);
		render_target->PSSetSamplers(0, linear_sampler_);
		render_target->PSSetSamplers(1, point_sampler_);
		render_target->Draw();
		render_target->End();
		render_target->PSSetTexture(0, NULL);
		output_texture_ = render_target;
	}
}

void D3D11Renderer::UpdateI444(PixelFrame* frame)
{
	ID3D11Texture2D* y_texture = input_textures_[PIXEL_PLANE_Y]->GetTexture();
	ID3D11Texture2D* u_texture = input_textures_[PIXEL_PLANE_U]->GetTexture();
	ID3D11Texture2D* v_texture = input_textures_[PIXEL_PLANE_V]->GetTexture();
	ID3D11ShaderResourceView* y_texture_view = input_textures_[PIXEL_PLANE_Y]->GetShaderResourceView();
	ID3D11ShaderResourceView* u_texture_view = input_textures_[PIXEL_PLANE_U]->GetShaderResourceView();
	ID3D11ShaderResourceView* v_texture_view = input_textures_[PIXEL_PLANE_V]->GetShaderResourceView();

	D3D11_MAPPED_SUBRESOURCE map;
	HRESULT hr = S_OK;
	UINT sub_resource = ::D3D11CalcSubresource(0, 0, 1);

	if (y_texture) {
		hr = d3d11_context_->Map(y_texture, sub_resource, D3D11_MAP_WRITE_DISCARD, 0, &map);
		if (SUCCEEDED(hr)) {
			int src_pitch = frame->pitch[0];
			int dst_pitch = map.RowPitch;
			int pitch = (src_pitch <= dst_pitch) ? src_pitch : dst_pitch;
			uint8_t* src_data = frame->plane[0];
			uint8_t* dst_data = (uint8_t*)map.pData;

			for (int i = 0; i < frame->height; i++) {
				memcpy(dst_data, src_data, pitch);
				dst_data += dst_pitch;
				src_data += src_pitch;
			}
		}
		d3d11_context_->Unmap((ID3D11Resource*)y_texture, sub_resource);
	}

	if (u_texture) {
		hr = d3d11_context_->Map(u_texture, sub_resource, D3D11_MAP_WRITE_DISCARD, 0, &map);
		if (SUCCEEDED(hr)) {
			int src_pitch = frame->pitch[1];
			int dst_pitch = map.RowPitch;
			int pitch = (src_pitch <= dst_pitch) ? src_pitch : dst_pitch;
			uint8_t* src_data = frame->plane[1];
			uint8_t* dst_data = (uint8_t*)map.pData;

			for (int i = 0; i < frame->height; i++) {
				memcpy(dst_data, src_data, pitch);
				dst_data += dst_pitch;
				src_data += src_pitch;
			}
		}

		d3d11_context_->Unmap((ID3D11Resource*)u_texture, sub_resource);
	}

	if (v_texture) {
		hr = d3d11_context_->Map(v_texture, sub_resource, D3D11_MAP_WRITE_DISCARD, 0, &map);
		if (SUCCEEDED(hr)) {
			int src_pitch = frame->pitch[2];
			int dst_pitch = map.RowPitch;
			int pitch = (src_pitch <= dst_pitch) ? src_pitch : dst_pitch;
			uint8_t* src_data = frame->plane[2];
			uint8_t* dst_data = (uint8_t*)map.pData;

			for (int i = 0; i < frame->height; i++) {
				memcpy(dst_data, src_data, pitch);
				dst_data += dst_pitch;
				src_data += src_pitch;
			}
		}

		d3d11_context_->Unmap((ID3D11Resource*)v_texture, sub_resource);
	}

	D3D11RenderTexture* render_target = render_targets_[PIXEL_SHADER_YUV_BT601].get();
	if (render_target) {
		render_target->Begin();
		render_target->PSSetTexture(0, y_texture_view);
		render_target->PSSetTexture(1, u_texture_view);
		render_target->PSSetTexture(2, v_texture_view);
		render_target->PSSetSamplers(0, linear_sampler_);
		render_target->PSSetSamplers(1, point_sampler_);
		render_target->Draw();
		render_target->End();
		render_target->PSSetTexture(0, NULL);
		render_target->PSSetTexture(1, NULL);
		render_target->PSSetTexture(2, NULL);
		output_texture_ = render_target;
	}
}

void D3D11Renderer::UpdateI420(PixelFrame* frame)
{
	ID3D11Texture2D* y_texture = input_textures_[PIXEL_PLANE_Y]->GetTexture();
	ID3D11Texture2D* u_texture = input_textures_[PIXEL_PLANE_U]->GetTexture();
	ID3D11Texture2D* v_texture = input_textures_[PIXEL_PLANE_V]->GetTexture();
	ID3D11ShaderResourceView* y_texture_view = input_textures_[PIXEL_PLANE_Y]->GetShaderResourceView();
	ID3D11ShaderResourceView* u_texture_view = input_textures_[PIXEL_PLANE_U]->GetShaderResourceView();
	ID3D11ShaderResourceView* v_texture_view = input_textures_[PIXEL_PLANE_V]->GetShaderResourceView();

	D3D11_MAPPED_SUBRESOURCE map;
	HRESULT hr = S_OK;
	UINT sub_resource = ::D3D11CalcSubresource(0, 0, 1);
	int half_width = (frame->width + 1) / 2;
	int half_height = (frame->height + 1) / 2;

	if (y_texture) {
		hr = d3d11_context_->Map(y_texture, sub_resource, D3D11_MAP_WRITE_DISCARD, 0, &map);
		if (SUCCEEDED(hr)) {
			int src_pitch = frame->pitch[0];
			int dst_pitch = map.RowPitch;
			int pitch = (src_pitch <= dst_pitch) ? src_pitch : dst_pitch;
			uint8_t* src_data = frame->plane[0];
			uint8_t* dst_data = (uint8_t*)map.pData;

			for (int i = 0; i < frame->height; i++) {
				memcpy(dst_data, src_data, pitch);
				dst_data += dst_pitch;
				src_data += src_pitch;
			}
		}
		d3d11_context_->Unmap((ID3D11Resource*)y_texture, sub_resource);
	}

	if (u_texture) {
		hr = d3d11_context_->Map(u_texture, sub_resource, D3D11_MAP_WRITE_DISCARD, 0, &map);
		if (SUCCEEDED(hr)) {
			int src_pitch = frame->pitch[1];
			int dst_pitch = map.RowPitch;
			int pitch = (src_pitch <= dst_pitch) ? src_pitch : dst_pitch;
			uint8_t* src_data = frame->plane[1];
			uint8_t* dst_data = (uint8_t*)map.pData;

			for (int i = 0; i < half_height; i++) {
				memcpy(dst_data, src_data, pitch);
				dst_data += dst_pitch;
				src_data += src_pitch;
			}
		}

		d3d11_context_->Unmap((ID3D11Resource*)u_texture, sub_resource);
	}

	if (v_texture) {
		hr = d3d11_context_->Map(v_texture, sub_resource, D3D11_MAP_WRITE_DISCARD, 0, &map);
		if (SUCCEEDED(hr)) {
			int src_pitch = frame->pitch[2];
			int dst_pitch = map.RowPitch;
			int pitch = (src_pitch <= dst_pitch) ? src_pitch : dst_pitch;
			uint8_t* src_data = frame->plane[2];
			uint8_t* dst_data = (uint8_t*)map.pData;

			for (int i = 0; i < half_height; i++) {
				memcpy(dst_data, src_data, pitch);
				dst_data += dst_pitch;
				src_data += src_pitch;
			}
		}

		d3d11_context_->Unmap((ID3D11Resource*)v_texture, sub_resource);
	}

	D3D11RenderTexture* render_target = render_targets_[PIXEL_SHADER_YUV_BT601].get();
	if (render_target) {
		render_target->Begin();
		render_target->PSSetTexture(0, y_texture_view);
		render_target->PSSetTexture(1, u_texture_view);
		render_target->PSSetTexture(2, v_texture_view);
		render_target->PSSetSamplers(0, linear_sampler_);
		render_target->PSSetSamplers(1, point_sampler_);
		render_target->Draw();
		render_target->End();
		render_target->PSSetTexture(0, NULL);
		render_target->PSSetTexture(1, NULL);
		render_target->PSSetTexture(2, NULL);
		output_texture_ = render_target;
	}
}

void D3D11Renderer::UpdateNV12(PixelFrame* frame)
{
	ID3D11Texture2D* texture = input_textures_[PIXEL_PLANE_NV12]->GetTexture();
	ID3D11ShaderResourceView* luminance_view = input_textures_[PIXEL_PLANE_NV12]->GetNV12YShaderResourceView();
	ID3D11ShaderResourceView* chrominance_view = input_textures_[PIXEL_PLANE_NV12]->GetNV12UVShaderResourceView();

	D3D11_MAPPED_SUBRESOURCE map;
	HRESULT hr = S_OK;
	UINT sub_resource = ::D3D11CalcSubresource(0, 0, 1);
	int half_width = (frame->width + 1) / 2;
	int half_height = (frame->height + 1) / 2;

	if (texture) {
		hr = d3d11_context_->Map(texture, sub_resource, D3D11_MAP_WRITE_DISCARD, 0, &map);
		if (SUCCEEDED(hr)) {
			uint8_t* src_data = (uint8_t*)frame->plane[0];
			uint8_t* dst_data = (uint8_t*)map.pData;
			int src_pitch = frame->pitch[0];
			int dst_pitch = map.RowPitch;
			int pitch = (src_pitch <= dst_pitch) ? src_pitch : dst_pitch;

			for (int i = 0; i < frame->height; i++) {
				memcpy(dst_data, src_data, pitch);
				src_data += src_pitch;
				dst_data += dst_pitch;
			}

			src_data = (uint8_t*)frame->plane[1];
			src_pitch = frame->pitch[1];
			pitch = (src_pitch <= dst_pitch) ? src_pitch : dst_pitch;

			for (int i = 0; i < half_height; i++) {
				memcpy(dst_data, src_data, pitch);
				src_data += src_pitch;
				dst_data += dst_pitch;
			}
		}
		d3d11_context_->Unmap((ID3D11Resource*)texture, sub_resource);
	}

	D3D11RenderTexture* render_target = render_targets_[PIXEL_SHADER_NV12_BT601].get();
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
}
