#include "d3d11_qsv_device.h"
#include "d3d11_qsv_encoder.h"
#include "d3d11_screen_capture.h"
#include "d3d11_rgb_to_yuv_converter.h"
#include "d3d11_yuv_to_rgb_converter.h"
#include <cstdio>

int main(int argc, char** argv)
{
	DX::D3D11ScreenCapture screen_capture;
	if (!screen_capture.Init()) {
		printf("init screen capture failed. \n");
		return -1;
	}

	DX::Image image;
	screen_capture.Capture(image);

	D3D11QSVDevice device;
	if (!device.Init()) {
		printf("init qsv device failed. \n");
		return -2;
	}

	ID3D11Device* d3d11_device = device.GetD3D11Device();

	D3D11QSVEncoder yuv420_encoder(d3d11_device);
	yuv420_encoder.SetOption(QSV_OPTION_WIDTH, image.width);
	yuv420_encoder.SetOption(QSV_OPTION_HEIGHT, image.height);
	if (!yuv420_encoder.Init()) {
		printf("init yuv420 encoder failed. \n");
		return -3;
	}

	D3D11QSVEncoder chromat420_encoder(d3d11_device);
	chromat420_encoder.SetOption(QSV_OPTION_WIDTH, image.width);
	chromat420_encoder.SetOption(QSV_OPTION_HEIGHT, image.height);
	if (!chromat420_encoder.Init()) {
		printf("init chroma encoder failed. \n");
		return -4;
	}

	DX::D3D11RGBToYUVConverter rgb_to_yuv_converter(d3d11_device);
	if (!rgb_to_yuv_converter.Init(image.width, image.height)) {
		printf("init rgb_to_yuv converter failed. \n");
		return -5;
	}

	DX::D3D11YUVToRGBConverter yuv_to_rgb_converter(d3d11_device);
	if (!rgb_to_yuv_converter.Init(image.width, image.height)) {
		printf("init yuv_to_rgb converter failed. \n");
		return -6;
	}

	while (1) {
		std::vector<uint8_t> yuv_frame;
		std::vector<uint8_t> chroma_frame;
		int frame_size = 0;

		if (screen_capture.Capture(image)) {
			ID3D11Texture2D* argb_texture = NULL;
			HRESULT hr = d3d11_device->OpenSharedResource(image.shared_handle,__uuidof(ID3D11Texture2D), (void**)(&argb_texture));
			if (FAILED(hr)) {
				break;
			}
			
			if (!rgb_to_yuv_converter.Convert(argb_texture)) {
				printf("convert rgb to yuv failed. \n");
				argb_texture->Release();
				break;
			}
			
			argb_texture->Release();

			ID3D11Texture2D* yuv420_texture = rgb_to_yuv_converter.GetYUV420Texture();
			frame_size = yuv420_encoder.Encode(yuv420_texture, yuv_frame);
			if (frame_size < 0) {
				printf("yuv420_encoder encode failed. \n");
				break;
			}

			frame_size = chromat420_encoder.Encode(rgb_to_yuv_converter.GetChroma420Texture(), chroma_frame);
			if (frame_size < 0) {
				printf("chromat420_encoder encode failed. \n");
				break;
			}
		}
		Sleep(100);
	}

	return 0;
}