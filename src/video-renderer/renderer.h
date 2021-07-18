#pragma once

#include <Windows.h>
#include <memory>

namespace xop {

enum PixelFormat
{
	PIXEL_FORMAT_UNKNOW = 0,
	PIXEL_FORMAT_ARGB,
	PIXEL_FORMAT_I420,
	PIXEL_FORMAT_NV12,
	PIXEL_FORMAT_I444,
	PIXEL_FORMAT_MAX,
};

enum PixelShader
{
	PIXEL_SHADER_UNKNOW = 0,
	PIXEL_SHADER_ARGB,
	PIXEL_SHADER_YUV_BT601,
	PIXEL_SHADER_YUV_BT709,
	PIXEL_SHADER_NV12_BT601,
	PIXEL_SHADER_NV12_BT709,
	PIXEL_SHADER_SHARPEN,
	PIXEL_SHADER_MAX,
};

enum PixelPlane
{	
	PIXEL_PLANE_UNKNOW = 0,
	PIXEL_PLANE_ARGB,
	PIXEL_PLANE_NV12,
	PIXEL_PLANE_Y,
	PIXEL_PLANE_U,
	PIXEL_PLANE_V,
	PIXEL_PLANE_UV,
	PIXEL_PLANE_MAX,
};

struct PixelFrame
{
	int          width  = 0;
	int          height = 0;
	int          pitch[3] = { 0, 0, 0 };
	uint8_t*     plane[3] = { NULL, NULL, NULL };
	PixelFormat  format = PIXEL_FORMAT_UNKNOW;
};

class Renderer
{
public:
	Renderer() {};
	virtual ~Renderer() {};

	virtual bool Init(HWND hwnd) = 0;
	virtual void Destroy() = 0;

	virtual bool Resize() = 0;

	virtual void Render(PixelFrame* frame) = 0;

	virtual void SetSharpen(float unsharp) {}

};

}
