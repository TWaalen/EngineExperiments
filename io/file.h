/* Copyright (c) 2022, Thijs Waalen
 *
 * SPDX-License-Identifier: ISC
 */

#pragma once

#include "datastructures/fixed_vector.h"

namespace io
{
	enum class file_mode
	{
		text,
		binary
	};

	datastructures::fixed_vector<char> read_entire_file(char const* path, file_mode);
}
