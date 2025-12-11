#pragma once

#include <stdint.h>
#include <functional>

#include "type.h"

namespace core
{
    namespace detail
    {
        constexpr uint32_t val_32_const = 0x811c9dc5;
        constexpr uint32_t prime_32_const = 0x1000193;
        constexpr uint64_t val_64_const = 0xcbf29ce484222325;
        constexpr uint64_t prime_64_const = 0x100000001b3;

        inline constexpr uint32_t hash_32_fnv1a_const(const char* const _str, const uint32_t _value = val_32_const) noexcept
        {
            return (_str[0] == '\0') ? _value : hash_32_fnv1a_const(&_str[1], (_value ^ uint32_t(_str[0])) * prime_32_const);
        }

        inline constexpr uint64_t hash_64_fnv1a_const(const char* const _str, const uint64_t _value = val_64_const) noexcept
        {
            return (_str[0] == '\0') ? _value : hash_64_fnv1a_const(&_str[1], (_value ^ uint64_t(_str[0])) * prime_64_const);
        }

        inline constexpr uint32_t hash_32_fnv1a_const(const wchar_t* const _str, const uint32_t _value = val_32_const) noexcept
        {
            return (_str[0] == '\0') ? _value : hash_32_fnv1a_const(&_str[1], (_value ^ uint32_t(_str[0])) * prime_32_const);
        }

        inline constexpr uint64_t hash_64_fnv1a_const(const wchar_t* const _str, const uint64_t _value = val_64_const) noexcept
        {
            return (_str[0] == '\0') ? _value : hash_64_fnv1a_const(&_str[1], (_value ^ uint64_t(_str[0])) * prime_64_const);
        }
    }

    #define HASH32(str) ::core::detail::hash_32_fnv1a_const(#str)
    #define HASH32_STR(str) ::core::detail::hash_32_fnv1a_const(str)

    // Helpers to combine hashes
    inline void hash_combine(std::size_t& /*_seed*/)
    {
    }

    template <typename T, typename... Rest>
    inline void hash_combine(std::size_t& _seed, const T& _v, Rest... _rest) 
    {
        std::hash<T> hasher;
        _seed ^= hasher(_v) + 0x9e3779b9 + (_seed << 6) + (_seed >> 2);
        hash_combine(_seed, _rest...);
    }
}
