#pragma once

#include <exasol/udf/v2/udf_protocol.hpp>

#include <functional>
#include <optional>

namespace exasol::udf::v2::interface
{

/// Interface for a view of one received connection-level message.
class ControlMessageView
{
public:
    virtual ~ControlMessageView()                            = default;
    ControlMessageView(const ControlMessageView&)            = delete;
    ControlMessageView& operator=(const ControlMessageView&) = delete;
    ControlMessageView(ControlMessageView&&)                 = delete;
    ControlMessageView& operator=(ControlMessageView&&)      = delete;

    /// Returns whether server capabilities are present.
    [[nodiscard]] virtual bool has_server_capabilities() const noexcept = 0;
    [[nodiscard]] virtual std::optional<std::reference_wrapper<const protocol::ServerCapabilities>>
    server_capabilities() const noexcept = 0;
    /// Returns whether a wire-level FlatBuffer keep-alive object is present.
    [[nodiscard]] virtual bool has_keep_alive() const noexcept = 0;
    /// Returns whether wire-level payloads are present.
    [[nodiscard]] virtual bool has_payloads() const noexcept = 0;
    /// Returns the wire-level FlatBuffer payload table, or an empty optional when absent.
    [[nodiscard]] virtual std::optional<std::reference_wrapper<const protocol::Payloads>> payloads()
        const noexcept = 0;
    /// Returns whether an error is present.
    [[nodiscard]] virtual bool has_error() const noexcept = 0;
    /// Returns the wire-level FlatBuffer error table, or an empty optional when absent.
    [[nodiscard]] virtual std::optional<std::reference_wrapper<const protocol::Error>> error()
        const noexcept = 0;
    /// Returns whether this message starts or acknowledges connection shutdown.
    [[nodiscard]] virtual bool has_close_connection() const noexcept = 0;

protected:
    ControlMessageView() = default;
};

} // namespace exasol::udf::v2::interface
