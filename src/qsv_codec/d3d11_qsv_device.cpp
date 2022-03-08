#include "d3d11_qsv_device.h"
#include "common_utils.h"
#include <atlbase.h>

const struct {
	mfxIMPL impl;        // actual implementation
	mfxU32  adapter_id;  // device adapter number
} ImplTypes[] = {
	{MFX_IMPL_HARDWARE, 0},
	{MFX_IMPL_HARDWARE2, 1},
	{MFX_IMPL_HARDWARE3, 2},
	{MFX_IMPL_HARDWARE4, 3}
};

D3D11QSVDevice::D3D11QSVDevice()
{

}

D3D11QSVDevice::~D3D11QSVDevice()
{
	Destroy();
}

bool D3D11QSVDevice::Init()
{
	HRESULT hr = S_OK;
	CComPtr<IDXGIFactory2> factory;
	CComPtr<IDXGIAdapter>  adapter;

	mfxVersion mfx_ver = { {0, 1} };
	MFXVideoSession mfx_session;
	mfxIMPL mfx_impl = MFX_IMPL_AUTO_ANY | MFX_IMPL_VIA_D3D11;
	mfxStatus sts = MFX_ERR_NONE;
	mfxU32 adapter_id = 0;

	sts = mfx_session.Init(mfx_impl, &mfx_ver);
	if (sts != MFX_ERR_NONE) {
		return false;
	}

	MFXQueryIMPL(mfx_session, &mfx_impl);
	mfxIMPL base_impl = MFX_IMPL_BASETYPE(mfx_impl);

	for (mfxU8 i = 0; i < sizeof(ImplTypes) / sizeof(ImplTypes[0]); i++) {
		if (ImplTypes[i].impl == base_impl) {
			adapter_id = ImplTypes[i].adapter_id;
			break;
		}
	}

	mfx_session.Close();

	hr = CreateDXGIFactory(__uuidof(IDXGIFactory2), (void**)(&factory));
	if (FAILED(hr)) {
		return false;
	}
		
	hr = factory->EnumAdapters(adapter_id, &adapter);
	if (FAILED(hr)) {
		return false;
	}

	D3D_FEATURE_LEVEL feature_level;
	//UINT flags = D3D11_CREATE_DEVICE_DEBUG;

	hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
		&d3d11_device_, &feature_level, &d3d11_device_context_);

	if (FAILED(hr)) {
		return false;
	}

	// turn on multithreading for the DX11 context
	CComQIPtr<ID3D10Multithread> p_mt(d3d11_device_context_);
	if (p_mt) {
		p_mt->SetMultithreadProtected(true);
	}
	
	return true;
}

void D3D11QSVDevice::Destroy()
{
	if (d3d11_device_) {
		d3d11_device_->Release();
		d3d11_device_ = NULL;
	}

	if (d3d11_device_context_) {
		d3d11_device_context_->Release();
		d3d11_device_context_ = NULL;
	}
}