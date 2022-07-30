/* Copyright (c) 2022, Thijs Waalen
 *
 * SPDX-License-Identifier: ISC
 */

#include "datastructures/fixed_vector.h"
#include "engine/backend/vulkan/renderer.h"
#include "engine/utils.h"
#include "engine/window.h"

#include <format>
#include <iostream>

using datastructures::fixed_vector;
using engine::renderer_vulkan;
using engine::window;

#if defined(_DEBUG)
const renderer_vulkan::debug_output debug_renderer = renderer_vulkan::debug_output::enabled;
#else
const renderer_vulkan::debug_output debug_renderer = renderer_vulkan::debug_output::disabled;
#endif

int main()
{
	engine::create_console();

	auto window = window::create(L"Vulkan window", 800, 600);
	if (window == nullptr)
		return -1;

	auto renderer = renderer_vulkan::create_with_window(*window.get(), debug_renderer);
	if (renderer == nullptr)
		return -1;

	while (!window->should_close())
	{
		window->update();
		renderer->render();
	}

	return 0;
}
