#pragma once

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <string>

#include <exasol/udf/v2/exception.hpp>

namespace exasol::udf::v2::detail
{

[[noreturn]] inline void assertion_failure(const char* expression, std::source_location location)
{
    const Exception error("Assertion failed: " + std::string(expression), location);
    std::fprintf(stderr, "%s:%u: %s: %s\n", error.location().file_name(), error.location().line(),
                 error.location().function_name(), error.what());

    std::size_t frame_number = 0;
    for (const auto& frame : error.stacktrace())
    {
        const std::string description = frame.description();
        const std::string source_file = frame.source_file();
        if (source_file.empty())
        {
            std::fprintf(stderr, "  #%zu %s\n", frame_number, description.c_str());
        }
        else
        {
            std::fprintf(stderr, "  #%zu %s (%s:%u)\n", frame_number, description.c_str(),
                         source_file.c_str(), frame.source_line());
        }
        ++frame_number;
    }
    std::fflush(stderr);
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
