#ifndef EXASOL_UDF_V2_PROVIDER_LOGGING_PROVIDER_H
#define EXASOL_UDF_V2_PROVIDER_LOGGING_PROVIDER_H

#include <ostream>

namespace exasol::udf::v2::provider::logging
{

/**
 Abstract interface for severity-specific log streams.
 Disabled streams discard output.
 */
class LoggingProviderIf
{
public:
    virtual ~LoggingProviderIf()                           = default;
    LoggingProviderIf()                                    = default;
    LoggingProviderIf(const LoggingProviderIf&)            = delete;
    LoggingProviderIf& operator=(const LoggingProviderIf&) = delete;
    LoggingProviderIf(LoggingProviderIf&&)                 = delete;
    LoggingProviderIf& operator=(LoggingProviderIf&&)      = delete;
    virtual std::ostream& error()                          = 0;
    virtual std::ostream& warning()                        = 0;
    virtual std::ostream& notice()                         = 0;
    virtual std::ostream& debug()                          = 0;
    virtual std::ostream& verbose()                        = 0;
    [[nodiscard]] virtual bool isNoticeEnabled() const = 0; // NOLINT(readability-identifier-naming)
    [[nodiscard]] virtual bool isDebugEnabled() const  = 0; // NOLINT(readability-identifier-naming)
    // NOLINTNEXTLINE(readability-identifier-naming)
    [[nodiscard]] virtual bool isVerboseEnabled() const = 0;
};

/**
 * Writes enabled messages to stderr. Reads UDF_LOG_LEVEL at construction.
 * Unset or invalid values use level 1. Verbose logging is disabled in release builds.
 */
class LoggingProvider final : public LoggingProviderIf
{
public:
    LoggingProvider();

    std::ostream& error() override;
    std::ostream& warning() override;
    std::ostream& notice() override;
    std::ostream& debug() override;
    std::ostream& verbose() override;
    [[nodiscard]] bool isNoticeEnabled() const override;
    [[nodiscard]] bool isDebugEnabled() const override;
    [[nodiscard]] bool isVerboseEnabled() const override;

private:
    int level;
};


} // namespace exasol::udf::v2::provider::logging

#endif // EXASOL_UDF_V2_PROVIDER_LOGGING_PROVIDER_H
