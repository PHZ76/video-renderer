#include "main_window.h"

#include <cstdio>

int main(int argc, char** argv)
{
	MainWindow window;
	if (!window.Init(500, 500, 1920 , 1080)) {
		return -1;
	}

	MSG msg;
	ZeroMemory(&msg, sizeof(msg));

	RECT rect;
	GetWindowRect(window.GetHandle(), &rect);

	while (msg.message != WM_QUIT) {
		if (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
			continue;
		}
		else {

		}
	}

	return 0;
}