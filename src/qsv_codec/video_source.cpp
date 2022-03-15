#include "video_source.h"

VideoSource::VideoSource()
{

}

VideoSource::~VideoSource()
{
	Destroy();
}

bool VideoSource::Init()
{
	screen_capture_ = std::make_shared<DX::D3D11ScreenCapture>();
	if (!screen_capture_->Init()) {
		printf("[VideoSource] Init screen capture failed. \n");
		return false;
	}

	qsv_device_ = std::make_shared<D3D11QSVDevice>();
	if (!qsv_device_->Init()) {
		printf("[VideoSource] Init qsv device failed. \n");
		return false;
	}

	DX::Image image;
	if (!screen_capture_->Capture(image)) {
		printf("[VideoSource] Capture image failed. \n");
		return false;
	}

	video_width_ = image.width;
	video_height_ = image.height;
	ID3D11Device* d3d11_device = qsv_device_->GetD3D11Device();

	yuv420_encoder_ = std::make_shared<D3D11QSVEncoder>(d3d11_device);
	yuv420_encoder_->SetOption(QSV_ENCODER_OPTION_WIDTH, video_width_);
	yuv420_encoder_->SetOption(QSV_ENCODER_OPTION_HEIGHT, video_height_);
	if (!yuv420_encoder_->Init()) {
		printf("Init yuv420 encoder failed. \n");
		return false;
	}

	chroma420_encoder_ = std::make_shared<D3D11QSVEncoder>(d3d11_device);
	chroma420_encoder_->SetOption(QSV_ENCODER_OPTION_WIDTH, video_width_);
	chroma420_encoder_->SetOption(QSV_ENCODER_OPTION_HEIGHT, video_height_);
	if (!chroma420_encoder_->Init()) {
		printf("Init chroma encoder failed. \n");
		return false;
	}

	color_converter_ = std::make_shared<DX::D3D11RGBToYUVConverter>(d3d11_device);
	if (!color_converter_->Init(video_width_, video_height_)) {
		printf("init color converter failed. \n");
		return false;
	}

	return true;
}

void VideoSource::Destroy()
{
	if (screen_capture_) {
		screen_capture_->Destroy();
	}

	if (color_converter_) {
		color_converter_->Destroy();
	}

	if (yuv420_encoder_) {
		yuv420_encoder_->Destroy();
	}

	if (chroma420_encoder_) {
		chroma420_encoder_->Destroy();
	}

	if (qsv_device_) {
		qsv_device_->Destroy();
	}
}

bool VideoSource::Capture(DX::Image& screen_frame)
{
	if (!screen_capture_->Capture(screen_frame)) {
		printf("[VideoSource] Capture image failed. \n");
		return false;
	}
	return true;
}

bool VideoSource::Capture(std::vector<std::vector<uint8_t>>& compressed_frame)
{
	DX::Image image;

	if (!screen_capture_->Capture(image)) {
		printf("[VideoSource] Capture image failed. \n");
		return false;
	}
	
	ID3D11Device* d3d11_device = qsv_device_->GetD3D11Device();
	ID3D11Texture2D* argb_texture = NULL;

	HRESULT hr = d3d11_device->OpenSharedResource(image.shared_handle, __uuidof(ID3D11Texture2D), (void**)(&argb_texture));
	if (FAILED(hr)) {
		printf("[VideoSource] Open shared handle failed. \n");
		return false;
	}

	if (!color_converter_->Convert(argb_texture)) {
		printf("[VideoSource] Convert image failed. \n");
		argb_texture->Release();
		return false;
	}
	argb_texture->Release();

	std::vector<uint8_t> yuv420_frame;
	std::vector<uint8_t> chroma420_frame;
	int frame_size = 0;

	frame_size = yuv420_encoder_->Encode(color_converter_->GetYUV420Texture(), yuv420_frame);
	if (frame_size < 0) {
		printf("[VideoSource] YUV420 Encoder encode failed. \n");
		return false;
	}

	frame_size = chroma420_encoder_->Encode(color_converter_->GetChroma420Texture(), chroma420_frame);
	if (frame_size < 0) {
		printf("[VideoSource] Chroma420 Encoder encode failed. \n");
		return false;
	}

	compressed_frame.clear();
	compressed_frame.push_back(yuv420_frame);
	compressed_frame.push_back(chroma420_frame);

	return true;
}

int VideoSource::GetWidth()
{
	return video_width_;
}

int VideoSource::GetHeight()
{
	return video_height_;
}