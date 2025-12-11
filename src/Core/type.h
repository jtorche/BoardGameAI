#pragma once

#include <inttypes.h>

#include "linalg.h"

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using uint = uint32_t;
using u64 = uint64_t;

using i32 = int32_t;
using i64 = int64_t;

using f32 = float;
using f64 = double;

using ubyte = unsigned char;

using vec4 = linalg::aliases::float4;
using ivec4 = linalg::aliases::int4;
using ivec2 = linalg::aliases::int2;
using vec3 = linalg::aliases::float3;
using vec2 = linalg::aliases::float2;
using mat4 = linalg::aliases::float4x4;
using uvec2 = linalg::aliases::uint2;
using uvec3 = linalg::aliases::uint3;
using uvec4 = linalg::aliases::uint4;
using ubyte2 = linalg::aliases::byte2;

namespace core
{
	struct Color
	{
		float r, g, b, a;
	};

	struct ColorInteger
	{
		union
		{
			struct Color { ubyte r, g, b, a; };
			u32 color;
		};
	};

    template<typename TO, typename FROM>
    TO union_cast(FROM _from)
    {
        union
        {
            FROM a;
            TO b;
        } res;
        res.a = _from;
        return res.b;
    }

    template<typename T>
    constexpr T alignUp(T _x, T _align)
    {
        return _x + (_x % _align == 0 ? 0 : _align - _x % _align);
    }

    template<typename T, typename A>
    constexpr T * alignUpPtr(T * _ptr, A _align)
    {
        std::uintptr_t ptr = reinterpret_cast<std::uintptr_t>(_ptr);
        ptr = ptr + (ptr % _align == 0 ? 0 : _align - ptr % _align);
        return reinterpret_cast<T*>(ptr);
    }

    template<typename T>
    constexpr T absolute_difference(T _a, T _b)
    {
        return _a < _b ? _b - _a : _a - _b;
    }
}
