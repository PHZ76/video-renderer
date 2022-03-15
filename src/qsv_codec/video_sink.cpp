#include "video_sink.h"

VideoSink::VideoSink()
{

}

VideoSink::~VideoSink()
{
	Destroy();
}

bool VideoSink::Init(HWND hwnd, int width, int height)
{
	if (!DX::D3D11Renderer::Init(hwnd)) {
		printf("[VideoSink] Init dx11 renderer failed.");
		return false;
	}

	yuv420_decoder_ = std::make_shared<D3D11VADecoder>(d3d11_device_);
	yuv420_decoder_->SetOption(AV_DECODER_OPTION_WIDTH, width);
	yuv420_decoder_->SetOption(AV_DECODER_OPTION_HEIGHT, height);
	if (!yuv420_decoder_->Init()) {
		printf("[VideoSink] Init yuv420 decoder failed.");
		DX::D3D11Renderer::Destroy();
		return false;
	}

	chroma420_decoder_ = std::make_shared<D3D11VADecoder>(d3d11_device_);
	chroma420_decoder_->SetOption(AV_DECODER_OPTION_WIDTH, width);
	chroma420_decoder_->SetOption(AV_DECODER_OPTION_HEIGHT, height);
	if (!chroma420_decoder_->Init()) {
		printf("[VideoSink] Init chroma420 decoder failed.");
		DX::D3D11Renderer::Destroy();
		return false;
	}

	return true;
}

void VideoSink::Destroy()
{
	if (yuv420_decoder_) {
		yuv420_decoder_->Destroy();
	}

	if (chroma420_decoder_) {
		chroma420_decoder_->Destroy();
	}

	DX::D3D11Renderer::Destroy();
}

void VideoSink::RenderFrame(DX::Image image)
{
	ID3D11Texture2D* texture = nullptr;

	HRESULT hr = d3d11_device_->OpenSharedResource((HANDLE)(uintptr_t)image.shared_handle,
		__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&texture));
	if (FAILED(hr)) {
		return;
	}

	D3D11_TEXTURE2D_DESC desc;
	texture->GetDesc(&desc);

	if (pixel_format_ != DX::PIXEL_FORMAT_ARGB ||
		width_ != desc.Width || height_ != desc.Height) {
		if (!CreateTexture(desc.Width, desc.Height, DX::PIXEL_FORMAT_ARGB)) {
			return;
		}
	}

	Begin();

	ID3D11Texture2D* argb_texture = input_textures_[DX::PIXEL_PLANE_ARGB]->GetTexture();
	ID3D11ShaderResourceView* argb_texture_svr = input_textures_[DX::PIXEL_PLANE_ARGB]->GetShaderResourceView();

	d3d11_context_->CopySubresourceRegion(
		argb_texture,
		0,
		0,
		0,
		0,
		(ID3D11Resource*)texture,
		0,
		NULL);

	DX::D3D11RenderTexture* render_target = render_targets_[DX::PIXEL_SHADER_ARGB].get();
	if (render_target) {
		render_target->Begin();
		render_target->PSSetTexture(0, argb_texture_svr);
		render_target->PSSetSamplers(0, linear_sampler_);
		render_target->PSSetSamplers(1, point_sampler_);
		render_target->Draw();
		render_target->End();
		render_target->PSSetTexture(0, NULL);
		output_texture_ = render_target;
	}

	End();
}

void VideoSink::RenderFrame(std::vector<uint8_t> yuv420_frame)
{
	if (yuv420_frame.empty()) {
		return;
	}

	int ret = yuv420_decoder_->Send(yuv420_frame);
	if (ret < 0) {
		printf("[VideoSink] Send yuv420 frame failed. \n");
		return;
	}

	std::shared_ptr<AVFrame> frame;
	ret = yuv420_decoder_->Recv(frame);
	if (ret < 0) {
		printf("[VideoSink] Recv yuv420 frame failed. \n");
		return;
	}
}

void VideoSink::RenderFrame(std::vector<uint8_t> yuv420_frame, std::vector<uint8_t> chroma420_frame)
{

}
