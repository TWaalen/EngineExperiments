/* Copyright (c) 2022, Thijs Waalen
 *
 * SPDX-License-Identifier: ISC
 */

#include "engine/backend/vulkan/renderer.h"

#include "datastructures/fixed_vector.h"
#include "datastructures/vector.h"
#include "engine/backend/vulkan/formatters.h"
#include "engine/window.h"
#include "io/file.h"
#include "math/math.h"

#include <format>
#include <map>
#include <iostream>
#include <set>

#define VOLK_IMPLEMENTATION
#include <volk/volk.h>

using namespace engine;

using datastructures::fixed_vector;
using datastructures::vector;
using io::read_entire_file;
using math::clamp;

struct swapchain_support_details
{
	VkSurfaceCapabilitiesKHR capabilities;
	vector<VkSurfaceFormatKHR> formats;
	vector<VkPresentModeKHR> present_modes;
};

fixed_vector<char const*> REQUIRED_DEVICE_EXTENSION_NAMES{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };

bool check_device_extension_support(VkPhysicalDevice, fixed_vector<char const*> const& extensionNames);
bool check_extension_support(fixed_vector<char const*>& extensionNames);
bool check_layer_support(fixed_vector<char const*>& layerNames);
VkExtent2D choose_surface_extent(VkSurfaceCapabilitiesKHR const&, uint32_t ideal_width, uint32_t ideal_height);
VkPresentModeKHR choose_present_mode(vector<VkPresentModeKHR> const&);
VkSurfaceFormatKHR choose_surface_format(vector<VkSurfaceFormatKHR> const&);
VkInstance create_instance(renderer_vulkan::debug_output);
VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT, VkDebugUtilsMessengerCallbackDataEXT const*, void* userData);
fixed_vector<char const*> get_required_extension_names(renderer_vulkan::debug_output);
swapchain_support_details get_swapchain_support_details(VkPhysicalDevice, VkSurfaceKHR);
void output_vulkan_details();
void output_vulkan_device_details(VkInstance);

std::unique_ptr<renderer_vulkan> renderer_vulkan::create_with_window(window const& window, debug_output debugOutput /* = debug_output::enabled */)
{
	if (volkInitialize() != VK_SUCCESS)
	{
		std::cerr << "Failed to initialize Volk" << std::endl;
		return nullptr;
	}

	bool debugOutputEnabled = debugOutput == debug_output::enabled;
	if (debugOutputEnabled)
		output_vulkan_details();

	auto instance = create_instance(debugOutput);
	if (instance == nullptr)
		return nullptr;

	auto renderer = std::make_unique<renderer_vulkan>(instance);

	if (debugOutputEnabled)
	{
		renderer->create_debug_messenger();

		output_vulkan_device_details(instance);
	}

	if (!renderer->create_window_surface(window))
		return nullptr;

	if (!renderer->create_logical_device(debugOutput))
		return nullptr;

	if (!renderer->create_swapchain(window))
		return nullptr;

	if (!renderer->create_render_pass())
		return nullptr;

	if (!renderer->create_graphics_pipeline())
		return nullptr;

	if (!renderer->create_framebuffers())
		return nullptr;

	if (!renderer->create_command_pool())
		return nullptr;

	if (!renderer->create_command_buffer())
		return nullptr;

	if (!renderer->create_synchronization_objects())
		return nullptr;

	return renderer;
}

