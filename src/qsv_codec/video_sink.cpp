#include "video_sink.h"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#define ENABLE_IMGUI 0

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

	color_converter_ = std::make_shared<DX::D3D11YUVToRGBConverter>(d3d11_device_);
	if (!color_converter_->Init(width, height)) {
		printf("[VideoSink] Init color converter failed.");
		goto failed;
	}

	yuv420_decoder_ = std::make_shared<D3D11VADecoder>(d3d11_device_);
	yuv420_decoder_->SetOption(AV_DECODER_OPTION_WIDTH, width);
	yuv420_decoder_->SetOption(AV_DECODER_OPTION_HEIGHT, height);
	if (!yuv420_decoder_->Init()) {
		printf("[VideoSink] Init yuv420 decoder failed.");
		goto failed;
	}

	chroma420_decoder_ = std::make_shared<D3D11VADecoder>(d3d11_device_);
	chroma420_decoder_->SetOption(AV_DECODER_OPTION_WIDTH, width);
	chroma420_decoder_->SetOption(AV_DECODER_OPTION_HEIGHT, height);
	if (!chroma420_decoder_->Init()) {
		printf("[VideoSink] Init chroma420 decoder failed.");
		goto failed;
	}

#if ENABLE_IMGUI
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX11_Init(d3d11_device_, d3d11_context_);
#endif

	return true;

failed:
	DX::D3D11Renderer::Destroy();
	return false;
}

void VideoSink::Destroy()
{
#if ENABLE_IMGUI
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
#endif

	if (yuv420_decoder_) {
		yuv420_decoder_->Destroy();
	}

	if (chroma420_decoder_) {
		chroma420_decoder_->Destroy();
	}

	DX::D3D11Renderer::Destroy();
}

void VideoSink::End()
{
	if (output_texture_) {
		ID3D11Texture2D* texture = output_texture_->GetTexture();
		ID3D11Texture2D* back_buffer = NULL;
		HRESULT hr = dxgi_swap_chain_->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&back_buffer));
		if (SUCCEEDED(hr)) {
			d3d11_context_->CopyResource(back_buffer, texture);
			back_buffer->Release();
		}
		output_texture_ = NULL;
	}

#if ENABLE_IMGUI
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	bool show_demo_window = true;
	bool show_another_window = false;
	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
	static float f = 0.0f;
	static int counter = 0;

	if (show_demo_window)
		ImGui::ShowDemoWindow(&show_demo_window);

	ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

	ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
	ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
	ImGui::Checkbox("Another Window", &show_another_window);

	ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
	ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

	if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
		counter++;
	ImGui::SameLine();
	ImGui::Text("counter = %d", counter);

	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
	ImGui::End();

	if (show_another_window) {
		ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
		ImGui::Text("Hello from another window!");
		if (ImGui::Button("Close Me"))
			show_another_window = false;
		ImGui::End();
	}

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
#endif

	HRESULT hr = dxgi_swap_chain_->Present(0, 0);
	if (FAILED(hr) && hr != DXGI_ERROR_WAS_STILL_DRAWING) {
		if (hr == DXGI_ERROR_DEVICE_REMOVED) {
			DX::D3D11Renderer::Destroy();
			DX::D3D11Renderer::Init(wnd_);
		}
		else {
			return;
		}
	}
}

void VideoSink::RenderFrame(DX::Image& image)
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

