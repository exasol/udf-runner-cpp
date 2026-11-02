#pragma once

#include <exasol/udf/v2/udf_protocol.hpp>

#include <cstdint>
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

    /// Returns the stream ID of the frame backing this view.
    [[nodiscard]] virtual std::uint64_t stream_id() const noexcept = 0;
    [[nodiscard]] virtual std::optional<std::reference_wrapper<const protocol::ServerCapabilities>>
    server_capabilities() const noexcept = 0;
    /// Returns the wire-level FlatBuffer keep-alive table, or an empty optional when absent.
    [[nodiscard]] virtual std::optional<std::reference_wrapper<const protocol::KeepAlive>>
    keep_alive() const noexcept = 0;
    /// Returns the wire-level FlatBuffer payload table, or an empty optional when absent.
    [[nodiscard]] virtual std::optional<std::reference_wrapper<const protocol::Payloads>> payloads()
        const noexcept = 0;
    /// Returns the wire-level FlatBuffer error table, or an empty optional when absent.
    [[nodiscard]] virtual std::optional<std::reference_wrapper<const protocol::Error>> error()
        const noexcept = 0;
    /// Returns the wire-level FlatBuffer close-connection table, or an empty optional when absent.
    [[nodiscard]] virtual std::optional<std::reference_wrapper<const protocol::CloseConnection>>
    close_connection() const noexcept = 0;

protected:
    ControlMessageView() = default;
};

} // namespace exasol::udf::v2::interface