renderer_vulkan::~renderer_vulkan()
{
	m_device_functions.vkDeviceWaitIdle(m_device);

	if (m_in_flight_fence != VK_NULL_HANDLE)
		m_device_functions.vkDestroyFence(m_device, m_in_flight_fence, nullptr);

	if (m_render_finished_semaphore != VK_NULL_HANDLE)
		m_device_functions.vkDestroySemaphore(m_device, m_render_finished_semaphore, nullptr);

	if (m_image_available_semaphore != VK_NULL_HANDLE)
		m_device_functions.vkDestroySemaphore(m_device, m_image_available_semaphore, nullptr);

	if (m_command_pool != VK_NULL_HANDLE)
		m_device_functions.vkDestroyCommandPool(m_device, m_command_pool, nullptr);

	for (auto& swapchain_framebuffer : m_swapchain_framebuffers)
	{
		if (swapchain_framebuffer != VK_NULL_HANDLE)
			m_device_functions.vkDestroyFramebuffer(m_device, swapchain_framebuffer, nullptr);
	}

	if (m_graphics_pipeline != VK_NULL_HANDLE)
		m_device_functions.vkDestroyPipeline(m_device, m_graphics_pipeline, nullptr);

	if (m_pipeline_layout != VK_NULL_HANDLE)
		m_device_functions.vkDestroyPipelineLayout(m_device, m_pipeline_layout, nullptr);

	if (m_render_pass != VK_NULL_HANDLE)
		m_device_functions.vkDestroyRenderPass(m_device, m_render_pass, nullptr);

	for (auto& swapchain_image_view : m_swapchain_image_views)
	{
		if (swapchain_image_view != VK_NULL_HANDLE)
			m_device_functions.vkDestroyImageView(m_device, swapchain_image_view, nullptr);
	}

	if (m_swapchain != VK_NULL_HANDLE)
		m_device_functions.vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);

	if (m_device != VK_NULL_HANDLE)
		m_device_functions.vkDestroyDevice(m_device, nullptr);

	if (m_window_surface != VK_NULL_HANDLE)
		vkDestroySurfaceKHR(m_instance, m_window_surface, nullptr);

	if (m_debugMessenger != VK_NULL_HANDLE)
		vkDestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);

	if (m_instance != VK_NULL_HANDLE)
		vkDestroyInstance(m_instance, nullptr);
}

void renderer_vulkan::render()
{
	m_device_functions.vkWaitForFences(m_device, 1, &m_in_flight_fence, VK_TRUE, UINT64_MAX);
	m_device_functions.vkResetFences(m_device, 1, &m_in_flight_fence);

	uint32_t image_index;
	m_device_functions.vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, m_image_available_semaphore,
											 VK_NULL_HANDLE, &image_index);

	m_device_functions.vkResetCommandBuffer(m_command_buffer, 0);
	if (!record_command_buffer(m_command_buffer, image_index))
		return;

	VkPipelineStageFlags wait_stages[]{ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	VkSubmitInfo submit_info{};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.pWaitDstStageMask = wait_stages;
	submit_info.pWaitSemaphores = &m_image_available_semaphore;
	submit_info.waitSemaphoreCount = 1;
	submit_info.pCommandBuffers = &m_command_buffer;
	submit_info.commandBufferCount = 1;
	submit_info.pSignalSemaphores = &m_render_finished_semaphore;
	submit_info.signalSemaphoreCount = 1;

	auto result = m_device_functions.vkQueueSubmit(m_queues.graphics, 1, &submit_info, m_in_flight_fence);
	if (result != VK_SUCCESS)
	{
		std::cerr << std::format("Failed to submit queue: {}", result) << std::endl;
		return;
	}

	VkPresentInfoKHR present_info{};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.pWaitSemaphores = &m_render_finished_semaphore;
	present_info.waitSemaphoreCount = 1;
	present_info.pSwapchains = &m_swapchain;
	present_info.swapchainCount = 1;
	present_info.pImageIndices = &image_index;

	result = m_device_functions.vkQueuePresentKHR(m_queues.present, &present_info);
	if (result != VK_SUCCESS)
	{
		std::cerr << std::format("Failed to present queue: {}", result) << std::endl;
		return;
	}
}

bool renderer_vulkan::create_command_buffer()
{
	VkCommandBufferAllocateInfo allocate_info{};
	allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocate_info.commandPool = m_command_pool;
	allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocate_info.commandBufferCount = 1;

	auto result = m_device_functions.vkAllocateCommandBuffers(m_device, &allocate_info, &m_command_buffer);
	if (result != VK_SUCCESS)
	{
		std::cerr << std::format("Failed to allocate command buffer: {}", result) << std::endl;
		return false;
	}

	return true;
}

bool renderer_vulkan::create_command_pool()
{
	auto queue_family_indices = find_queue_families(m_physical_device);

	VkCommandPoolCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	create_info.queueFamilyIndex = queue_family_indices.graphics.value();

	auto result = m_device_functions.vkCreateCommandPool(m_device, &create_info, nullptr, &m_command_pool);
	if (result != VK_SUCCESS)
	{
		std::cerr << std::format("Failed to create command pool: {}", result) << std::endl;
		return false;
	}

	return true;
}

void renderer_vulkan::create_debug_messenger()
{
	VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo{};
	debugUtilsMessengerCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	debugUtilsMessengerCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
													VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	debugUtilsMessengerCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
												VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
	debugUtilsMessengerCreateInfo.pfnUserCallback = debug_callback;

	vkCreateDebugUtilsMessengerEXT(m_instance, &debugUtilsMessengerCreateInfo, nullptr, &m_debugMessenger);
}

