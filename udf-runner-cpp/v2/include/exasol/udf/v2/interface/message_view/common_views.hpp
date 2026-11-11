#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>

namespace exasol::udf::v2::interface
{

enum class PayloadType
{
    String,
    Binary
};

class PayloadValueView
{
public:
    virtual ~PayloadValueView()                                                         = default;
    PayloadValueView(const PayloadValueView&)                                           = delete;
    PayloadValueView& operator=(const PayloadValueView&)                                = delete;
    PayloadValueView(PayloadValueView&&)                                                = delete;
    PayloadValueView& operator=(PayloadValueView&&)                                     = delete;
    [[nodiscard]] virtual PayloadType type() const noexcept                             = 0;
    [[nodiscard]] virtual std::optional<std::string_view> string_value() const noexcept = 0;
    [[nodiscard]] virtual std::optional<std::span<const std::uint8_t>> binary_value()
        const noexcept = 0;

protected:
    PayloadValueView() = default;
};

class PayloadView
{
public:
    virtual ~PayloadView()                                                      = default;
    PayloadView(const PayloadView&)                                             = delete;
    PayloadView& operator=(const PayloadView&)                                  = delete;
    PayloadView(PayloadView&&)                                                  = delete;
    PayloadView& operator=(PayloadView&&)                                       = delete;
    [[nodiscard]] virtual std::optional<std::string_view> name() const noexcept = 0;
    [[nodiscard]] virtual std::unique_ptr<PayloadValueView> value() const       = 0;

protected:
    PayloadView() = default;
};

class PayloadsView
{
public:
    virtual ~PayloadsView()                                                        = default;
    PayloadsView(const PayloadsView&)                                              = delete;
    PayloadsView& operator=(const PayloadsView&)                                   = delete;
    PayloadsView(PayloadsView&&)                                                   = delete;
    PayloadsView& operator=(PayloadsView&&)                                        = delete;
    [[nodiscard]] virtual std::size_t size() const noexcept                        = 0;
    [[nodiscard]] virtual std::unique_ptr<PayloadView> at(std::size_t index) const = 0;

protected:
    PayloadsView() = default;
};

class ErrorView
{
public:
    virtual ~ErrorView()                                                           = default;
    ErrorView(const ErrorView&)                                                    = delete;
    ErrorView& operator=(const ErrorView&)                                         = delete;
    ErrorView(ErrorView&&)                                                         = delete;
    ErrorView& operator=(ErrorView&&)                                              = delete;
    [[nodiscard]] virtual std::optional<std::string_view> code() const noexcept    = 0;
    [[nodiscard]] virtual std::optional<std::string_view> message() const noexcept = 0;

protected:
    ErrorView() = default;
};

} // namespace exasol::udf::v2::interface
