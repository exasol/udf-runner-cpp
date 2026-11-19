#pragma once

#include <exasol/udf/v2/provider/logging_provider.h>

#include <sstream>
#include <string>

namespace exasol::udf::v2::test_utils
{

class RecordingLoggingProvider final : public provider::logging::LoggingProviderIf
{
public:
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
        return output;
    }
    [[nodiscard]] bool isNoticeEnabled() const override
    {
        return false;
    }
    [[nodiscard]] bool isDebugEnabled() const override
    {
        return false;
    }
    [[nodiscard]] bool isVerboseEnabled() const override
    {
        return true;
    }
    [[nodiscard]] std::string messages() const
    {
        return output.str();
    }

private:
    std::ostringstream output;
};

} // namespace exasol::udf::v2::test_utils
