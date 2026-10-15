#pragma once

#include <concepts>
#include <type_traits>

namespace mull_template_example
{

template <typename T>
concept SignedArithmetic = std::is_arithmetic_v<T> && std::is_signed_v<T>;

template <SignedArithmetic T>
constexpr T transform(T value)
{
    if (value < T{0})
    {
        return -value;
    }
    return value + T{1};
}

} // namespace mull_template_example