void VideoSink::RenderNV12(std::vector<std::vector<uint8_t>>& compressed_frame)
{
	if (compressed_frame.size() != 2) {
		return;
	}

	if (compressed_frame[0].empty() ||
		compressed_frame[1].empty()) {
		return;
	}

	std::shared_ptr<AVFrame> yuv420_frame, chroma420_frame;

	int ret = yuv420_decoder_->Send(compressed_frame[0]);
	if (ret < 0) {
		printf("[VideoSink] Send yuv420 frame failed. \n");
		return;
	}

	ret = yuv420_decoder_->Recv(yuv420_frame);
	if (ret < 0) {
		printf("[VideoSink] Recv yuv420 frame failed. \n");
		return;
	}

	ret = chroma420_decoder_->Send(compressed_frame[1]);
	if (ret < 0) {
		printf("[VideoSink] Send chroma420 frame failed. \n");
		return;
	}

	ret = chroma420_decoder_->Recv(chroma420_frame);
	if (ret < 0) {
		printf("[VideoSink] Recv chroma420 frame failed. \n");
		return;
	}

	ID3D11Texture2D* yuv420_texture = (ID3D11Texture2D*)yuv420_frame->data[0];
	int yuv420_index = (int)yuv420_frame->data[1];

	D3D11_TEXTURE2D_DESC desc;
	yuv420_texture->GetDesc(&desc);

	if (pixel_format_ != DX::PIXEL_FORMAT_NV12 ||
		width_ != desc.Width || height_ != desc.Height) {
		if (!CreateTexture(desc.Width, desc.Height, DX::PIXEL_FORMAT_NV12)) {			
			return;
		}
	}

	Begin();

	auto input_texture = input_textures_[DX::PIXEL_PLANE_NV12];
	ID3D11Texture2D* nv12_texture = input_texture->GetTexture();
	ID3D11ShaderResourceView* nv12_texture_y_srv = input_texture->GetNV12YShaderResourceView();
	ID3D11ShaderResourceView* nv12_texture_uv_srv = input_texture->GetNV12UVShaderResourceView();

	d3d11_context_->CopySubresourceRegion(
		nv12_texture,
		0,
		0,
		0,
		0,
		yuv420_texture,
		yuv420_index,
		NULL);

	auto render_target = render_targets_[DX::PIXEL_SHADER_NV12_BT601];
	if (render_target) {
		render_target->Begin();
		render_target->PSSetTexture(0, nv12_texture_y_srv);
		render_target->PSSetTexture(1, nv12_texture_uv_srv);
		render_target->PSSetSamplers(0, linear_sampler_);
		render_target->PSSetSamplers(1, point_sampler_);
		render_target->Draw();
		render_target->End();
		render_target->PSSetTexture(0, NULL);
		render_target->PSSetTexture(1, NULL);
		output_texture_ = render_target.get();
	}

	Process();
	End();

}

void VideoSink::RenderARGB(std::vector<std::vector<uint8_t>>& compressed_frame)
{
	if (compressed_frame.size() != 2) {
		return;
	}

	if (compressed_frame[0].empty() || 
		compressed_frame[1].empty()) {
		return;
	}

	std::shared_ptr<AVFrame> yuv420_frame, chroma420_frame;

	int ret = yuv420_decoder_->Send(compressed_frame[0]);
	if (ret < 0) {
		printf("[VideoSink] Send yuv420 frame failed. \n");
		return;
	}

	ret = yuv420_decoder_->Recv(yuv420_frame);
	if (ret < 0) {
		printf("[VideoSink] Recv yuv420 frame failed. \n");
		return;
	}

	ret = chroma420_decoder_->Send(compressed_frame[1]);
	if (ret < 0) {
		printf("[VideoSink] Send chroma420 frame failed. \n");
		return;
	}

	ret = chroma420_decoder_->Recv(chroma420_frame);
	if (ret < 0) {
		printf("[VideoSink] Recv chroma420 frame failed. \n");
		return;
	}

	ID3D11Texture2D* yuv420_texture = (ID3D11Texture2D*)yuv420_frame->data[0];
	int yuv420_index = (int)yuv420_frame->data[1];

	ID3D11Texture2D* chroma420_texture = (ID3D11Texture2D*)chroma420_frame->data[0];
	int chroma420_index = (int)chroma420_frame->data[1];

	if (!color_converter_->Combine(yuv420_texture, yuv420_index, 
		chroma420_texture, chroma420_index)) {
		printf("[VideoSink] Combine frame failed. \n");
		return;
	}

	ID3D11Texture2D* combine_texture = color_converter_->GetRGBATexture();
	D3D11_TEXTURE2D_DESC desc;
	combine_texture->GetDesc(&desc);

	if (pixel_format_ != DX::PIXEL_FORMAT_ARGB ||
		width_ != desc.Width || height_ != desc.Height) {
		if (!CreateTexture(desc.Width, desc.Height, DX::PIXEL_FORMAT_ARGB)) {			
			return;
		}
	}

	Begin();

	auto input_texture = input_textures_[DX::PIXEL_PLANE_ARGB];
	ID3D11Texture2D* argb_texture = input_texture->GetTexture();
	ID3D11ShaderResourceView* argb_texture_svr = input_texture->GetShaderResourceView();

	d3d11_context_->CopySubresourceRegion(
		argb_texture,
		0,
		0,
		0,
		0,
		combine_texture,
		0,
		NULL);

	auto render_target = render_targets_[DX::PIXEL_SHADER_ARGB];
	if (render_target) {
		render_target->Begin();
		render_target->PSSetTexture(0, argb_texture_svr);
		render_target->PSSetSamplers(0, linear_sampler_);
		render_target->PSSetSamplers(1, point_sampler_);
		render_target->Draw();
		render_target->End();
		render_target->PSSetTexture(0, NULL);
		output_texture_ = render_target.get();
	}

	End();
}
