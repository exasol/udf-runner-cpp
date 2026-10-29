#pragma once

#include <exasol/udf/v2/interface/error_info.hpp>
#include <exasol/udf/v2/interface/message_builder/call_message_builder.hpp>
#include <exasol/udf/v2/interface/message_builder/control_message_builder.hpp>
#include <exasol/udf/v2/interface/message_view/call_message_view.hpp>
#include <exasol/udf/v2/interface/message_view/control_message_view.hpp>

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

namespace exasol::udf::v2::internal
{

/// Event that wakes the low-level Context.
enum class Event : std::uint8_t
{
    InboundAvailable,
    ShutdownRequested,
    DeadlineExpired,
};

/// Immutable snapshot of background-thread lifecycle and terminal outcome.
struct BackgroundThreadResult
{
    Event event;
    std::optional<interface::ErrorInfo> error;
};

/// A completed builder transferred from the low-level Context for sending.
/// The background thread owns the builder and does not modify it.
using OutboundMessage = std::variant<std::unique_ptr<interface::CallMessageBuilder>,
                                     std::unique_ptr<interface::ControlMessageBuilder>>;

/// A verified message view transferred from the background thread to the low-level Context.
using InboundMessage = std::variant<std::unique_ptr<interface::CallMessageView>,
                                    std::unique_ptr<interface::ControlMessageView>>;

using OutboundBatch = std::vector<OutboundMessage>;
using InboundBatch  = std::vector<InboundMessage>;

/// Background-thread operations used by the low-level Context.
///
/// The production background-thread component implements this interface and
/// owns the queue endpoints, notifications, and thread. Tests inject a mock
/// into the low-level Context. Socket I/O, framing, and protocol processing
/// are not part of this Context-to-thread boundary.
class BackgroundThreadInterface
{
public:
    virtual ~BackgroundThreadInterface() = default;

    BackgroundThreadInterface(const BackgroundThreadInterface&)            = delete;
    BackgroundThreadInterface& operator=(const BackgroundThreadInterface&) = delete;
    BackgroundThreadInterface(BackgroundThreadInterface&&)                 = delete;
    BackgroundThreadInterface& operator=(BackgroundThreadInterface&&)      = delete;

    /// Waits for inbound work, shutdown, or an optional absolute deadline.
    ///
    /// An absent deadline waits indefinitely. Shutdown takes precedence when
    /// it is observed with inbound work.
    [[nodiscard]] virtual BackgroundThreadResult wait_until(
        std::optional<std::chrono::steady_clock::time_point> deadline) = 0;

    /// Transfers complete outbound messages to the background thread without blocking.
    ///
    /// Returns false without consuming `messages` when the background thread
    /// has stopped accepting work.
    virtual bool enqueue_outbound_messages(OutboundBatch&& messages) = 0;

    /// Transfers one outbound message to the background thread without blocking.
    ///
    /// Returns false without consuming `message` when the background thread
    /// has stopped accepting work.
    virtual bool enqueue_outbound_message(OutboundMessage&& message) = 0;

    /// Removes all currently available verified messages from the inbound queue.
    [[nodiscard]] virtual InboundBatch dequeue_inbound_messages() = 0;

    /// Requests shutdown of this context's background work.
    virtual void request_shutdown() noexcept = 0;

protected:
    BackgroundThreadInterface() = default;
};

} // namespace exasol::udf::v2::internal
