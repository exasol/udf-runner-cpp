#pragma once

#include <string>
#include <utility>

namespace exasol::udf::v2::internal
{

/// One named text or binary payload retained by a message builder.
class Payload
{
public:
    Payload(std::string name, std::string value, bool binary)
        : name_(std::move(name)), value_(std::move(value)), binary_(binary)
    {
    }

    [[nodiscard]] const std::string& name() const noexcept
    {
        return name_;
    }
    [[nodiscard]] const std::string& value() const noexcept
    {
        return value_;
    }
    [[nodiscard]] bool is_binary() const noexcept
    {
        return binary_;
    }

private:
    std::string name_;
    std::string value_;
    bool binary_;
};

} // namespace exasol::udf::v2::internal
