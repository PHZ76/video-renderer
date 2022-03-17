#include "main_window.h"
#include "video_source.h"
#include "video_sink.h"
#include <chrono>

int main(int argc, char** argv)
{
	int display_mode = 0; // [0:rgb, 1:yuv420, 2:yuv420+chroma420]

	MainWindow window;
	if (!window.Init(600, 600, 3440, 1440)) {
		return -1;
	}

	window.SetMessageCallback([&display_mode](UINT msg, WPARAM wp, LPARAM lp, LRESULT* result) {
		if (msg == WM_KEYDOWN && wp == VK_SPACE) {
			display_mode += 1;
			display_mode %= 3;
			printf("display mode: %d \n", display_mode);
		}
	});

	VideoSource video_source;
	if (!video_source.Init()) {
		return -2;
	}

	VideoSink video_sink;
	if (!video_sink.Init(window.GetHandle(), video_source.GetWidth(), video_source.GetHeight())) {
		return -3;
	}

	MSG msg;
	ZeroMemory(&msg, sizeof(msg));

	RECT rect;
	GetWindowRect(window.GetHandle(), &rect);

	auto start_time = std::chrono::high_resolution_clock::now();

	while (msg.message != WM_QUIT) {
		if (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
			continue;
		}
		else {
			RECT rect_;
			GetWindowRect(window.GetHandle(), &rect_);
			if (((rect_.right - rect_.left) != (rect.right - rect.left)) ||
				((rect_.bottom - rect_.top) != (rect.bottom - rect.top))) {
				memcpy(&rect, &rect_, sizeof(RECT));
				video_sink.Resize();
			}

			auto cur_time = std::chrono::high_resolution_clock::now();
			auto elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(cur_time - start_time).count();
			if (elapsed_time >= 33) {
				start_time = cur_time;
				if (display_mode == 0) {
					DX::Image image;
					if (video_source.Capture(image)) {
						video_sink.RenderFrame(image);
					}
				}
				else {
					std::vector<std::vector<uint8_t>> compressed_frame;
					if (video_source.Capture(compressed_frame)) {
						if (display_mode == 1) {
							video_sink.RenderNV12(compressed_frame);
						}
						if (display_mode == 2) {
							video_sink.RenderARGB(compressed_frame);
						}
					}
				}
			}
		}
		Sleep(1);
	}

	return 0;
}
