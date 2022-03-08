#include "d3d11_qsv_device.h"
#include "d3d11_qsv_encoder.h"
#include "d3d11_screen_capture.h"
#include "d3d11_rgb_to_yuv_converter.h"
#include <cstdio>

int main(int argc, char** argv)
{
	DX::D3D11ScreenCapture screen_capture;
	if (screen_capture.Init()) {
		printf("init capture succeeded !!! \n");
	}

	DX::Image argb_image;
	screen_capture.Capture(argb_image);

	D3D11QSVDevice device;
	if (device.Init()) {
		printf("init device succeeded !!! \n");
	}

	ID3D11Device* d3d11_device = device.GetD3D11Device();
	xop::D3D11RGBToYUVConverter rgb_to_yuv_converter(d3d11_device);
	if (rgb_to_yuv_converter.Init(argb_image.width, argb_image.height)) {
		printf("init device converter !!! \n");
	}

	D3D11QSVEncoder yuv420_encoder(d3d11_device);
	yuv420_encoder.SetOption(QSV_OPTION_WIDTH, argb_image.width);
	yuv420_encoder.SetOption(QSV_OPTION_HEIGHT, argb_image.height);
	if (yuv420_encoder.Init()) {
		printf("init encoder succeeded !!! \n");
	}

	D3D11QSVEncoder chromat420_encoder(d3d11_device);
	chromat420_encoder.SetOption(QSV_OPTION_WIDTH, argb_image.width);
	chromat420_encoder.SetOption(QSV_OPTION_HEIGHT, argb_image.height);
	if (chromat420_encoder.Init()) {
		printf("init encoder succeeded !!! \n");
	}

	while (1) {
		std::vector<uint8_t> yuv_frame;
		std::vector<uint8_t> chroma_frame;

		if (screen_capture.Capture(argb_image)) {
			ID3D11Texture2D* argb_texture = NULL;

			HRESULT hr = d3d11_device->OpenSharedResource(argb_image.shared_handle,__uuidof(ID3D11Texture2D), (void**)(&argb_texture));
			if (FAILED(hr)) {
				break;
			}

			rgb_to_yuv_converter.Convert(argb_texture);
			yuv420_encoder.Encode(rgb_to_yuv_converter.GetYUV420Texture(), yuv_frame);
			chromat420_encoder.Encode(rgb_to_yuv_converter.GetChroma420Texture(), chroma_frame);
			argb_texture->Release();
		}
		Sleep(100);
	}

	return 0;
}