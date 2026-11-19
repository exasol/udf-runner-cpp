#include <exasol/udf/v2/provider/logging_provider.h>
#include <exasol/udf/v2/utils/log_utils.h>

#include <gtest/gtest.h>

#include <cstdlib>
#include <initializer_list>
#include <ios>
#include <iostream>
#include <sstream>
#include <source_location>
#include <string>
#include <string_view>

namespace
{

using exasol::udf::v2::provider::logging::LoggingProvider;
namespace log_utils = exasol::udf::v2::utils::logging;

class ScopedLogLevel final
{
public:
    explicit ScopedLogLevel(const char* value)
    {
        if (const char* previous = std::getenv("UDF_LOG_LEVEL"); previous != nullptr)
        {
            original     = previous;
            had_original = true;
        }
        set(value);
    }

    ~ScopedLogLevel()
    {
        set(had_original ? original.c_str() : nullptr);
    }

    ScopedLogLevel(const ScopedLogLevel&)            = delete;
    ScopedLogLevel& operator=(const ScopedLogLevel&) = delete;
    ScopedLogLevel(ScopedLogLevel&&)                 = delete;
    ScopedLogLevel& operator=(ScopedLogLevel&&)      = delete;

private:
    static void set(const char* value)
    {
        if (value == nullptr)
        {
            EXPECT_EQ(unsetenv("UDF_LOG_LEVEL"), 0);
        }
        else
        {
            EXPECT_EQ(setenv("UDF_LOG_LEVEL", value, 1), 0);
        }
    }