bool renderer_vulkan::create_framebuffers()
{
	m_swapchain_framebuffers.resize(m_swapchain_image_views.size());
	VkFramebufferCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	create_info.renderPass = m_render_pass;
	create_info.attachmentCount = 1;
	create_info.width = m_swapchain_extent.width;
	create_info.height = m_swapchain_extent.height;
	create_info.layers = 1;
	for (size_t i = 0; i < m_swapchain_image_views.size(); ++i)
	{
		create_info.pAttachments = &m_swapchain_image_views[i];

		auto result = m_device_functions.vkCreateFramebuffer(m_device, &create_info, nullptr,
															 &m_swapchain_framebuffers[i]);
		if (result != VK_SUCCESS)
		{
			std::cerr << std::format("Failed to create framebuffer: {}", result) << std::endl;
			return false;
		}
	}

	return true;
}

bool renderer_vulkan::create_graphics_pipeline()
{
	auto vertex_shader_code = read_entire_file("shaders/triangle.vert.spv", io::file_mode::binary);
	if (vertex_shader_code.empty())
		return false;

	auto fragment_shader_code = read_entire_file("shaders/triangle.frag.spv", io::file_mode::binary);
	if (fragment_shader_code.empty())
		return false;

	VkShaderModule vertex_shader = create_shader_module(vertex_shader_code);
	if (vertex_shader == VK_NULL_HANDLE)
		return false;

	VkShaderModule fragment_shader = create_shader_module(fragment_shader_code);
	if (fragment_shader == VK_NULL_HANDLE)
		return false;

	VkPipelineShaderStageCreateInfo vertex_shader_stage_create_info{};
	vertex_shader_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertex_shader_stage_create_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertex_shader_stage_create_info.module = vertex_shader;
	vertex_shader_stage_create_info.pName = "main";

	VkPipelineShaderStageCreateInfo fragment_shader_stage_create_info{};
	fragment_shader_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragment_shader_stage_create_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragment_shader_stage_create_info.module = fragment_shader;
	fragment_shader_stage_create_info.pName = "main";

	VkPipelineShaderStageCreateInfo shader_stages_create_info[]{
		vertex_shader_stage_create_info,
		fragment_shader_stage_create_info
	};

	VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info{};
	vertex_input_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	VkPipelineInputAssemblyStateCreateInfo input_assembly_state_create_info{};
	input_assembly_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_state_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	VkViewport viewport{};
	viewport.width = (float)m_swapchain_extent.width;
	viewport.height = (float)m_swapchain_extent.height;
	viewport.maxDepth = 1.f;

	VkRect2D scissor{};
	scissor.extent = m_swapchain_extent;

	VkPipelineViewportStateCreateInfo viewport_state_create_info{};
	viewport_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state_create_info.pViewports = &viewport;
	viewport_state_create_info.viewportCount = 1;
	viewport_state_create_info.pScissors = &scissor;
	viewport_state_create_info.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo rasterization_state_create_info{};
	rasterization_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization_state_create_info.polygonMode = VK_POLYGON_MODE_FILL;
	rasterization_state_create_info.lineWidth = 1.f;
	rasterization_state_create_info.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterization_state_create_info.frontFace = VK_FRONT_FACE_CLOCKWISE;

	VkPipelineMultisampleStateCreateInfo multisample_state_create_info{};
	multisample_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample_state_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineColorBlendAttachmentState color_blend_attachment_state{};
	color_blend_attachment_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
												  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo color_blend_state_create_info{};
	color_blend_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	color_blend_state_create_info.pAttachments = &color_blend_attachment_state;
	color_blend_state_create_info.attachmentCount = 1;

	VkPipelineLayoutCreateInfo pipeline_layout_create_info{};
	pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

	auto result = m_device_functions.vkCreatePipelineLayout(m_device, &pipeline_layout_create_info, nullptr,
															&m_pipeline_layout);
	if (result != VK_SUCCESS)
	{
		std::cerr << std::format("Failed to create pipeline layout: {}", result) << std::endl;
		return false;
	}

	VkGraphicsPipelineCreateInfo graphics_pipeline_create_info{};
	graphics_pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	graphics_pipeline_create_info.pStages = shader_stages_create_info;
	graphics_pipeline_create_info.stageCount = 2;
	graphics_pipeline_create_info.pVertexInputState = &vertex_input_state_create_info;
	graphics_pipeline_create_info.pInputAssemblyState = &input_assembly_state_create_info;
	graphics_pipeline_create_info.pViewportState = &viewport_state_create_info;
	graphics_pipeline_create_info.pRasterizationState = &rasterization_state_create_info;
	graphics_pipeline_create_info.pMultisampleState = &multisample_state_create_info;
	graphics_pipeline_create_info.pColorBlendState = &color_blend_state_create_info;
	graphics_pipeline_create_info.layout = m_pipeline_layout;
	graphics_pipeline_create_info.renderPass = m_render_pass;

	result = m_device_functions.vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &graphics_pipeline_create_info,
														  nullptr, &m_graphics_pipeline);
	if (result != VK_SUCCESS)
	{
		std::cerr << std::format("Failed to create graphics pipeline: {}", result) << std::endl;
		return false;
	}

	m_device_functions.vkDestroyShaderModule(m_device, vertex_shader, nullptr);
	m_device_functions.vkDestroyShaderModule(m_device, fragment_shader, nullptr);

	return true;
}

