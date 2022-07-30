/* Copyright (c) 2022, Thijs Waalen
 *
 * SPDX-License-Identifier: ISC
 */

#pragma once

#include <cassert>
#include <cstring>
#include <limits>

namespace datastructures
{
	template <typename T>
	class vector
	{
	public:
		vector(size_t initial_capacity = 2) : m_data(new T[initial_capacity]), m_capacity(initial_capacity) {}
		vector(vector<T>& other) : m_data(new T[other.capacity()]), m_capacity(other.capacity()), m_size(other.size())
		{
			std::memcpy(m_data, other.data(), sizeof(T) * m_size);
		}

		~vector() { delete[] m_data; }

		constexpr T* begin() noexcept { return m_data; }
		constexpr T* end() noexcept { return m_data + m_size; }

		constexpr T const* begin() const noexcept { return m_data; }
		constexpr T const* end() const noexcept { return m_data + m_size; }

		constexpr T const* cbegin() const noexcept { return m_data; }
		constexpr T const* cend() const noexcept { return m_data + m_size; }

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

		size_t size() const noexcept { return m_size; }
		size_t capacity() const noexcept { return m_capacity; }
		bool empty() const noexcept { return m_size == 0; }

		void clear() { m_size = 0; }

		void resize(size_t new_size)
		{
			if (new_size > m_capacity)
			{
				m_capacity = new_size;
				auto new_data = new T[m_capacity];
				std::memcpy(new_data, m_data, sizeof(T) * m_size);
				std::memset(new_data + m_size, 0, sizeof(T) * (m_capacity - m_size));
				delete[] m_data;
				m_data = new_data;
			}

			m_size = new_size;
		}

		void push_back(T const& value)
		{
			++m_size;
			if (m_size > m_capacity)
			{
				m_capacity *= 2;
				auto new_data = new T[m_capacity];
				std::memcpy(new_data, m_data, sizeof(T) * (m_size - 1));
				delete[] m_data;
				m_data = new_data;
			}

			m_data[m_size - 1] = value;
		}

		bool contains(T const& value) const
		{
			for (auto i = 0; i < m_size; ++i)
			{
				if (m_data[i] == value)
					return true;
			}

			return false;
		}

	private:
		T* m_data;
		size_t m_size = 0;
		size_t m_capacity;
	};
}
