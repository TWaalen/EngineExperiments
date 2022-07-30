/* Copyright (c) 2022, Thijs Waalen
 *
 * SPDX-License-Identifier: ISC
 */

#pragma once

#include <cassert>
#include <cstring>
#include <initializer_list>

namespace datastructures
{
	template <typename T>
	class fixed_vector
	{
	public:
		fixed_vector(size_t count) : m_data(new T[count]), m_size(count) {}
		fixed_vector(fixed_vector<T>& other) : m_data(new T[other.size()]), m_size(other.size())
		{
			std::memcpy(m_data, other.data(), sizeof(T) * m_size);
		}
		fixed_vector(std::initializer_list<T> initializer_list) : m_data(new T[initializer_list.size()]), m_size(initializer_list.size())
		{
			auto i = 0;
			for (auto const& value : initializer_list)
			{
				m_data[i] = value;
				++i;
			}
		}

		~fixed_vector() { delete[] m_data; }

		constexpr T const& at(size_t pos) const
		{
			assert(pos < m_size);
			return m_data[pos];
		}

		constexpr T& at(size_t pos)
		{
			assert(pos < m_size);
			return m_data[pos];
		}

		constexpr T const& operator[](size_t pos) const { return m_data[pos]; }
		constexpr T& operator[](size_t pos) { return m_data[pos]; }

		T* data() { return m_data; }
		T const* data() const { return m_data; }

		size_t size() const { return m_size; }
		bool empty() const { return m_size == 0; }

	private:
		T* m_data;
		size_t m_size;
	};
}