bool renderer_vulkan::create_logical_device(debug_output debugOutput)
{
	m_physical_device = pick_physical_device();
	if (m_physical_device == nullptr)
	{
		std::cerr << "Failed to find a suitable GPU" << std::endl;
		return false;
	}

	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(m_physical_device, &deviceProperties);
	std::cout << std::format("Using {} {} ({})", (VkVendorId)deviceProperties.vendorID, deviceProperties.deviceName, deviceProperties.deviceType) << std::endl;

	auto queueFamilyIndices = find_queue_families(m_physical_device);

	float queuePriority = 1.0f;
	std::set<uint32_t> uniqueQueueFamilies{ queueFamilyIndices.graphics.value(), queueFamilyIndices.present.value() };
	vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	for (uint32_t queueFamilyIndex : uniqueQueueFamilies)
	{
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	}

	VkPhysicalDeviceFeatures features{};

	VkDeviceCreateInfo deviceCreateInfo{};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
	deviceCreateInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();
	deviceCreateInfo.ppEnabledExtensionNames = REQUIRED_DEVICE_EXTENSION_NAMES.data();
	deviceCreateInfo.enabledExtensionCount = (uint32_t)REQUIRED_DEVICE_EXTENSION_NAMES.size();
	deviceCreateInfo.pEnabledFeatures = &features;

	fixed_vector<char const*> requiredLayerNames(1);
	if (debugOutput == debug_output::enabled)
	{
		requiredLayerNames[0] = "VK_LAYER_KHRONOS_validation";
		deviceCreateInfo.enabledLayerCount = 1;
		deviceCreateInfo.ppEnabledLayerNames = requiredLayerNames.data();
	}

	auto result = vkCreateDevice(m_physical_device, &deviceCreateInfo, nullptr, &m_device);
	if (result != VK_SUCCESS)
	{
		std::cerr << std::format("Failed creating Vulkan logical device: {}", result) << std::endl;
		return false;
	}

	volkLoadDeviceTable(&m_device_functions, m_device);

	m_device_functions.vkGetDeviceQueue(m_device, queueFamilyIndices.graphics.value(), 0, &m_queues.graphics);
	m_device_functions.vkGetDeviceQueue(m_device, queueFamilyIndices.present.value(), 0, &m_queues.present);

	return true;
}

bool renderer_vulkan::create_render_pass()
{
	VkAttachmentDescription color_attachment_description{};
	color_attachment_description.format = m_swapchain_image_format;
	color_attachment_description.samples = VK_SAMPLE_COUNT_1_BIT;
	color_attachment_description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	color_attachment_description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	color_attachment_description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_attachment_description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	color_attachment_description.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference color_attachment_reference{};
	color_attachment_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass_description{};
	subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass_description.pColorAttachments = &color_attachment_reference;
	subpass_description.colorAttachmentCount = 1;

	VkSubpassDependency subpass_dependency{};
	subpass_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	subpass_dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpass_dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpass_dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo render_pass_create_info{};
	render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	render_pass_create_info.pAttachments = &color_attachment_description;
	render_pass_create_info.attachmentCount = 1;
	render_pass_create_info.pSubpasses = &subpass_description;
	render_pass_create_info.subpassCount = 1;
	render_pass_create_info.pDependencies = &subpass_dependency;
	render_pass_create_info.dependencyCount = 1;

	auto result = m_device_functions.vkCreateRenderPass(m_device, &render_pass_create_info, nullptr, &m_render_pass);
	if (result != VK_SUCCESS)
	{
		std::cerr << std::format("Failed creating render pass: {}", result) << std::endl;
		return false;
	}

	return true;
}

