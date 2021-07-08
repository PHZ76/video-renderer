#include "main_window.h"
#include "d3d9_screen_capture.h"
#include "d3d11_screen_capture.h"
#include "d3d9_renderer.h"
#include "d3d11_renderer.h"

#include "libyuv/libyuv.h"

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3d9.lib")

static void GetWindowSize(HWND hwnd, int& width, int& height)
{
	RECT rect;
	GetWindowRect(hwnd, &rect);
	width  = static_cast<int>(rect.right - rect.left);
	height = static_cast<int>(rect.bottom - rect.top);
}

static void RenderARGB(xop::Renderer* renderer, DX::Image argb_image)
{
	xop::PixelFrame pixel_frame;

	pixel_frame.width = argb_image.width;
	pixel_frame.height = argb_image.height;
	pixel_frame.format = xop::PIXEL_FORMAT_ARGB;
	pixel_frame.pitch[0] = argb_image.width * 4;
	pixel_frame.plane[0] = &argb_image.bgra[0];

	renderer->Render(&pixel_frame);
}

static void RenderI444(xop::Renderer* renderer, DX::Image argb_image)
{
	xop::PixelFrame pixel_frame;

	int dst_stride_y = argb_image.width;
	int dst_stride_u = argb_image.width;
	int dst_stride_v = argb_image.width;
	std::unique_ptr<uint8_t> dst_y(new uint8_t[dst_stride_y * argb_image.height]);
	std::unique_ptr<uint8_t> dst_u(new uint8_t[dst_stride_u * argb_image.height]);
	std::unique_ptr<uint8_t> dst_v(new uint8_t[dst_stride_v * argb_image.height]);

	libyuv::ARGBToI444(
		&argb_image.bgra[0], 
		argb_image.width * 4,
		dst_y.get(), 
		dst_stride_y, 
		dst_u.get(),
		dst_stride_u,
		dst_v.get(), 
		dst_stride_v, 
		argb_image.width, 
		argb_image.height
	);

	pixel_frame.width = argb_image.width;
	pixel_frame.height = argb_image.height;
	pixel_frame.format = xop::PIXEL_FORMAT_I444;
	pixel_frame.pitch[0] = dst_stride_y;
	pixel_frame.pitch[1] = dst_stride_u;
	pixel_frame.pitch[2] = dst_stride_v;
	pixel_frame.plane[0] = dst_y.get();
	pixel_frame.plane[1] = dst_u.get();
	pixel_frame.plane[2] = dst_v.get();

	renderer->Render(&pixel_frame);
}

static void RenderI420(xop::Renderer* renderer, DX::Image argb_image)
{
	xop::PixelFrame pixel_frame;

	int dst_stride_y = argb_image.width;
	int dst_stride_u = (argb_image.width + 1) / 2;
	int dst_stride_v = (argb_image.width + 1) / 2;
	std::unique_ptr<uint8_t> dst_y(new uint8_t[dst_stride_y * argb_image.height]);
	std::unique_ptr<uint8_t> dst_u(new uint8_t[dst_stride_u * argb_image.height / 2]);
	std::unique_ptr<uint8_t> dst_v(new uint8_t[dst_stride_v * argb_image.height / 2]);

	libyuv::ARGBToI420(
		&argb_image.bgra[0],
		argb_image.width * 4,
		dst_y.get(),
		dst_stride_y,
		dst_u.get(),
		dst_stride_u,
		dst_v.get(),
		dst_stride_v,
		argb_image.width,
		argb_image.height
	);

	pixel_frame.width = argb_image.width;
	pixel_frame.height = argb_image.height;
	pixel_frame.format = xop::PIXEL_FORMAT_I420;
	pixel_frame.pitch[0] = dst_stride_y;
	pixel_frame.pitch[1] = dst_stride_u;
	pixel_frame.pitch[2] = dst_stride_v;
	pixel_frame.plane[0] = dst_y.get();
	pixel_frame.plane[1] = dst_u.get();
	pixel_frame.plane[2] = dst_v.get();

	renderer->Render(&pixel_frame);
}

static void RenderNV12(xop::Renderer* renderer, DX::Image argb_image)
{
	xop::PixelFrame pixel_frame;

	int dst_stride_y  = argb_image.width;
	int dst_stride_uv = argb_image.width;

	std::unique_ptr<uint8_t> dst_y(new uint8_t[dst_stride_y * argb_image.height]);
	std::unique_ptr<uint8_t> dst_uv(new uint8_t[dst_stride_uv * argb_image.height / 2]);

	libyuv::ARGBToNV12(
		&argb_image.bgra[0],
		argb_image.width * 4,
		dst_y.get(),
		dst_stride_y,
		dst_uv.get(),
		dst_stride_uv,
		argb_image.width,
		argb_image.height
	);
	
	pixel_frame.width    = argb_image.width;
	pixel_frame.height   = argb_image.height;
	pixel_frame.format   = xop::PIXEL_FORMAT_NV12;
	pixel_frame.pitch[0] = dst_stride_y;
	pixel_frame.pitch[1] = dst_stride_uv;
	pixel_frame.plane[0] = dst_y.get();
	pixel_frame.plane[1] = dst_uv.get();

	renderer->Render(&pixel_frame);
}

int main(int argc, char** argv)
{
	MainWindow window;
	if (!window.Init(100, 100, 1920 * 4 / 5, 1080 * 4 / 5)) {
		return -1;
	}

	DX::D3D11ScreenCapture screen_capture;
	if (!screen_capture.Init()) {
		return -2;
	}

	xop::D3D9Renderer renderer;
	if (!renderer.Init(window.GetHandle())) {
		return -3;
	}

	//renderer.SetSharpen(0.5);

	int original_width = 0, original_height = 0;
	GetWindowSize(window.GetHandle(), original_width, original_height);

	MSG msg;
	ZeroMemory(&msg, sizeof(msg));

	xop::PixelFormat render_format = xop::PIXEL_FORMAT_I420;

	while (msg.message != WM_QUIT) {
		if (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
			continue;
		}
		else {
			int current_width = 0, current_height = 0;
			GetWindowSize(window.GetHandle(), current_width, current_height);
			if (current_width != original_width || current_height != original_height) {
				original_width = current_width;
				original_height = current_height;
				renderer.Resize();
			}

			DX::Image argb_image;
			if (screen_capture.Capture(argb_image)) {
				if (render_format == xop::PIXEL_FORMAT_ARGB) {
					RenderARGB(&renderer, argb_image);
				}
				else if (render_format == xop::PIXEL_FORMAT_I444) {
					RenderI444(&renderer, argb_image);
				}
				else if (render_format == xop::PIXEL_FORMAT_I420) {
					RenderI420(&renderer, argb_image);
				}
				else if (render_format == xop::PIXEL_FORMAT_NV12) {
					RenderNV12(&renderer, argb_image);
				}
			}
		}
	}

	return 0;
}
