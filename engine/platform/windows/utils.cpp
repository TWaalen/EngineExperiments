/* Copyright (c) 2022, Thijs Waalen
 *
 * SPDX-License-Identifier: ISC
 */

#include "engine/utils.h"

#include <cstdio>

#include <Windows.h>

namespace engine
{
	void create_console()
	{
		(void)AllocConsole();
		(void)std::freopen("CONIN$", "r", stdin);
		(void)std::freopen("CONOUT$", "w", stdout);
		(void)std::freopen("CONOUT$", "w", stderr);
		HANDLE hConOut = CreateFile(L"CONOUT$", GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		HANDLE hConIn = CreateFile(L"CONIN$", GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		(void)SetStdHandle(STD_INPUT_HANDLE, hConIn);
		(void)SetStdHandle(STD_OUTPUT_HANDLE, hConOut);
		(void)SetStdHandle(STD_ERROR_HANDLE, hConOut);
	}
}