VkShaderModule renderer_vulkan::create_shader_module(fixed_vector<char> const& code)
{
	VkShaderModuleCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	create_info.codeSize = code.size();
	create_info.pCode = reinterpret_cast<uint32_t const*>(code.data());

	VkShaderModule shader_module;
	auto result = m_device_functions.vkCreateShaderModule(m_device, &create_info, nullptr, &shader_module);
	if (result != VK_SUCCESS)
	{
		std::cerr << std::format("Failed to create shader module: {}", result) << std::endl;
		return VK_NULL_HANDLE;
	}

	return shader_module;
}

bool renderer_vulkan::create_swapchain(window const& window)
{
	auto swapchain_support_details = get_swapchain_support_details(m_physical_device, m_window_surface);

	auto surface_format = choose_surface_format(swapchain_support_details.formats);
	auto present_mode = choose_present_mode(swapchain_support_details.present_modes);
	m_swapchain_extent = choose_surface_extent(swapchain_support_details.capabilities, window.width(), window.height());
	m_swapchain_image_format = surface_format.format;

	uint32_t image_count = swapchain_support_details.capabilities.minImageCount + 1;
	if (swapchain_support_details.capabilities.maxImageCount > 0 &&
		image_count > swapchain_support_details.capabilities.maxImageCount)
		image_count = swapchain_support_details.capabilities.maxImageCount;

	auto queue_family_indices = find_queue_families(m_physical_device);
	uint32_t queue_family_indices_array[]{ queue_family_indices.graphics.value(), queue_family_indices.present.value() };

	VkSwapchainCreateInfoKHR swapchain_create_info{};
	swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchain_create_info.surface = m_window_surface;
	swapchain_create_info.minImageCount = image_count;
	swapchain_create_info.imageFormat = m_swapchain_image_format;
	swapchain_create_info.imageColorSpace = surface_format.colorSpace;
	swapchain_create_info.imageExtent = m_swapchain_extent;
	swapchain_create_info.imageArrayLayers = 1;
	swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchain_create_info.preTransform = swapchain_support_details.capabilities.currentTransform;
	swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchain_create_info.presentMode = present_mode;
	swapchain_create_info.clipped = VK_TRUE;
	swapchain_create_info.oldSwapchain = VK_NULL_HANDLE;
	if (queue_family_indices.graphics != queue_family_indices.present)
	{
		swapchain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		swapchain_create_info.queueFamilyIndexCount = 2;
		swapchain_create_info.pQueueFamilyIndices = queue_family_indices_array;
	}
	else
		swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;

	auto result = m_device_functions.vkCreateSwapchainKHR(m_device, &swapchain_create_info, nullptr, &m_swapchain);
	if (result != VK_SUCCESS)
	{
		std::cerr << std::format("Failed to create swapchain: {}", result) << std::endl;
		return false;
	}

	m_device_functions.vkGetSwapchainImagesKHR(m_device, m_swapchain, &image_count, nullptr);
	m_swapchain_images.resize(image_count);
	m_device_functions.vkGetSwapchainImagesKHR(m_device, m_swapchain, &image_count, m_swapchain_images.data());

	m_swapchain_image_views.resize(image_count);
	VkImageViewCreateInfo image_view_create_info{};
	image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	image_view_create_info.format = m_swapchain_image_format;
	image_view_create_info.components = { VK_COMPONENT_SWIZZLE_IDENTITY };
	image_view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	image_view_create_info.subresourceRange.levelCount = 1;
	image_view_create_info.subresourceRange.layerCount = 1;
	for (size_t i = 0; i < m_swapchain_images.size(); ++i)
	{
		image_view_create_info.image = m_swapchain_images[i];
		result = m_device_functions.vkCreateImageView(m_device, &image_view_create_info, nullptr,
													  &m_swapchain_image_views[i]);
		if (result != VK_SUCCESS)
		{
			std::cerr << std::format("Failed to create image view: {}", result) << std::endl;
			return false;
		}
	}

	return true;
}

