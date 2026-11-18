#include <exasol/udf/v2/provider/logging_provider.h>

#include <cstdlib>
#include <iostream>
#include <streambuf>

namespace exasol::udf::v2::provider::logging
{

namespace
{
    class NullBuffer final : public std::streambuf
    {
    protected:
        int_type overflow(int_type character) override
        {
            return traits_type::not_eof(character);
        }
    };

    std::ostream& disabled_stream()
    {
        static NullBuffer buffer;
        static std::ostream stream(&buffer);
        return stream;
    }

    int configured_level()
    {
        const char* value = std::getenv("UDF_LOG_LEVEL");
        return value != nullptr && value[0] >= '1' && value[0] <= '4' && value[1] == '\0'
                   ? value[0] - '0'
                   : 1;
    }
} // namespace

LoggingProvider::LoggingProvider() : level(configured_level())
{
}

std::ostream& LoggingProvider::warning()
{
    return std::cerr;
}

std::ostream& LoggingProvider::error()
{
    return std::cerr;
}

std::ostream& LoggingProvider::notice()
{
    return isNoticeEnabled() ? std::cerr : disabled_stream();
}

std::ostream& LoggingProvider::debug()
{
    return isDebugEnabled() ? std::cerr : disabled_stream();
}

std::ostream& LoggingProvider::verbose()
{
    return isVerboseEnabled() ? std::cerr : disabled_stream();
}

bool LoggingProvider::isNoticeEnabled() const
{
    return level >= 2;
}

bool LoggingProvider::isDebugEnabled() const
{
    return level >= 3;
}

bool LoggingProvider::isVerboseEnabled() const
{
#ifndef NDEBUG
    return level >= 4;
#else
    return false;
#endif
}

} // namespace exasol::udf::v2::provider::logging
