/* Copyright (c) 2022, Thijs Waalen
 *
 * SPDX-License-Identifier: ISC
 */

#pragma once

#include <memory>

namespace engine
{
	class window
	{
	public:
		static std::unique_ptr<window> create(const wchar_t* title, int width, int height);

		virtual void update() = 0;

		virtual uint32_t width() const = 0;
		virtual uint32_t height() const = 0;

		bool should_close() const { return m_should_close; }

	protected:
		bool m_should_close{ false };
	};
}