bool renderer_vulkan::create_synchronization_objects()
{
	VkSemaphoreCreateInfo semaphore_create_info{};
	semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	auto result = m_device_functions.vkCreateSemaphore(m_device, &semaphore_create_info, nullptr, &m_image_available_semaphore);
	if (result != VK_SUCCESS)
	{
		std::cerr << std::format("Failed to create semaphore: {}", result) << std::endl;
		return false;
	}

	result = m_device_functions.vkCreateSemaphore(m_device, &semaphore_create_info, nullptr, &m_render_finished_semaphore);
	if (result != VK_SUCCESS)
	{
		std::cerr << std::format("Failed to create semaphore: {}", result) << std::endl;
		return false;
	}

	VkFenceCreateInfo fence_create_info{};
	fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	result = m_device_functions.vkCreateFence(m_device, &fence_create_info, nullptr, &m_in_flight_fence);
	if (result != VK_SUCCESS)
	{
		std::cerr << std::format("Failed to create fence: {}", result) << std::endl;
		return false;
	}

	return true;
}

renderer_vulkan::queue_family_indices renderer_vulkan::find_queue_families(VkPhysicalDevice physicalDevice)
{
	queue_family_indices indices;

	uint32_t queueFamilyCount;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
	fixed_vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyProperties.data());

	for (uint32_t i = 0; i < queueFamilyCount; ++i)
	{
		auto const& queueFamily = queueFamilyProperties[i];
		if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
			indices.graphics = i;

		VkBool32 hasPresentSupport;
		vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, m_window_surface, &hasPresentSupport);

		if (hasPresentSupport)
			indices.present = i;

		if (indices.is_complete())
			break;
	}

	return indices;
}

VkPhysicalDevice renderer_vulkan::pick_physical_device()
{
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
	fixed_vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

	std::multimap<int, VkPhysicalDevice> candidates;
	for (uint32_t i = 0; i < deviceCount; ++i)
		candidates.insert(std::make_pair(rate_device_suitability(devices[i]), devices[i]));

	if (candidates.rbegin()->first > 0)
		return candidates.rbegin()->second;

	return nullptr;
}

int renderer_vulkan::rate_device_suitability(VkPhysicalDevice physicalDevice)
{
	int score = 0;

	VkPhysicalDeviceProperties properties;
	VkPhysicalDeviceFeatures features;

	vkGetPhysicalDeviceProperties(physicalDevice, &properties);
	vkGetPhysicalDeviceFeatures(physicalDevice, &features);

	if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		score += 1000;

	if (!check_device_extension_support(physicalDevice, REQUIRED_DEVICE_EXTENSION_NAMES))
		return false;

	auto swapchain_support_details = get_swapchain_support_details(physicalDevice, m_window_surface);
	if (swapchain_support_details.formats.empty() || swapchain_support_details.present_modes.empty())
		return false;

	auto format = choose_surface_format(swapchain_support_details.formats);
	if (format.format == VK_FORMAT_B8G8R8A8_SRGB)
		score += 50;

	if (format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		score += 50;

	auto present_mode = choose_present_mode(swapchain_support_details.present_modes);
	if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR)
		score += 100;

	auto queueFamilyIndices = find_queue_families(physicalDevice);
	if (!queueFamilyIndices.is_complete())
		return false;

	return score;
}

bool renderer_vulkan::record_command_buffer(VkCommandBuffer command_buffer, uint32_t image_index)
{
	VkCommandBufferBeginInfo command_buffer_begin_info{};
	command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	auto result = m_device_functions.vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info);
	if (result != VK_SUCCESS)
	{
		std::cerr << std::format("Failed to begin recording command buffer: {}", result) << std::endl;
		return false;
	}

	VkClearValue clear_color{ 0.f, 0.f, 0.f, 1.f };
	VkRenderPassBeginInfo render_pass_begin_info{};
	render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_begin_info.renderPass = m_render_pass;
	render_pass_begin_info.framebuffer = m_swapchain_framebuffers[image_index];
	render_pass_begin_info.renderArea.offset = {};
	render_pass_begin_info.renderArea.extent = m_swapchain_extent;
	render_pass_begin_info.pClearValues = &clear_color;
	render_pass_begin_info.clearValueCount = 1;

	m_device_functions.vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
	m_device_functions.vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphics_pipeline);
	m_device_functions.vkCmdDraw(command_buffer, 3, 1, 0, 0);
	m_device_functions.vkCmdEndRenderPass(command_buffer);

	result = m_device_functions.vkEndCommandBuffer(command_buffer);
	if (result != VK_SUCCESS)
	{
		std::cerr << std::format("Failed to end recording command buffer: {}", result) << std::endl;
		return false;
	}

	return true;
}

