#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <string>
#include <string_view>

#if __has_include(<print>)
#include <print>
#endif

#include <exasol/udf/v2/exception.hpp>

namespace exasol::udf::v2::detail
{

inline std::string format_stacktrace_entry(const std::size_t frame_number,
                                           const std::string_view description,
                                           const std::string_view source_file,
                                           const std::uint_least32_t source_line)
{
    if (source_file.empty())
    {
        return "  #" + std::to_string(frame_number) + " " + std::string(description) + "\n";
    }
    return "  #" + std::to_string(frame_number) + " " + std::string(description) + " (" +
           std::string(source_file) + ":" + std::to_string(source_line) + ")\n";
}

inline std::string format_assertion_failure(const Exception& error)
{
    std::string output = std::string(error.location().file_name()) + ":" +
                         std::to_string(error.location().line()) + ":" +
                         std::string(error.location().function_name()) + ": " + error.what() + "\n";
    std::size_t frame_number = 0;
    for (const auto& frame : error.stacktrace())
    {
        output += format_stacktrace_entry(frame_number, frame.description(), frame.source_file(),
                                          frame.source_line());
        ++frame_number;
    }
    return output;
}

using AssertionTerminator = void (*)();

[[noreturn]] inline void assertion_failure(const char* expression,
                                           std::source_location location,
                                           const AssertionTerminator terminator = std::abort)
{
    const Exception error("Assertion failed: " + std::string(expression), location);
    const std::string output = format_assertion_failure(error);
#if __has_include(<print>)
    std::print(stderr, "{}", output);
#else
    static_cast<void>(std::fwrite(output.data(), sizeof(char), output.size(), stderr));
#endif
    std::fflush(stderr);
    terminator();
    std::abort();
}

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
