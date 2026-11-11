#pragma once

#include <exasol/udf/v2/interface/message_view/common_views.hpp>
#include <exasol/udf/v2/interface/message_view/control_message_views.hpp>

#include <cstdint>
#include <memory>

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

    /// Returns the stream ID of this view's verified frame.
    [[nodiscard]] virtual std::uint64_t stream_id() const noexcept                            = 0;
    [[nodiscard]] virtual std::unique_ptr<ServerCapabilitiesView> server_capabilities() const = 0;
    /// Returns the keep-alive view, or nullptr when absent.
    [[nodiscard]] virtual std::unique_ptr<KeepAliveView> keep_alive() const = 0;
    /// Returns the payloads view, or nullptr when absent.
    [[nodiscard]] virtual std::unique_ptr<PayloadsView> payloads() const = 0;
    /// Returns the error view, or nullptr when absent.
    [[nodiscard]] virtual std::unique_ptr<ErrorView> error() const = 0;
    /// Returns the close-connection view, or nullptr when absent.
    [[nodiscard]] virtual std::unique_ptr<CloseConnectionView> close_connection() const = 0;

protected:
    ControlMessageView() = default;
};

} // namespace exasol::udf::v2::interface