bool check_device_extension_support(VkPhysicalDevice physicalDevice, fixed_vector<char const*> const& extensionNames)
{
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
	fixed_vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());

	for (int i = 0; i < extensionNames.size(); ++i)
	{
		bool extensionFound = false;
		for (int j = 0; j < availableExtensions.size(); ++j)
		{
			if (std::strcmp(extensionNames[i], availableExtensions[j].extensionName) == 0)
			{
				extensionFound = true;
				break;
			}
		}

		if (!extensionFound)
			return false;
	}

	return true;
}

bool check_extension_support(fixed_vector<char const*>& extensionNames)
{
	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
	fixed_vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());

	for (int i = 0; i < extensionNames.size(); ++i)
	{
		bool extensionFound = false;

		for (int j = 0; j < availableExtensions.size(); ++j)
		{
			if (strcmp(extensionNames[i], availableExtensions[j].extensionName) == 0)
			{
				extensionFound = true;
				break;
			}
		}

		if (!extensionFound)
		{
			std::cout << std::format("Required extension '{}' is not supported!", extensionNames[i]) << std::endl;
			return false;
		}
	}

	return true;
}

bool check_layer_support(fixed_vector<char const*>& layerNames)
{
	uint32_t layerCount = 0;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
	fixed_vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (int i = 0; i < layerNames.size(); ++i)
	{
		bool layerFound = false;

		for (int j = 0; j < availableLayers.size(); ++j)
		{
			if (strcmp(layerNames[i], availableLayers[j].layerName) == 0)
			{
				layerFound = true;
				break;
			}
		}

		if (!layerFound)
		{
			std::cout << std::format("Required layer '{}' is not supported!", layerNames[i]) << std::endl;
			return false;
		}
	}

	return true;
}

VkPresentModeKHR choose_present_mode(vector<VkPresentModeKHR> const& available_present_modes)
{
	if (available_present_modes.contains(VK_PRESENT_MODE_MAILBOX_KHR))
		return VK_PRESENT_MODE_MAILBOX_KHR;

	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D choose_surface_extent(VkSurfaceCapabilitiesKHR const& capabilities, uint32_t ideal_width, uint32_t ideal_height)
{
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
		return capabilities.currentExtent;

	return {
		clamp(ideal_width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
		clamp(ideal_height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
	};
}

VkSurfaceFormatKHR choose_surface_format(vector<VkSurfaceFormatKHR> const& available_formats)
{
	for (auto const& available_format : available_formats)
	{
		if (available_format.format == VK_FORMAT_B8G8R8A8_SRGB &&
			available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			return available_format;
	}

	return available_formats[0];
}

VkInstance create_instance(renderer_vulkan::debug_output debugOutput)
{
	fixed_vector<char const*> requiredExtensionNames = get_required_extension_names(debugOutput);
	if (!check_extension_support(requiredExtensionNames))
		return nullptr;

	auto debugOutputEnabled = debugOutput == renderer_vulkan::debug_output::enabled;
	fixed_vector<char const*> requiredLayerNames(1);
	requiredLayerNames[0] = "VK_LAYER_KHRONOS_validation";
	if (debugOutputEnabled && !check_layer_support(requiredLayerNames))
		return nullptr;

	VkApplicationInfo applicationInfo{};
	applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	applicationInfo.pApplicationName = "Hello Triangle";
	applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	applicationInfo.pEngineName = "VulkanEngine";
	applicationInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	applicationInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo instanceCreateInfo{};
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pApplicationInfo = &applicationInfo;
	instanceCreateInfo.ppEnabledExtensionNames = requiredExtensionNames.data();
	instanceCreateInfo.enabledExtensionCount = (uint32_t)requiredExtensionNames.size();

	VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo{};
	if (debugOutputEnabled)
	{
		instanceCreateInfo.ppEnabledLayerNames = requiredLayerNames.data();
		instanceCreateInfo.enabledLayerCount = (uint32_t)requiredLayerNames.size();

		debugUtilsMessengerCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		debugUtilsMessengerCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
														VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		debugUtilsMessengerCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
													VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
		debugUtilsMessengerCreateInfo.pfnUserCallback = debug_callback;
		instanceCreateInfo.pNext = &debugUtilsMessengerCreateInfo;
	}

	VkInstance instance;
	VkResult result = vkCreateInstance(&instanceCreateInfo, nullptr, &instance);
	if (result != VK_SUCCESS)
	{
		std::cout << std::format("Failed creating Vulkan instance: {}", result) << std::endl;
		return nullptr;
	}

	volkLoadInstanceOnly(instance);

	return instance;
}

VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type,
											  VkDebugUtilsMessengerCallbackDataEXT const* callbackData, void* userData)
{
	auto message = std::format("{} (Vulkan) - {}: {}", severity, (VkDebugUtilsMessageTypeFlagBitsEXT)type, callbackData->pMessage);
	if (severity == VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		std::cerr << message << std::endl;
	else
		std::cout << message << std::endl;

	return VK_FALSE;
}

fixed_vector<char const*> get_required_extension_names(renderer_vulkan::debug_output debugOutput)
{
	size_t extensionCount = 2;
	if (debugOutput == renderer_vulkan::debug_output::enabled)
		++extensionCount;

	fixed_vector<char const*> extensionNames(extensionCount);
	extensionNames[0] = VK_KHR_SURFACE_EXTENSION_NAME;
	extensionNames[1] = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
	if (debugOutput == renderer_vulkan::debug_output::enabled)
		extensionNames[2] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;

	return extensionNames;
}

swapchain_support_details get_swapchain_support_details(VkPhysicalDevice physical_device, VkSurfaceKHR surface)
{
	swapchain_support_details details;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &details.capabilities);

	uint32_t format_count;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, nullptr);
	if (format_count != 0)
	{
		details.formats.resize(format_count);
		vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, details.formats.data());
	}

	uint32_t present_mode_count;
	vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, nullptr);
	if (present_mode_count != 0)
	{
		details.present_modes.resize(present_mode_count);
		vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count,
												  details.present_modes.data());
	}

	return details;
}

