#pragma once

#include <exception>
#include <source_location>
#include <stacktrace>
#include <string>
#include <utility>

namespace exasol::udf::v2
{

class Exception : public std::exception
{
public:
    explicit Exception(std::string message,
                       std::source_location location = std::source_location::current())
        : message_(std::move(message)),
          location_(location),
          stacktrace_(std::stacktrace::current(1))
    {
    }

    [[nodiscard]] const char* what() const noexcept override
    {
        return message_.c_str();
    }

    [[nodiscard]] const std::source_location& location() const noexcept
    {
        return location_;
    }

    [[nodiscard]] const std::stacktrace& stacktrace() const noexcept
    {
        return stacktrace_;
    }

private:
    std::string message_;
    std::source_location location_;
    std::stacktrace stacktrace_;
};

} // namespace exasol::udf::v2
