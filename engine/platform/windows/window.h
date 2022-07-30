/* Copyright (c) 2022, Thijs Waalen
 *
 * SPDX-License-Identifier: ISC
 */

#pragma once

#include "engine/Window.h"

#include <Windows.h>

namespace engine
{
	class window_win32 final : public window
	{
	public:
		explicit window_win32(HWND windowHandle) : window(), m_window_handle(windowHandle) {}

		void update() override;

		uint32_t width() const override;
		uint32_t height() const override;

		HWND window_handle() const { return m_window_handle; }

		friend LRESULT CALLBACK window_procedure(HWND, UINT message, WPARAM, LPARAM);

	private:
		HWND m_window_handle;
	};
}
