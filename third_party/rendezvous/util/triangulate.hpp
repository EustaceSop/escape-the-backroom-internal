#pragma once
#include "math.hpp"

namespace rv::triangulate
{
	inline float sign(const position& p1, const position& p2, const position& p3) noexcept
	{
		return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
	}

	inline bool point_in_triangle(const position& pt, const position& v1, const position& v2, const position& v3) noexcept
	{
		const float d1 = sign(pt, v1, v2);
		const float d2 = sign(pt, v2, v3);
		const float d3 = sign(pt, v3, v1);

		const bool has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
		const bool has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);

		return !(has_neg && has_pos);
	}

	[[nodiscard]] inline vector_t<cstd::uint32_t> execute(const span_t<const position> points) noexcept
	{
		vector_t<cstd::uint32_t> indices;
		if (points.size() < 3) 
		{
			return indices;
		}

		const cstd::uint32_t n = static_cast<cstd::uint32_t>(points.size());
		
		vector_t<cstd::uint32_t> remaining;
		remaining.reserve(n);
		for (cstd::uint32_t i = 0; i < n; ++i) 
		{
			remaining.push_back(i);
		}

		float area = 0.f;
		for (cstd::uint32_t i = 0; i < n; ++i)
		{
			const cstd::uint32_t j = (i + 1) % n;
			area += points[i].x * points[j].y - points[j].x * points[i].y;
		}

		const bool is_ccw = area < 0.f;

		auto is_convex = [&](const position& prev, const position& curr, const position& next) -> bool
		{
			const float cross = (curr.x - prev.x) * (next.y - curr.y) - (curr.y - prev.y) * (next.x - curr.x);
			return is_ccw ? (cross <= 0.f) : (cross >= 0.f);
		};

		cstd::uint32_t error_counter = 2 * n;

		while (remaining.size() > 3)
		{
			bool ear_found = false;
			const cstd::uint32_t r = static_cast<cstd::uint32_t>(remaining.size());

			for (cstd::uint32_t i = 0; i < r; ++i)
			{
				const cstd::uint32_t prev_idx = remaining[(i + r - 1) % r];
				const cstd::uint32_t curr_idx = remaining[i];
				const cstd::uint32_t next_idx = remaining[(i + 1) % r];

				const position prev = points[prev_idx];
				const position curr = points[curr_idx];
				const position next = points[next_idx];

				if (is_convex(prev, curr, next))
				{
					bool is_ear = true;
					for (cstd::uint32_t j = 0; j < r; ++j)
					{
						if (j == i || j == (i + r - 1) % r || j == (i + 1) % r) 
						{
							continue;
						}

						if (point_in_triangle(points[remaining[j]], prev, curr, next))
						{
							is_ear = false;
							break;
						}
					}

					if (is_ear)
					{
						indices.push_back(prev_idx);
						indices.push_back(curr_idx);
						indices.push_back(next_idx);
						remaining.erase(remaining.begin() + i);
						ear_found = true;
						break;
					}
				}
			}

			if (!ear_found)
			{
				indices.push_back(remaining[0]);
				indices.push_back(remaining[1]);
				indices.push_back(remaining[2]);
				remaining.erase(remaining.begin() + 1);
			}

			if (--error_counter == 0) 
			{
				break;
			}
		}

		if (remaining.size() == 3)
		{
			indices.push_back(remaining[0]);
			indices.push_back(remaining[1]);
			indices.push_back(remaining[2]);
		}

		return indices;
	}
}
