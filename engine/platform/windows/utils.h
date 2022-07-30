/* Copyright (c) 2022, Thijs Waalen
 *
 * SPDX-License-Identifier: ISC
 */

#pragma once

#include <format>
#include <string>
#include <string_view>

#include <Windows.h>

namespace engine
{
	std::wstring error_message_from_win32_error_code(DWORD errorCode)
	{
		LPWSTR errorMessage;
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
					  nullptr, errorCode, 0, (LPWSTR)&errorMessage, 1, nullptr);
		auto ret = std::vformat(L"{} ({})", std::make_wformat_args(errorMessage, errorCode));
		LocalFree(errorMessage);

		return ret;
	}
}
