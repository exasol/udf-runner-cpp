#pragma once

#include <string>
#include <utility>

namespace exasol::udf::v2::interface
{

/// Machine-readable and human-readable operation error details.
class ErrorInfo
{
public:
    ErrorInfo(std::string code, std::string message)
        : code_(std::move(code)), message_(std::move(message))
    {
    }
    ErrorInfo(const ErrorInfo&)            = default;
    ErrorInfo(ErrorInfo&&) noexcept        = default;
    ErrorInfo& operator=(const ErrorInfo&) = delete;
    ErrorInfo& operator=(ErrorInfo&&)      = delete;
    ~ErrorInfo()                           = default;

    [[nodiscard]] const std::string& code() const noexcept
    {
        return code_;
    }

    [[nodiscard]] const std::string& message() const noexcept
    {
        return message_;
    }

private:
    std::string code_;
    std::string message_;
};

} // namespace exasol::udf::v2::interface
