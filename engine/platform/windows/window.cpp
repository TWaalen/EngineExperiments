/* Copyright (c) 2022, Thijs Waalen
 *
 * SPDX-License-Identifier: ISC
 */

#include "engine/platform/windows/Window.h"

#include "engine/platform/windows/Utils.h"

#include <iostream>

namespace engine
{
	LRESULT CALLBACK window_procedure(HWND, UINT, WPARAM, LPARAM);

	std::unique_ptr<window> window::create(const wchar_t* title, int width, int height)
	{
		const wchar_t* class_name = L"VulkanEngineWindowClass";

		WNDCLASS window_class = {};
		window_class.lpfnWndProc = window_procedure;
		window_class.hInstance = GetModuleHandle(nullptr);
		window_class.lpszClassName = class_name;
		auto class_atom = RegisterClass(&window_class);
		if (class_atom == 0)
		{
			std::wcerr << L"Error creating window class: " << error_message_from_win32_error_code(GetLastError())
					   << std::endl;
			return nullptr;
		}

		HWND window_handle = CreateWindowEx(0, class_name, title, WS_OVERLAPPEDWINDOW,
											CW_USEDEFAULT, CW_USEDEFAULT, width, height, nullptr,
											nullptr, GetModuleHandle(nullptr), nullptr);
		if (window_handle == nullptr)
		{
			std::wcerr << L"Error creating window: " << error_message_from_win32_error_code(GetLastError())
					   << std::endl;
			return nullptr;
		}

		auto window = std::make_unique<window_win32>(window_handle);
		SetLastError(0);
		DWORD win32_error_code;
		if (SetWindowLongPtr(window_handle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window.get())) == 0 &&
			(win32_error_code = GetLastError()) != 0)
		{
			std::wcerr << L"Error setting window user data: " << error_message_from_win32_error_code(GetLastError())
					   << std::endl;
			return nullptr;
		}

		ShowWindow(window_handle, SW_SHOW);

		return window;
	}

	void window_win32::update()
	{
		MSG message;
		BOOL result;
		while ((result = PeekMessage(&message, m_window_handle, 0, 0, PM_REMOVE)) != 0)
		{
			if (result == -1)
			{
				std::wcerr << L"Error getting message for window: " << error_message_from_win32_error_code(GetLastError())
						   << std::endl;
				m_should_close = true;
				return;
			}

			TranslateMessage(&message);
			DispatchMessage(&message);
		}
	}

	uint32_t window_win32::width() const
	{
		RECT client_rect;
		if (GetClientRect(m_window_handle, &client_rect) == 0)
			return 0;

		return client_rect.right - client_rect.left;
	}

	uint32_t window_win32::height() const
	{
		RECT client_rect;
		if (GetClientRect(m_window_handle, &client_rect) == 0)
			return 0;

		return client_rect.bottom - client_rect.top;
	}

	LRESULT CALLBACK window_procedure(HWND window_handle, UINT message, WPARAM w_param, LPARAM l_param)
	{
		auto* window = reinterpret_cast<window_win32*>(GetWindowLongPtr(window_handle, GWLP_USERDATA));
		if (window != nullptr)
		{
			switch (message)
			{
			case WM_DESTROY:
				DestroyWindow(window_handle);
				PostQuitMessage(0);
				return 0;
			case WM_CLOSE:
			case WM_QUIT:
				window->m_should_close = true;
				return 0;
			case WM_PAINT:
				ValidateRect(window_handle, nullptr);
				return 0;
			}
		}
		else if (message != WM_NCCREATE)
			std::wcerr << std::format(L"No available window for message {}", message) << std::endl;

		return DefWindowProc(window_handle, message, w_param, l_param);
	}
}
