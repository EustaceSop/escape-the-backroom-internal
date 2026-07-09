#pragma once
#include "types.hpp"

namespace rv
{
	inline vector_t<std::uint8_t> read_file(const string_t& path)
	{
		ifstream_t file(path, cstd::ios::binary);

		if (!file.is_open())
		{
			return { };
		}

		return { std::istreambuf_iterator(file), { } };
	}
}
