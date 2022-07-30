/* Copyright (c) 2022, Thijs Waalen
 *
 * SPDX-License-Identifier: ISC
 */

#include "engine/backend/vulkan/renderer.h"
#include "engine/backend/vulkan/formatters.h"
#include "engine/platform/windows/Window.h"

#include <format>
#include <iostream>

namespace engine
{
	bool renderer_vulkan::create_window_surface(window const& window)
	{
		VkWin32SurfaceCreateInfoKHR surface_create_info{};
		surface_create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		surface_create_info.hwnd = ((window_win32 const&)window).window_handle();
		surface_create_info.hinstance = GetModuleHandle(nullptr);

		auto result = vkCreateWin32SurfaceKHR(m_instance, &surface_create_info, nullptr, &m_window_surface);
		if (result != VK_SUCCESS)
		{
			std::cout << std::format("Failed creating window surface: {}", result) << std::endl;
			return false;
		}

		return true;
	}
}
