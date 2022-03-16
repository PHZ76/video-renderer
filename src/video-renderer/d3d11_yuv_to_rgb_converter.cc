#include "d3d11_yuv_to_rgb_converter.h"
#include "shader/d3d11/shader_d3d11_yuv_to_rgb.h"

#include "log.h"
#include <wrl/client.h>

using namespace DX;

struct YUVParams
{
	float width;
	float height;
	float align0_;
	float align1_;
};

D3D11YUVToRGBConverter::D3D11YUVToRGBConverter(ID3D11Device* d3d11_device)
	: d3d11_device_(d3d11_device)
{
	d3d11_device_->AddRef();
	d3d11_device_->GetImmediateContext(&d3d11_context_);
}

D3D11YUVToRGBConverter::~D3D11YUVToRGBConverter()
{
	d3d11_context_->Release();
	d3d11_device_->Release();
}

bool D3D11YUVToRGBConverter::Init(int width, int height)
{
	if (!CreateTexture(width, height)) {
		return false;
	}

	if (!CreateSampler()) {
		return false;
	}

	if (!CreateBuffer()) {
		return false;
	}

	width_ = width;
	height_ = height;
	return true;
}

void D3D11YUVToRGBConverter::Destroy()
{
	if (point_sampler_) {		
		point_sampler_->Release();
		point_sampler_ = nullptr;
	}

	if (buffer_) {
		buffer_->Release();
		buffer_ = nullptr;
	}

	if (rgba_texture_) {
		rgba_texture_ = nullptr;
	}
}

bool D3D11YUVToRGBConverter::CreateTexture(int width, int height)
{
	rgba_texture_.reset(new D3D11RenderTexture(d3d11_device_));

	D3D11_USAGE usage = D3D11_USAGE_DEFAULT;
	DXGI_FORMAT format = DXGI_FORMAT_B8G8R8A8_UNORM;
	UINT bind_flags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	if (!rgba_texture_->InitTexture(width, height, format, usage, bind_flags, 0, 0)) {
		return  false;
	}

	if (!rgba_texture_->InitTexture(width, height, format, usage, bind_flags, 0, 0)) {
		return  false;
	}

	if (!rgba_texture_->InitVertexShader()) {
		return  false;
	}

	if (!rgba_texture_->InitRasterizerState()) {
		return  false;
	}

	if (!rgba_texture_->InitPixelShader(NULL, shader_d3d11_yuv_to_rgb, sizeof(shader_d3d11_yuv_to_rgb))) {
		return  false;
	}

	return true;
}

bool D3D11YUVToRGBConverter::CreateSampler()
{
	if (point_sampler_) {
		point_sampler_->Release();
	}

	D3D11_SAMPLER_DESC sampler_desc;
	memset(&sampler_desc, 0, sizeof(D3D11_SAMPLER_DESC));
	sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;//D3D11_FILTER_MIN_MAG_MIP_LINEAR;//D3D11_FILTER_MIN_MAG_MIP_POINT;
	sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_desc.MipLODBias = 0.0f;
	sampler_desc.MaxAnisotropy = 1;
	sampler_desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	sampler_desc.MinLOD = 0.0f;
	sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;

	HRESULT hr = d3d11_device_->CreateSamplerState(&sampler_desc, &point_sampler_);
	if (FAILED(hr)) {
		LOG("ID3D11Device::CreateSamplerState(POINT) failed, %x", hr);
		return false;
	}

	return true;
}

bool D3D11YUVToRGBConverter::CreateBuffer()
{
	if (buffer_) {
		buffer_->Release();
	}

	D3D11_BUFFER_DESC buffer_desc;
	memset(&buffer_desc, 0, sizeof(D3D11_BUFFER_DESC));
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.ByteWidth = sizeof(YUVParams);
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	buffer_desc.CPUAccessFlags = 0;

	HRESULT hr = d3d11_device_->CreateBuffer(&buffer_desc, NULL, &buffer_);
	if (FAILED(hr)) {
		LOG("[D3D11Renderer] CreateBuffer(CONSTANT_BUFFER) failed, %x", hr);
		return false;
	}

	return true;
}

