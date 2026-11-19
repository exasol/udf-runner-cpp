#include <exasol/udf/v2/utils/log_utils.h>

#include <iostream>
#include <string>
#include <thread>

namespace exasol::udf::v2::utils::logging
{
namespace
{

    std::ostream& write_log_prefix(std::ostream& output,
                                   const std::string_view severity,
                                   const std::source_location code_location)
    {
        return output << '[' << severity << " '" << code_location.function_name()
                      << "':" << code_location.line() << " threadId=" << std::this_thread::get_id()
                      << "]: ";
    }

} // namespace

std::string format_strings(const std::vector<std::string>& strings)
{
    std::string result = "[";
    for (const auto& string : strings)
    {
        if (result.size() > 1)
        {
            result += ", ";
        }
        result += string;
    }
    result += ']';
    return result;
}

// cppcheck-suppress constParameterReference -- the logging provider exposes non-const stream
// accessors.
void log_trace(exasol::udf::v2::provider::logging::LoggingProviderIf& logging_provider,
               const std::string_view message,
               const std::source_location code_location)
{
#ifndef NDEBUG
    write_log_prefix(logging_provider.verbose(), "VERBOSE", code_location) << message << std::endl;
#else
    static_cast<void>(logging_provider);
    static_cast<void>(message);
    static_cast<void>(code_location);
#endif
}

void log_debug(exasol::udf::v2::provider::logging::LoggingProviderIf& logging_provider,
               const std::string_view message,
               const std::source_location code_location)
{
    write_log_prefix(logging_provider.debug(), "DEBUG", code_location) << message << std::endl;
}

void log_warning(exasol::udf::v2::provider::logging::LoggingProviderIf& logging_provider,
                 const std::string_view message,
                 const std::source_location code_location)
{
    write_log_prefix(logging_provider.warning(), "WARNING", code_location) << message << std::endl;
}

void log_notice(exasol::udf::v2::provider::logging::LoggingProviderIf& logging_provider,
                const std::string_view message,
                const std::source_location code_location)
{
    write_log_prefix(logging_provider.notice(), "NOTICE", code_location) << message << std::endl;
}

void log_error(exasol::udf::v2::provider::logging::LoggingProviderIf& logging_provider,
               const std::string_view message,
               const std::source_location code_location)
{
    write_log_prefix(logging_provider.error(), "ERROR", code_location) << message << std::endl;
}

FunctionTraceLog::FunctionTraceLog(
    exasol::udf::v2::provider::logging::LoggingProviderIf& logging_provider,
    const std::source_location code_location) noexcept
#ifndef NDEBUG
    : logging_provider(logging_provider), code_location(code_location)
#endif
{
#ifndef NDEBUG
    try
    {
        enabled = this->logging_provider.isVerboseEnabled();
        if (enabled)
        {
            log_trace(this->logging_provider, "Start", this->code_location);
        }
    }
    catch (...)
    {
        // Scope tracing must not change the caller's control flow.
        enabled = false;
    }
#else
    static_cast<void>(logging_provider);
    static_cast<void>(code_location);
#endif
}

FunctionTraceLog::~FunctionTraceLog() noexcept
{
#ifndef NDEBUG
    try
    {
        if (enabled)
        {
            log_trace(this->logging_provider, "End", this->code_location);
        }
    }
    catch (...) // NOSONAR: A failed end trace must not throw from this destructor.
    {
        // Scope tracing is best-effort, and the logging provider may be the source of the failure.
    }
#endif
}

} // namespace exasol::udf::v2::utils::logging
