#include "main_window.h"

ATOM MainWindow::wnd_class_ = 0;
const wchar_t MainWindow::kClassName[] = L"VideoRender_MainWindow";

MainWindow::MainWindow()
{

}

MainWindow::~MainWindow()
{
	Destroy();
}

bool MainWindow::Init(int pos_x, int pos_y, int width, int height)
{
	if (!RegisterWindowClass()) {
		return false;
	}
		
	if (IsWindow()) {
		return true;
	}

	wnd_ = ::CreateWindowExW(WS_EX_OVERLAPPEDWINDOW, kClassName, L"video-renderer", 
		WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN,
		pos_x, pos_y, width, height,
		NULL, NULL, GetModuleHandle(NULL), this);

	return wnd_ != NULL;
}

void MainWindow::Destroy()
{
	if (IsWindow()) {
		::DestroyWindow(wnd_);
		wnd_ = NULL;
	}
}

bool MainWindow::IsWindow() 
{
	return wnd_ && ::IsWindow(wnd_) != FALSE;
}

void MainWindow::SetMessageCallback(const MessageCallback& message_callback)
{
	message_callback_ = message_callback;
}

HWND MainWindow::GetHandle()
{
	return wnd_;
}

bool MainWindow::OnMessage(UINT msg, WPARAM wp, LPARAM lp, LRESULT* result)
{
	switch (msg)
	{
	case WM_SIZE:
		//resize
		return true;
	case WM_DESTROY:
		::PostQuitMessage(0);
		return true;
	}

	return false;
}

LRESULT CALLBACK MainWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	MainWindow* windows = reinterpret_cast<MainWindow*>(::GetWindowLongPtr(hwnd, GWLP_USERDATA));
	if (!windows && WM_CREATE == msg) {
		CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lp);
		windows = reinterpret_cast<MainWindow*>(cs->lpCreateParams);
		windows->wnd_ = hwnd;
		::SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(windows));
	}

	LRESULT result = 0;
	if (windows) {
		bool handled = windows->OnMessage(msg, wp, lp, &result);
		if (!handled) {
			result = ::DefWindowProc(hwnd, msg, wp, lp);
		}
	}
	else {
		result = ::DefWindowProc(hwnd, msg, wp, lp);
	}

	return result;
}

bool MainWindow::RegisterWindowClass()
{
	if (wnd_class_) {
		return true;
	}
	
	WNDCLASSEXW wcex = { sizeof(WNDCLASSEX) };
	wcex.style = CS_DBLCLKS;
	wcex.hInstance = GetModuleHandle(NULL);
	wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
	wcex.hCursor = ::LoadCursor(NULL, IDC_ARROW);
	wcex.lpfnWndProc = &WndProc;
	wcex.lpszClassName = kClassName;
	wnd_class_ = ::RegisterClassExW(&wcex);
	return wnd_class_ != 0;
}
