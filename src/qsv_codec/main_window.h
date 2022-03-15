#pragma once

#include <Windows.h>
#include <functional>

class MainWindow
{
public:
	typedef std::function<void(UINT msg, WPARAM wp, LPARAM lp, LRESULT* result)> MessageCallback;

	MainWindow();
	virtual ~MainWindow();

	bool Init(int pos_x, int pos_y, int width, int height);
	void Destroy();

	bool IsWindow();
	HWND GetHandle();

	void SetMessageCallback(const MessageCallback& message_callback);

private:
	virtual bool OnMessage(UINT msg, WPARAM wp, LPARAM lp, LRESULT* result);
	static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
	static bool RegisterWindowClass();

	HWND wnd_ = NULL;
	MessageCallback message_callback_;

	static ATOM wnd_class_;
	static const wchar_t kClassName[];
};
