#pragma once

#include <exasol/udf/v2/interface/error_info.hpp>
#include <exasol/udf/v2/interface/status.hpp>

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
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

/// Lifecycle state of one background-thread component.
enum class BackgroundThreadState
{
    NotStarted,
    Running,
    ShutdownRequested,
    Stopped,
};

/// Immutable snapshot of background-thread lifecycle and terminal outcome.
struct BackgroundThreadResult
{
    BackgroundThreadState state{BackgroundThreadState::NotStarted};
    std::optional<interface::OperationStatus> terminal_status;
    std::optional<interface::ErrorInfo> error;
};

/// Private base type for a logical message submitted by the low-level Context.
class OutboundMessage
{
public:
    virtual ~OutboundMessage() = default;

protected:
    OutboundMessage() = default;
};

/// Private base type for a verified logical message delivered to the low-level Context.
class InboundMessage
{
public:
    virtual ~InboundMessage() = default;

protected:
    InboundMessage() = default;
};

using OutboundBatch = std::vector<std::unique_ptr<OutboundMessage>>;
using InboundBatch  = std::vector<std::unique_ptr<InboundMessage>>;

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
    [[nodiscard]] virtual Event wait_until(
        std::optional<std::chrono::steady_clock::time_point> deadline) = 0;

    /// Returns the current lifecycle state and, once terminal, its outcome.
    [[nodiscard]] virtual BackgroundThreadResult get_result() const = 0;

    /// Transfers complete outbound messages to the background thread without blocking.
    ///
    /// Returns false when the background thread has stopped accepting work.
    virtual bool enqueue_outbound_messages(OutboundBatch messages) = 0;

    /// Transfers one outbound message to the background thread without blocking.
    virtual bool enqueue_outbound_message(std::unique_ptr<OutboundMessage> message) = 0;

    /// Removes all currently available verified messages from the inbound queue.
    [[nodiscard]] virtual InboundBatch dequeue_inbound_messages() = 0;

    /// Requests background-thread shutdown and wakes a pending background wait.
    virtual void request_shutdown() noexcept = 0;

protected:
    BackgroundThreadInterface() = default;
};

} // namespace exasol::udf::v2::internal
