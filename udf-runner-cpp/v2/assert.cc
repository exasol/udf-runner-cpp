#include <exasol/udf/v2/assert.hpp>

#include <cstdio>
#include <sstream>
#include <utility>

namespace exasol::udf::v2::detail
{

std::string format_stacktrace_entry(const std::size_t frame_number,
                                    const std::string_view description,
                                    const std::string_view source_file,
                                    const std::uint_least32_t source_line)
{
    std::ostringstream output;
    output << "  #" << frame_number << ' ' << description;
    if (!source_file.empty())
    {
        output << " (" << source_file << ':' << source_line << ')';
    }
    output << '\n';
    return output.str();
}

std::string format_assertion_failure(const Exception& error)
{
    std::ostringstream output;
    output << error.location().file_name() << ':' << error.location().line() << ':'
           << error.location().function_name() << ": " << error.what() << '\n';
    std::size_t frame_number = 0;
    for (const auto& frame : error.stacktrace())
    {
        output << format_stacktrace_entry(frame_number, frame.description(), frame.source_file(),
                                          frame.source_line());
        ++frame_number;
    }
    return output.str();
}

[[noreturn]] void assertion_failure(const char* expression,
                                    std::source_location location,
                                    AssertionTerminator terminator)
{
    std::ostringstream message;
    message << "Assertion failed: " << expression;
    const Exception error(message.str(), location);
    const std::string output = format_assertion_failure(error);
    static_cast<void>(std::fwrite(output.data(), sizeof(char), output.size(), stderr));
    std::fflush(stderr);
    terminator();
    std::unreachable();
}

} // namespace exasol::udf::v2::detail