bool D3D11YUVToRGBConverter::Combine(ID3D11Texture2D* yuv420_texture, ID3D11Texture2D* chroma420_texture)
{
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> yuv420_y_srv;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> yuv420_uv_srv;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> chroma420_y_srv;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> chroma420_uv_srv;

	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
	HRESULT hr = S_OK;
	srv_desc.Format = DXGI_FORMAT_R8_UNORM;
	srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srv_desc.Texture2D.MostDetailedMip = 0;
	srv_desc.Texture2D.MipLevels = 1;

	hr = d3d11_device_->CreateShaderResourceView(yuv420_texture, &srv_desc, yuv420_y_srv.GetAddressOf());
	if (FAILED(hr)) {
		return false;
	}

	hr = d3d11_device_->CreateShaderResourceView(chroma420_texture, &srv_desc, chroma420_y_srv.GetAddressOf());
	if (FAILED(hr)) {
		return false;
	}

	srv_desc.Format = DXGI_FORMAT_R8G8_UNORM;

	hr = d3d11_device_->CreateShaderResourceView(yuv420_texture, &srv_desc, yuv420_uv_srv.GetAddressOf());
	if (FAILED(hr)) {
		return false;
	}

	hr = d3d11_device_->CreateShaderResourceView(chroma420_texture, &srv_desc, chroma420_uv_srv.GetAddressOf());
	if (FAILED(hr)) {
		return false;
	}

	YUVParams yuv_params;
	yuv_params.width = static_cast<float>(width_);
	yuv_params.height = static_cast<float>(height_);
	d3d11_context_->UpdateSubresource((ID3D11Resource*)buffer_, 0, NULL, &yuv_params, 0, 0);

	rgba_texture_->Begin();
	rgba_texture_->PSSetTexture(0, yuv420_y_srv.Get());
	rgba_texture_->PSSetTexture(1, yuv420_uv_srv.Get());
	rgba_texture_->PSSetTexture(2, chroma420_y_srv.Get());
	rgba_texture_->PSSetTexture(3, chroma420_uv_srv.Get());
	rgba_texture_->PSSetConstant(0, buffer_);
	rgba_texture_->PSSetSamplers(0, point_sampler_);
	rgba_texture_->Draw();
	rgba_texture_->End();
	rgba_texture_->PSSetTexture(0, NULL);
	rgba_texture_->PSSetTexture(1, NULL);
	rgba_texture_->PSSetTexture(2, NULL);
	rgba_texture_->PSSetTexture(3, NULL);

	return true;
}

bool D3D11YUVToRGBConverter::Combine(ID3D11Texture2D* yuv420_texture, int yuv420_index, ID3D11Texture2D* chroma420_texture, int chroma420_index)
{
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> yuv420_y_srv;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> yuv420_uv_srv;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> chroma420_y_srv;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> chroma420_uv_srv;

	D3D11_TEXTURE2D_DESC yuv420_texture_desc;
	yuv420_texture->GetDesc(&yuv420_texture_desc);

	D3D11_TEXTURE2D_DESC chroma420_texture_desc;
	yuv420_texture->GetDesc(&chroma420_texture_desc);

	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
	HRESULT hr = S_OK;
	srv_desc.Format = DXGI_FORMAT_R8_UNORM;
	srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
	srv_desc.Texture2DArray.MipLevels = 1;
	srv_desc.Texture2DArray.ArraySize = 1;
	srv_desc.Texture2DArray.MostDetailedMip = 0;
	srv_desc.Texture2DArray.FirstArraySlice = 0;


	srv_desc.Texture2DArray.FirstArraySlice = yuv420_index;
	hr = d3d11_device_->CreateShaderResourceView(yuv420_texture, &srv_desc, yuv420_y_srv.GetAddressOf());
	if (FAILED(hr)) {
		return false;
	}

	srv_desc.Texture2DArray.FirstArraySlice = chroma420_index;
	hr = d3d11_device_->CreateShaderResourceView(chroma420_texture, &srv_desc, chroma420_y_srv.GetAddressOf());
	if (FAILED(hr)) {
		return false;
	}

	srv_desc.Format = DXGI_FORMAT_R8G8_UNORM;
	srv_desc.Texture2DArray.FirstArraySlice = yuv420_index;
	hr = d3d11_device_->CreateShaderResourceView(yuv420_texture, &srv_desc, yuv420_uv_srv.GetAddressOf());
	if (FAILED(hr)) {
		return false;
	}

	srv_desc.Texture2DArray.FirstArraySlice = chroma420_index;
	hr = d3d11_device_->CreateShaderResourceView(chroma420_texture, &srv_desc, chroma420_uv_srv.GetAddressOf());
	if (FAILED(hr)) {
		return false;
	}

	YUVParams yuv_params;
	yuv_params.width = static_cast<float>(width_);
	yuv_params.height = static_cast<float>(height_);
	d3d11_context_->UpdateSubresource((ID3D11Resource*)buffer_, 0, NULL, &yuv_params, 0, 0);

	rgba_texture_->Begin();
	rgba_texture_->PSSetTexture(0, yuv420_y_srv.Get());
	rgba_texture_->PSSetTexture(1, yuv420_uv_srv.Get());
	rgba_texture_->PSSetTexture(2, chroma420_y_srv.Get());
	rgba_texture_->PSSetTexture(3, chroma420_uv_srv.Get());
	rgba_texture_->PSSetConstant(0, buffer_);
	rgba_texture_->PSSetSamplers(0, point_sampler_);
	rgba_texture_->Draw();
	rgba_texture_->End();
	rgba_texture_->PSSetTexture(0, NULL);
	rgba_texture_->PSSetTexture(1, NULL);
	rgba_texture_->PSSetTexture(2, NULL);
	rgba_texture_->PSSetTexture(3, NULL);

	return true;
}

ID3D11Texture2D* D3D11YUVToRGBConverter::GetRGBATexture()
{
	if (rgba_texture_) {
		return rgba_texture_->GetTexture();
	}

	return nullptr;
}