#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>

namespace exasol::udf::v2::interface
{

class OpenCallView
{
public:
    virtual ~OpenCallView()                                                          = default;
    [[nodiscard]] virtual std::optional<std::string_view> call_name() const noexcept = 0;

protected:
    OpenCallView() = default;
};

enum class PayloadType
{
    String,
    Binary
};

class PayloadValueView
{
public:
    virtual ~PayloadValueView()                                                         = default;
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
    [[nodiscard]] virtual std::optional<std::string_view> name() const noexcept = 0;
    [[nodiscard]] virtual std::unique_ptr<PayloadValueView> value() const       = 0;

protected:
    PayloadView() = default;
};

class PayloadsView
{
public:
    virtual ~PayloadsView()                                                        = default;
    [[nodiscard]] virtual std::size_t size() const noexcept                        = 0;
    [[nodiscard]] virtual std::unique_ptr<PayloadView> at(std::size_t index) const = 0;

protected:
    PayloadsView() = default;
};

class ResetView
{
public:
    virtual ~ResetView()                                        = default;
    [[nodiscard]] virtual std::uint64_t row_id() const noexcept = 0;

protected:
    ResetView() = default;
};

class ErrorView
{
public:
    virtual ~ErrorView()                                                           = default;
    [[nodiscard]] virtual std::optional<std::string_view> code() const noexcept    = 0;
    [[nodiscard]] virtual std::optional<std::string_view> message() const noexcept = 0;

protected:
    ErrorView() = default;
};

class CloseCallView
{
public:
    virtual ~CloseCallView() = default;

protected:
    CloseCallView() = default;
};

class VersionView
{
public:
    virtual ~VersionView()                                     = default;
    [[nodiscard]] virtual std::uint32_t major() const noexcept = 0;
    [[nodiscard]] virtual std::uint32_t minor() const noexcept = 0;

protected:
    VersionView() = default;
};

enum class Endianness
{
    Little,
    Big
};

class ServerCapabilitiesView
{
public:
    virtual ~ServerCapabilitiesView()                                                = default;
    [[nodiscard]] virtual std::unique_ptr<VersionView> supported_version() const     = 0;
    [[nodiscard]] virtual Endianness endianness() const noexcept                     = 0;
    [[nodiscard]] virtual std::uint32_t number_of_supported_workers() const noexcept = 0;

protected:
    ServerCapabilitiesView() = default;
};

class KeepAliveView
{
public:
    virtual ~KeepAliveView() = default;

protected:
    KeepAliveView() = default;
};

class CloseConnectionView
{
public:
    virtual ~CloseConnectionView() = default;

protected:
    CloseConnectionView() = default;
};

} // namespace exasol::udf::v2::interface
