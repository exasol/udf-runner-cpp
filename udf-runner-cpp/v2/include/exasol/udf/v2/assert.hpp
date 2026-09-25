#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <source_location>
#include <string>
#include <string_view>

#include <exasol/udf/v2/exception.hpp>

namespace exasol::udf::v2::detail
{

[[nodiscard]] std::string format_stacktrace_entry(const std::size_t frame_number,
                                                  const std::string_view description,
                                                  const std::string_view source_file,
                                                  const std::uint_least32_t source_line);

[[nodiscard]] std::string format_assertion_failure(const Exception& error);

using AssertionTerminator = std::function<void()>;

// Keep production termination injectable so tests can replace abort with a throwing callback.
[[noreturn]] void assertion_failure(
    const char* expression, std::source_location location, AssertionTerminator terminator = [] {
        std::abort();
    });

} // namespace exasol::udf::v2::detail

// Wrap the macro in one statement so it is safe to use in if/else control flow.
#define EXASOL_UDF_ASSERT(condition)                                                       \
    do                                                                                     \
    {                                                                                      \
        if (!(condition))                                                                  \
        {                                                                                  \
            ::exasol::udf::v2::detail::assertion_failure(#condition,                       \
                                                         std::source_location::current()); \
        }                                                                                  \
    } while (false)
