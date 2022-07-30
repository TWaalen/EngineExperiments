/* Copyright (c) 2022, Thijs Waalen
 *
 * SPDX-License-Identifier: ISC
 */

#pragma once

#include <format>
#include <string>
#include <volk/volk.h>

template <typename CharT>
struct std::formatter<VkDebugUtilsMessageSeverityFlagBitsEXT, CharT> : std::formatter<CharT const*, CharT>
{
	template <typename FormatContext>
	auto format(const VkDebugUtilsMessageSeverityFlagBitsEXT& v, FormatContext& ctx)
	{
		CharT const* severity;
		switch (v)
		{
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
			severity = "VERBOSE";
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
			severity = "INFO";
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
			severity = "WARNING";
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
			severity = "ERROR";
			break;
		}

		return std::formatter<CharT const*, CharT>::format(severity, ctx);
	}
};

template <typename CharT>
struct std::formatter<VkDebugUtilsMessageTypeFlagBitsEXT, CharT> : std::formatter<std::string, CharT>
{
	template <typename FormatContext>
	auto format(const VkDebugUtilsMessageTypeFlagBitsEXT& v, FormatContext& ctx)
	{
		std::string type = "";
		bool typeSet = false;
		if (v & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT)
		{
			type = "GENERAL";
			typeSet = true;
		}

		if (v & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
		{
			if (typeSet)
				type += " | ";
			else
				typeSet = true;

			type += "PERFORMANCE";
		}

		if (v & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
		{
			if (typeSet)
				type += " | ";
			type += "VALIDATION";
		}

		return std::formatter<std::string, CharT>::format(type, ctx);
	}
};

template <typename CharT>
struct std::formatter<VkPhysicalDeviceType, CharT> : std::formatter<char const*, CharT>
{
	template <typename FormatContext>
	auto format(const VkPhysicalDeviceType& v, FormatContext& ctx)
	{
		char const* message = "Unknown";

		switch (v)
		{
		case VK_PHYSICAL_DEVICE_TYPE_OTHER:
			message = "Other";
			break;
		case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
			message = "iGPU";
			break;
		case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
			message = "dGPU";
			break;
		case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
			message = "vGPU";
			break;
		case VK_PHYSICAL_DEVICE_TYPE_CPU:
			message = "CPU";
			break;
		}

		return std::formatter<char const*, CharT>::format(message, ctx);
	}
};

template <typename CharT>
struct std::formatter<VkResult, CharT> : std::formatter<CharT const*, CharT>
{
	template <typename FormatContext>
	auto format(const VkResult& v, FormatContext& ctx)
	{
		char const* message;
		switch (v)
		{
		case VK_ERROR_DEVICE_LOST:
			message = "The device has been lost";
			break;
		case VK_ERROR_EXTENSION_NOT_PRESENT:
			message = "A requested extension is not present";
			break;
		case VK_ERROR_FEATURE_NOT_PRESENT:
			message = "A requested feature is not present";
			break;
		case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:
			message = "Exclusive full-screen access has been lost";
			break;
		case VK_ERROR_INCOMPATIBLE_DRIVER:
			message = "Cannot find a compatible Vulkan driver";
			break;
		case VK_ERROR_INITIALIZATION_FAILED:
			message = "Initialization failed";
			break;
		case VK_ERROR_LAYER_NOT_PRESENT:
			message = "A requested layer is not present";
			break;
		case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
			message = "The specified window is already in use by Vulkan or another API";
			break;
		case VK_ERROR_OUT_OF_DATE_KHR:
			message = "The surface has changed and is no longer compatible with the used swapchain";
			break;
		case VK_ERROR_SURFACE_LOST_KHR:
			message = "The specified surface has been lost";
			break;
		case VK_ERROR_OUT_OF_DEVICE_MEMORY:
			message = "Out of device memory";
			break;
		case VK_ERROR_OUT_OF_HOST_MEMORY:
			message = "Out of host memory";
			break;
		case VK_ERROR_TOO_MANY_OBJECTS:
			message = "Too many objects of this type have been created";
			break;
		case VK_ERROR_VALIDATION_FAILED_EXT:
			message = "Validation failed";
			break;
		}

		return std::formatter<char const*, CharT>::format(message, ctx);
	}
};

template <typename CharT>
struct std::formatter<VkVendorId, CharT> : std::formatter<char const*, CharT>
{
	template <typename FormatContext>
	auto format(const VkVendorId& v, FormatContext& ctx)
	{
		char const* message = "Unknown";
		switch (v)
		{
		case 0x10de:
			message = "NVIDIA";
			break;
		}

		return std::formatter<char const*, CharT>::format(message, ctx);
	}
};

template <typename CharT>
struct std::formatter<VkExtensionProperties, CharT>
{
	template <typename ParseContext>
	constexpr auto parse(ParseContext& ctx) { return ctx.begin(); }

	template <typename FormatContext>
	auto format(const VkExtensionProperties& v, FormatContext& ctx)
	{
		return std::format_to(ctx.out(), "{} (Version {})", v.extensionName, v.specVersion);
	}
};

template <typename CharT>
struct std::formatter<VkLayerProperties, CharT>
{
	template <typename ParseContext>
	constexpr auto parse(ParseContext& ctx) { return ctx.begin(); }

	template <typename FormatContext>
	auto format(const VkLayerProperties& v, FormatContext& ctx)
	{
		return std::format_to(ctx.out(), "{} (based on Vulkan {}.{}.{} (variant {}), version {})",
							  v.layerName, VK_API_VERSION_MAJOR(v.specVersion), VK_API_VERSION_MINOR(v.specVersion),
							  VK_API_VERSION_PATCH(v.specVersion), VK_API_VERSION_VARIANT(v.specVersion),
							  v.implementationVersion);
	}
};
