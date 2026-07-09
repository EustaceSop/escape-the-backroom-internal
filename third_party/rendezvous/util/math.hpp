#pragma once
#include "types.hpp"

namespace rv
{
	template <class T>
	struct vector_2d
	{
		T x, y;

		[[nodiscard]] vector_2d operator+(const vector_2d& other) const noexcept
		{
			return vector_2d{ x + other.x, y + other.y };
		}

		[[nodiscard]] vector_2d operator-(const vector_2d& other) const noexcept
		{
			return vector_2d{ x - other.x, y - other.y };
		}

		[[nodiscard]] vector_2d operator*(const T scalar) const noexcept
		{
			return { x * scalar, y * scalar };
		}

		[[nodiscard]] T dot(const vector_2d& other) const noexcept
		{
			return x * other.x + y * other.y;
		}

		[[nodiscard]] bool operator==(const vector_2d& other) const
		{
			return x == other.x && y == other.y;
		}

		[[nodiscard]] bool operator!=(const vector_2d& other) const
		{
			return x != other.x || y != other.y;
		}

		[[nodiscard]] explicit operator bool() const noexcept
		{
			return x != T{} || y != T{};
		}

		[[nodiscard]] float distance(const vector_2d& other) const noexcept
		{
			const float dx = static_cast<float>(x - other.x);
			const float dy = static_cast<float>(y - other.y);

			return cstd::sqrtf(dx * dx + dy * dy);
		}

		[[nodiscard]] float magnitude() const noexcept
		{
			return cstd::sqrtf(static_cast<float>(x * x + y * y));
		}

		[[nodiscard]] vector_2d normalise() const noexcept
		{
			const float mag = magnitude();

			if (mag < 0.0001f)
			{
				return { };
			}

			return vector_2d{ x / mag, y / mag };
		}

		[[nodiscard]] vector_2d perpendicular() const noexcept
		{
			return { y, -x };
		}
	};
}
