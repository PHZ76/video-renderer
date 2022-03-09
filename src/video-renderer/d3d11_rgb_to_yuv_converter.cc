#include "d3d11_rgb_to_yuv_converter.h"
#include "shader/d3d11/shader_d3d11_rgb_to_yuv420.h"
#include "shader/d3d11/shader_d3d11_rgb_to_chroma420.h"

#include "log.h"
#include <wrl/client.h>

using namespace DX;

struct RGBParams
{
	float width;
	float height;
	float align0_;
	float align1_;
};

D3D11RGBToYUVConverter::D3D11RGBToYUVConverter(ID3D11Device* d3d11_device)
	: d3d11_device_(d3d11_device)
{
	d3d11_device_->AddRef();
	d3d11_device_->GetImmediateContext(&d3d11_context_);
}

D3D11RGBToYUVConverter::~D3D11RGBToYUVConverter()
{
	d3d11_context_->Release();
	d3d11_device_->Release();
}

bool D3D11RGBToYUVConverter::Init(int width, int height)
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

void D3D11RGBToYUVConverter::Destroy()
{
	if (point_sampler_) {		
		point_sampler_->Release();
		point_sampler_ = NULL;
	}

	if (buffer_) {
		buffer_->Release();
		buffer_ = NULL;
	}

	if (yuv420_texture_) {
		yuv420_texture_ = nullptr;
	}

	if (chroma420_texture_) {
		chroma420_texture_ = nullptr;
	}
}

bool D3D11RGBToYUVConverter::CreateTexture(int width, int height)
{
	yuv420_texture_.reset(new D3D11RenderTexture(d3d11_device_));
	chroma420_texture_.reset(new D3D11RenderTexture(d3d11_device_));

	D3D11_USAGE usage = D3D11_USAGE_DEFAULT;
	DXGI_FORMAT format = DXGI_FORMAT_NV12;
	UINT bind_flags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	if (!yuv420_texture_->InitTexture(width, height, format, usage, bind_flags, 0, 0)) {
		return  false;
	}

	if (!chroma420_texture_->InitTexture(width, height, format, usage, bind_flags, 0, 0)) {
		return  false;
	}

	if (!yuv420_texture_->InitVertexShader()) {
		return  false;
	}

	if (!chroma420_texture_->InitVertexShader()) {
		return  false;
	}

	if (!yuv420_texture_->InitRasterizerState()) {
		return  false;
	}

	if (!chroma420_texture_->InitRasterizerState()) {
		return  false;
	}

	if (!yuv420_texture_->InitPixelShader(NULL, shader_d3d11_rgb_to_yuv420, sizeof(shader_d3d11_rgb_to_yuv420))) {
		return  false;
	}

	if (!chroma420_texture_->InitPixelShader(NULL, shader_d3d11_rgb_to_chroma420, sizeof(shader_d3d11_rgb_to_chroma420))) {
		return  false;
	}

	return true;
}

bool D3D11RGBToYUVConverter::CreateSampler()
{
	if (point_sampler_) {
		point_sampler_->Release();
	}

	D3D11_SAMPLER_DESC sampler_desc;
	memset(&sampler_desc, 0, sizeof(D3D11_SAMPLER_DESC));
	sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT; // D3D11_FILTER_MIN_MAG_MIP_POINT;  //D3D11_FILTER_MIN_MAG_MIP_LINEAR
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

bool D3D11RGBToYUVConverter::CreateBuffer()
{
	if (buffer_) {
		buffer_->Release();
	}

	D3D11_BUFFER_DESC buffer_desc;
	memset(&buffer_desc, 0, sizeof(D3D11_BUFFER_DESC));
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.ByteWidth = sizeof(RGBParams);
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	buffer_desc.CPUAccessFlags = 0;

	HRESULT hr = d3d11_device_->CreateBuffer(&buffer_desc, NULL, &buffer_);
	if (FAILED(hr)) {
		LOG("[D3D11Renderer] CreateBuffer(CONSTANT_BUFFER) failed, %x", hr);
		return false;
	}

	return true;
}

bool D3D11RGBToYUVConverter::Convert(ID3D11Texture2D* rgba_texture)
{
	if (!yuv420_texture_) {
		return false;
	}

	D3D11_TEXTURE2D_DESC texture_desc;
	rgba_texture->GetDesc(&texture_desc);

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> argb_srv;
	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
	srv_desc.Format = texture_desc.Format;
	srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srv_desc.Texture2D.MostDetailedMip = 0;
	srv_desc.Texture2D.MipLevels = 1;

	HRESULT hr = d3d11_device_->CreateShaderResourceView(rgba_texture, &srv_desc, argb_srv.GetAddressOf());
	if (FAILED(hr)) {
		LOG("[D3D11Renderer] CreateShaderResourceView(CONSTANT_BUFFER) failed, %x", hr);
		return false;
	}

	RGBParams rgb_params;
	rgb_params.width = static_cast<float>(width_);
	rgb_params.height = static_cast<float>(height_);
	d3d11_context_->UpdateSubresource((ID3D11Resource*)buffer_, 0, NULL, &rgb_params, 0, 0);

	yuv420_texture_->Begin();
	yuv420_texture_->PSSetTexture(0, argb_srv.Get());
	yuv420_texture_->PSSetConstant(0, buffer_);
	yuv420_texture_->PSSetSamplers(0, point_sampler_);
	yuv420_texture_->Draw();
	yuv420_texture_->End();
	yuv420_texture_->PSSetTexture(0, NULL);

	chroma420_texture_->Begin();
	chroma420_texture_->PSSetTexture(0, argb_srv.Get());
	chroma420_texture_->PSSetConstant(0, buffer_);
	chroma420_texture_->PSSetSamplers(0, point_sampler_);
	chroma420_texture_->Draw();
	chroma420_texture_->End();
	chroma420_texture_->PSSetTexture(0, NULL);

	return true;
}

ID3D11Texture2D* D3D11RGBToYUVConverter::GetYUV420Texture()
{
	if (yuv420_texture_) {
		return yuv420_texture_->GetTexture();
	}

	return nullptr;
}

ID3D11Texture2D* D3D11RGBToYUVConverter::GetChroma420Texture()
{
	if (chroma420_texture_) {
		return chroma420_texture_->GetTexture();
	}

	return nullptr;
}