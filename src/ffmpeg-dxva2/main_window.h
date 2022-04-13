#pragma once

#include <Windows.h>

class DXVA2Renderer;

class MainWindow
{
public:
	MainWindow();
	virtual ~MainWindow();

	bool Init(int pos_x, int pos_y, int width, int height);
	void Destroy();

	bool IsWindow();
	HWND GetHandle();
	void SetRender(DXVA2Renderer* render);

	virtual bool OnMessage(UINT msg, WPARAM wp, LPARAM lp, LRESULT* result);

private:
	static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
	static bool RegisterWindowClass();

	HWND wnd_ = NULL;
	DXVA2Renderer* render = NULL;
	bool first_display = false;

	static ATOM wnd_class_;
	static const wchar_t kClassName[];
};