    std::string original;
    bool had_original = false;
};

class ThrowingLoggingProvider final : public exasol::udf::v2::provider::logging::LoggingProviderIf
{
public:
    [[noreturn]] std::ostream& error() override
    {
        fail();
    }
    [[noreturn]] std::ostream& warning() override
    {
        fail();
    }
    [[noreturn]] std::ostream& notice() override
    {
        fail();
    }
    [[noreturn]] std::ostream& debug() override
    {
        fail();
    }
    [[noreturn]] std::ostream& verbose() override
    {
        fail();
    }
    [[nodiscard]] bool isNoticeEnabled() const override
    {
        return true;
    }
    [[nodiscard]] bool isDebugEnabled() const override
    {
        return true;
    }
    [[nodiscard]] bool isVerboseEnabled() const override
    {
        return true;
    }

private:
    [[noreturn]] static void fail()
    {
        throw std::ios_base::failure("log stream unavailable");
    }
};

#ifndef NDEBUG
class FailingTraceLoggingProvider final
    : public exasol::udf::v2::provider::logging::LoggingProviderIf
{
public:
    enum class Failure
    {
        Enable,
        Start,
        End
    };

    explicit FailingTraceLoggingProvider(const Failure failure) : failure(failure)
    {
    }

    std::ostream& error() override
    {
        return output;
    }
    std::ostream& warning() override
    {
        return output;
    }
    std::ostream& notice() override
    {
        return output;
    }
    std::ostream& debug() override
    {
        return output;
    }
    std::ostream& verbose() override
    {
        ++verbose_calls;
        if ((failure == Failure::Start && verbose_calls == 1) ||
            (failure == Failure::End && verbose_calls == 2))
        {
            throw std::ios_base::failure("trace stream unavailable");
        }
        return output;
    }
    [[nodiscard]] bool isNoticeEnabled() const override
    {
        return true;
    }
    [[nodiscard]] bool isDebugEnabled() const override
    {
        return true;
    }
    [[nodiscard]] bool isVerboseEnabled() const override
    {
        if (failure == Failure::Enable)
        {
            throw std::ios_base::failure("trace level unavailable");
        }
        return true;
    }
    [[nodiscard]] int verbose_call_count() const noexcept
    {
        return verbose_calls;
    }

private:
    Failure failure;
    int verbose_calls = 0;
    std::ostringstream output;
};
#endif

std::string capture_messages(LoggingProvider& provider)
{
    const auto location = std::source_location::current();
    testing::internal::CaptureStderr();
    log_utils::log_error(provider, "error message", location);
    log_utils::log_warning(provider, "warning message", location);
    log_utils::log_notice(provider, "notice message", location);
    log_utils::log_debug(provider, "debug message", location);
    log_utils::log_trace(provider, "verbose message", location);
    return testing::internal::GetCapturedStderr();
}

void expect_message(const std::string_view output,
                    const std::string_view severity,
                    const bool expected)
{
    const std::string prefix = "[" + std::string(severity) + " '";
    EXPECT_EQ(output.find(prefix) != std::string_view::npos, expected) << severity;
}

TEST(LoggingProviderTest, EnablesEachConfiguredLevel)
{
    for (int level = 1; level <= 4; ++level)
    {
        const ScopedLogLevel environment(std::to_string(level).c_str());
        LoggingProvider provider;
        EXPECT_EQ(provider.isNoticeEnabled(), level >= 2);
        EXPECT_EQ(provider.isDebugEnabled(), level >= 3);
#ifndef NDEBUG
        EXPECT_EQ(provider.isVerboseEnabled(), level >= 4);
#else
        EXPECT_FALSE(provider.isVerboseEnabled());
#endif

        const std::string output = capture_messages(provider);
        expect_message(output, "ERROR", true);
        expect_message(output, "WARNING", true);
        expect_message(output, "NOTICE", level >= 2);
        expect_message(output, "DEBUG", level >= 3);
#ifndef NDEBUG
        expect_message(output, "VERBOSE", level >= 4);
#else
        expect_message(output, "VERBOSE", false);
#endif
        EXPECT_NE(output.find("threadId="), std::string::npos);
    }
}

TEST(LoggingProviderTest, DefaultsToLevelOneForMissingOrInvalidValues)
{
    for (const char* value :
         std::initializer_list<const char*>{nullptr, "", "0", "5", "2x", " 3", "-1"})
    {
        const ScopedLogLevel environment(value);
        LoggingProvider provider;
        EXPECT_FALSE(provider.isNoticeEnabled());
        EXPECT_FALSE(provider.isDebugEnabled());
        EXPECT_FALSE(provider.isVerboseEnabled());
    }
}

TEST(LoggingProviderTest, KeepsLevelSelectedAtConstruction)
{
    const ScopedLogLevel first_level("2");
    LoggingProvider original;
    {
        const ScopedLogLevel second_level("4");
        EXPECT_TRUE(original.isNoticeEnabled());
        EXPECT_FALSE(original.isDebugEnabled());
        LoggingProvider updated;
        EXPECT_TRUE(updated.isDebugEnabled());
    }
}

TEST(LoggingProviderTest, DiscardsWritesToDisabledStreams)
{
    const ScopedLogLevel environment("1");
    LoggingProvider provider;
    testing::internal::CaptureStderr();
    provider.notice() << "notice" << std::endl;
    provider.debug() << "debug" << std::endl;
    provider.verbose() << "verbose" << std::endl;
    EXPECT_TRUE(testing::internal::GetCapturedStderr().empty());
}

TEST(LogUtilsTest, LazyMessagesRunOnlyForEnabledSeverities)
{
    const auto location = std::source_location::current();
    for (int level = 1; level <= 4; ++level)
    {
        const ScopedLogLevel environment(std::to_string(level).c_str());
        LoggingProvider provider;
        int notice_calls  = 0;
        int debug_calls   = 0;
        int verbose_calls = 0;
        testing::internal::CaptureStderr();
        log_utils::log_notice_lazy_if_enabled(
            provider,
            [&notice_calls] {
                ++notice_calls;
                return std::string("notice");
            },
            location);
        log_utils::log_debug_lazy_if_enabled(
            provider,
            [&debug_calls] {
                ++debug_calls;
                return std::string("debug");
            },
            location);
        log_utils::log_trace_lazy_if_enabled(
            provider,
            [&verbose_calls] {
                ++verbose_calls;
                return std::string("verbose");
            },
            location);
        const std::string output = testing::internal::GetCapturedStderr();
        EXPECT_EQ(notice_calls, level >= 2);
        EXPECT_EQ(debug_calls, level >= 3);
#ifndef NDEBUG
        EXPECT_EQ(verbose_calls, level >= 4);
#else
        EXPECT_EQ(verbose_calls, 0);
#endif
        EXPECT_EQ(output.find("[NOTICE") != std::string::npos, level >= 2);
        EXPECT_EQ(output.find("[DEBUG") != std::string::npos, level >= 3);
    }
}

TEST(LogUtilsTest, FunctionTraceUsesVerboseLevel)
{
    for (const char* level : {"3", "4"})
    {
        const ScopedLogLevel environment(level);
        LoggingProvider provider;
        testing::internal::CaptureStderr();
        {
            log_utils::FunctionTraceLog trace(provider, std::source_location::current());
        }
        const std::string output = testing::internal::GetCapturedStderr();
#ifndef NDEBUG
        const bool expected = std::string_view(level) == "4";
#else
        const bool expected = false;
#endif
        EXPECT_EQ(output.find("Start") != std::string::npos, expected);
        EXPECT_EQ(output.find("End") != std::string::npos, expected);
    }
}

#ifndef NDEBUG
TEST(LogUtilsTest, FunctionTraceIgnoresLoggingFailures)
{
    for (const auto failure :
         {FailingTraceLoggingProvider::Failure::Enable, FailingTraceLoggingProvider::Failure::Start,
          FailingTraceLoggingProvider::Failure::End})
    {
        FailingTraceLoggingProvider provider(failure);
        EXPECT_NO_THROW(
            { log_utils::FunctionTraceLog trace(provider, std::source_location::current()); });
        EXPECT_EQ(provider.verbose_call_count(),
                  failure == FailingTraceLoggingProvider::Failure::Enable  ? 0
                  : failure == FailingTraceLoggingProvider::Failure::Start ? 1
                                                                           : 2);
    }
}
#endif

TEST(LogUtilsTest, PropagatesProviderErrors)
{
    ThrowingLoggingProvider provider;
    const auto location = std::source_location::current();
    EXPECT_THROW(log_utils::log_error(provider, "error", location), std::ios_base::failure);
    EXPECT_THROW(log_utils::log_warning(provider, "warning", location), std::ios_base::failure);
    EXPECT_THROW(log_utils::log_notice(provider, "notice", location), std::ios_base::failure);
    EXPECT_THROW(log_utils::log_debug(provider, "debug", location), std::ios_base::failure);
#ifndef NDEBUG
    EXPECT_THROW(log_utils::log_trace(provider, "trace", location), std::ios_base::failure);
#endif
}

TEST(LogUtilsTest, FormatsStrings)
{
    EXPECT_EQ(log_utils::format_strings({}), "[]");
    EXPECT_EQ(log_utils::format_strings({"first"}), "[first]");
    EXPECT_EQ(log_utils::format_strings({"first", "second"}), "[first, second]");
}

} // namespace