void output_vulkan_details()
{
	std::cout << "Vulkan support details:" << std::endl;
	uint32_t supportedVulkanVersion;
	vkEnumerateInstanceVersion(&supportedVulkanVersion);
	std::cout << std::format("\tVersion: {}.{}.{} (variant {})", VK_API_VERSION_MAJOR(supportedVulkanVersion),
							 VK_API_VERSION_MINOR(supportedVulkanVersion), VK_API_VERSION_PATCH(supportedVulkanVersion),
							 VK_API_VERSION_VARIANT(supportedVulkanVersion))
			  << std::endl;

	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
	fixed_vector<VkExtensionProperties> extensions(extensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

	std::cout << std::format("\tExtensions ({}):", extensionCount) << std::endl;
	for (int i = 0; i < extensions.size(); ++i)
	{
		auto const& extension = extensions[i];
		std::cout << std::format("\t\t{}", extension) << std::endl;
	}

	uint32_t layerCount = 0;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
	fixed_vector<VkLayerProperties> layers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, layers.data());

	std::cout << std::format("\tLayers ({}):", layerCount) << std::endl;
	for (int i = 0; i < layers.size(); ++i)
	{
		auto const& layer = layers[i];
		std::cout << std::format("\t\t{}", layer) << std::endl;
	}
}

void output_vulkan_device_details(VkInstance instance)
{
	uint32_t physicalDeviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);
	fixed_vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
	vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices.data());

	std::cout << std::format("\tPhysical devices ({}):", physicalDeviceCount) << std::endl;
	for (int i = 0; i < physicalDevices.size(); ++i)
	{
		auto& physicalDevice = physicalDevices[i];
		VkPhysicalDeviceProperties physicalDeviceProperties;
		vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

		std::cout << "\t{" << std::endl;
		std::cout << std::format("\t\tVendor: {}", (VkVendorId)physicalDeviceProperties.vendorID) << std::endl;
		std::cout << std::format("\t\tName: {}", physicalDeviceProperties.deviceName) << std::endl;
		std::cout << std::format("\t\tType: {}", physicalDeviceProperties.deviceType) << std::endl;
		std::cout << std::format("\t\tSupported version: {}.{}.{} (variant {})",
								 VK_API_VERSION_MAJOR(physicalDeviceProperties.apiVersion),
								 VK_API_VERSION_MINOR(physicalDeviceProperties.apiVersion),
								 VK_API_VERSION_PATCH(physicalDeviceProperties.apiVersion),
								 VK_API_VERSION_VARIANT(physicalDeviceProperties.apiVersion))
				  << std::endl;

		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
		fixed_vector<VkExtensionProperties> extensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, extensions.data());

		std::cout << std::format("\t\tExtensions ({}):", extensionCount) << std::endl;
		for (int i = 0; i < extensions.size(); ++i)
		{
			auto const& extension = extensions[i];
			std::cout << std::format("\t\t\t{}", extension) << std::endl;
		}

		std::cout << "\t}" << std::endl;
	}
}
