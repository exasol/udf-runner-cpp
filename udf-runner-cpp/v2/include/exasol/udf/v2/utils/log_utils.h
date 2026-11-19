#ifndef EXASOL_UDF_V2_UTILS_LOG_UTILS_H
#define EXASOL_UDF_V2_UTILS_LOG_UTILS_H

#include <exasol/udf/v2/provider/logging_provider.h>

#include <source_location>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace exasol::udf::v2::utils::logging
{

/** Formats strings as a comma-separated list enclosed in square brackets. */
std::string format_strings(const std::vector<std::string>& strings);

/** Writes a trace message to the provider verbose stream. */
void log_trace(exasol::udf::v2::provider::logging::LoggingProviderIf& logging_provider,
               std::string_view message,
               const std::source_location code_location);

/** Writes a message to the provider debug stream with its source location. */
void log_debug(exasol::udf::v2::provider::logging::LoggingProviderIf& logging_provider,
               std::string_view message,
               const std::source_location code_location);

/** Writes a message to the provider warning stream with its source location. */
void log_warning(exasol::udf::v2::provider::logging::LoggingProviderIf& logging_provider,
                 std::string_view message,
                 const std::source_location code_location);

/** Writes a message to the provider notice stream with its source location. */
void log_notice(exasol::udf::v2::provider::logging::LoggingProviderIf& logging_provider,
                std::string_view message,
                const std::source_location code_location);

/** Creates and writes a debug message only when the provider debug stream is enabled. */
template <typename MessageFactory>
void log_debug_lazy_if_enabled(
    exasol::udf::v2::provider::logging::LoggingProviderIf& logging_provider,
    MessageFactory&& message_factory,
    const std::source_location code_location)
{
    if (logging_provider.isDebugEnabled())
    {
        log_debug(logging_provider, std::forward<MessageFactory>(message_factory)(), code_location);
    }
}

/** Creates and writes a notice message only when the provider notice stream is enabled. */
template <typename MessageFactory>
void log_notice_lazy_if_enabled(
    exasol::udf::v2::provider::logging::LoggingProviderIf& logging_provider,
    MessageFactory&& message_factory,
    const std::source_location code_location)
{
    if (logging_provider.isNoticeEnabled())
    {
        log_notice(logging_provider, std::forward<MessageFactory>(message_factory)(),
                   code_location);
    }
}

/** Writes a message to the provider error stream with its source location. */
void log_error(exasol::udf::v2::provider::logging::LoggingProviderIf& logging_provider,
               std::string_view message,
               const std::source_location code_location);

/** Creates and writes a trace message to the provider verbose stream in Debug builds. */
template <typename MessageFactory>
void log_trace_lazy_if_enabled(
    exasol::udf::v2::provider::logging::LoggingProviderIf& logging_provider,
    MessageFactory&& message_factory,
    const std::source_location code_location)
{
#ifndef NDEBUG
    if (logging_provider.isVerboseEnabled())
    {
        log_trace(logging_provider, std::forward<MessageFactory>(message_factory)(), code_location);
    }
#else
    static_cast<void>(logging_provider);
    static_cast<void>(message_factory);
    static_cast<void>(code_location);
#endif
}

/**
 * Logs the start and end of a function scope to the provider verbose stream.
 *
 * The messages are emitted only in Debug builds. Logging failures are ignored
 * so creating or destroying a trace cannot throw.
 */
class FunctionTraceLog final
{
public:
    FunctionTraceLog(exasol::udf::v2::provider::logging::LoggingProviderIf& logging_provider,
                     std::source_location code_location) noexcept;
    ~FunctionTraceLog() noexcept;

    FunctionTraceLog(const FunctionTraceLog&)            = delete;
    FunctionTraceLog& operator=(const FunctionTraceLog&) = delete;
    FunctionTraceLog(FunctionTraceLog&&)                 = delete;
    FunctionTraceLog& operator=(FunctionTraceLog&&)      = delete;

private:
#ifndef NDEBUG
    exasol::udf::v2::provider::logging::LoggingProviderIf& logging_provider;
    std::source_location code_location;
    bool enabled = false;
#endif
};

} // namespace exasol::udf::v2::utils::logging

#endif // EXASOL_UDF_V2_UTILS_LOG_UTILS_H
