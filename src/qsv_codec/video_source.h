#pragma once

#include "d3d11_screen_capture.h"
#include "d3d11_qsv_device.h"
#include "d3d11_qsv_encoder.h"
#include "d3d11_rgb_to_yuv_converter.h"
#include <memory>
#include <vector>

class VideoSource
{
public:
	VideoSource();
	virtual ~VideoSource();

	bool Init();
	void Destroy();

	bool Capture(DX::Image& image);
	bool Capture(std::vector<std::vector<uint8_t>>& compressed_frame);

private:
	std::shared_ptr<DX::ScreenCapture> screen_capture_;

	std::shared_ptr<DX::D3D11RGBToYUVConverter> color_converter_;

	std::shared_ptr<D3D11QSVDevice>  qsv_device_;
	std::shared_ptr<D3D11QSVEncoder> yuv420_encoder_;
	std::shared_ptr<D3D11QSVEncoder> chroma420_encoder_;
};