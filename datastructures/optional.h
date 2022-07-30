/* Copyright (c) 2022, Thijs Waalen
 *
 * SPDX-License-Identifier: ISC
 */

#pragma once

#include <cassert>

namespace datastructures
{
	template <typename T>
	class optional
	{
	public:
		inline bool has_value() const
		{
			return m_has_value;
		}

		inline T const& value() const
		{
			assert(m_has_value);
			return m_value;
		}

		void operator=(T value)
		{
			m_has_value = true;
			m_value = value;
		}

		bool operator!=(optional<T> const& rhs)
		{
			return has_value() != rhs.has_value() || value() != rhs.value();
		}

	private:
		bool m_has_value{ false };
		T m_value;
	};
}
