/* Copyright (c) 2022, Thijs Waalen
 *
 * SPDX-License-Identifier: ISC
 */

#pragma once

namespace math
{
	template <typename T>
	constexpr T const& clamp(T const& value, T const& min, T const& max)
	{
		return value < min ? min : value < max ? value
											   : max;
	}
}
