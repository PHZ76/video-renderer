#pragma once

#include <d3d11.h>
#include <dxgi1_2.h>

class D3D11QSVDevice
{
public:
	D3D11QSVDevice();
	virtual ~D3D11QSVDevice();

	bool Init();
	void Destroy();

	ID3D11Device* GetD3D11Device() const
	{ return d3d11_device_; }

	ID3D11DeviceContext* GetD3D11DeviceContext() const
	{ return d3d11_device_context_; }

private:
	ID3D11Device* d3d11_device_ = NULL;
	ID3D11DeviceContext* d3d11_device_context_ = NULL;
};