/* Copyright (c) 2022, Thijs Waalen
 *
 * SPDX-License-Identifier: ISC
 */

#pragma once

#include "datastructures/optional.h"
#include "datastructures/fixed_vector.h"
#include "datastructures/vector.h"

#include <memory>

#include <volk/volk.h>

namespace engine
{
	class window;
	class renderer_vulkan
	{
	public:
		enum class debug_output
		{
			enabled,
			disabled
		};

		static std::unique_ptr<renderer_vulkan> create_with_window(window const&, debug_output debugOutput = debug_output::enabled);

		renderer_vulkan(VkInstance instance) : m_instance(instance) {}
		~renderer_vulkan();

		void render();

	private:
		struct queue_family_indices
		{
			datastructures::optional<uint32_t> graphics;
			datastructures::optional<uint32_t> present;

			bool is_complete() const
			{
				return graphics.has_value() && present.has_value();
			}
		};

		struct queues
		{
			VkQueue graphics;
			VkQueue present;
		};

		bool create_command_buffer();
		bool create_command_pool();
		void create_debug_messenger();
		bool create_framebuffers();
		bool create_graphics_pipeline();
		bool create_logical_device(debug_output);
		bool create_render_pass();
		VkShaderModule create_shader_module(datastructures::fixed_vector<char> const& code);
		bool create_swapchain(window const&);
		bool create_synchronization_objects();
		bool create_window_surface(window const&);
		queue_family_indices find_queue_families(VkPhysicalDevice);
		VkPhysicalDevice pick_physical_device();
		int rate_device_suitability(VkPhysicalDevice);
		bool record_command_buffer(VkCommandBuffer, uint32_t image_index);

		VkInstance m_instance = VK_NULL_HANDLE;
		VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
		VkDevice m_device = VK_NULL_HANDLE;
		VolkDeviceTable m_device_functions{};
		VkSurfaceKHR m_window_surface = VK_NULL_HANDLE;
		VkRenderPass m_render_pass = VK_NULL_HANDLE;
		VkPipelineLayout m_pipeline_layout = VK_NULL_HANDLE;
		VkPipeline m_graphics_pipeline = VK_NULL_HANDLE;

		VkCommandPool m_command_pool = VK_NULL_HANDLE;
		VkCommandBuffer m_command_buffer = VK_NULL_HANDLE;

		VkSemaphore m_image_available_semaphore = VK_NULL_HANDLE;
		VkSemaphore m_render_finished_semaphore = VK_NULL_HANDLE;
		VkFence m_in_flight_fence = VK_NULL_HANDLE;

		VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
		VkFormat m_swapchain_image_format;
		VkExtent2D m_swapchain_extent;
		datastructures::vector<VkImage> m_swapchain_images;
		datastructures::vector<VkImageView> m_swapchain_image_views;
		datastructures::vector<VkFramebuffer> m_swapchain_framebuffers;

		queues m_queues{};

		VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
	};
}
