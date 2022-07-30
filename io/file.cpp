/* Copyright (c) 2022, Thijs Waalen
 *
 * SPDX-License-Identifier: ISC
 */

#include "file.h"

#include <format>
#include <fstream>
#include <iostream>

using datastructures::fixed_vector;

namespace io
{
	fixed_vector<char> read_entire_file(char const* path, file_mode mode)
	{
		int open_mode = std::ios::ate;
		if (mode == file_mode::binary)
			open_mode |= std::ios::binary;

		std::ifstream file(path, open_mode);
		if (!file)
		{
			std::cerr << std::format("Failed to open file {}", path) << std::endl;
			return {};
		}

		auto file_size = file.tellg();
		if (file_size == -1)
		{
			std::cerr << std::format("Failed to get file size of {}", path) << std::endl;
			return {};
		}

		file.seekg(0);
		if (file.fail())
		{
			std::cerr << std::format("Failed to seek to the beginning of {}", path) << std::endl;
			return {};
		}

		fixed_vector<char> data(file_size);
		file.read(data.data(), file_size);
		if (file.fail())
		{
			std::cerr << std::format("Failed to read all data from {}", path) << std::endl;
			return {};
		}

		return data;
	}
}
