#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace exasol::udf::v2::internal
{

/// Events that wake the background context thread.
enum class BackgroundEvent : std::uint8_t
{
    None              = 0,
    OutboundAvailable = 1U << 0U,
    ShutdownRequested = 1U << 1U,
    DeadlineExpired   = 1U << 2U,
};

/// Bit set returned by a background-context wait operation.
class BackgroundEvents
{
public:
    constexpr BackgroundEvents() = default;

    constexpr explicit BackgroundEvents(const BackgroundEvent event)
        : events_(static_cast<std::uint8_t>(event))
    {
    }

    [[nodiscard]] constexpr bool contains(const BackgroundEvent event) const noexcept
    {
        return (events_ & static_cast<std::uint8_t>(event)) != 0;
    }

    constexpr void add(const BackgroundEvent event) noexcept
    {
        events_ |= static_cast<std::uint8_t>(event);
    }

private:
    std::uint8_t events_{0};
};

/// Private base type for a logical message submitted by the worker.
class OutboundMessage
{
public:
    virtual ~OutboundMessage() = default;

protected:
    OutboundMessage() = default;
};

/// Private base type for a verified logical message delivered to the worker.
class InboundMessage
{
public:
    virtual ~InboundMessage() = default;

protected:
    InboundMessage() = default;
};

using OutboundBatch = std::vector<std::unique_ptr<OutboundMessage>>;
using InboundBatch  = std::vector<std::unique_ptr<InboundMessage>>;

/// Queue operations shared by one Context owner and one background context thread.
///
/// The production implementation owns the queue endpoints and worker
/// notifications. A background-loop test injects a mock of this interface to
/// script availability and verify queue interactions. Socket I/O, framing,
/// and protocol processing are not part of this Context-to-thread boundary.
class BackgroundThreadInterface
{
public:
    virtual ~BackgroundThreadInterface() = default;

    BackgroundThreadInterface(const BackgroundThreadInterface&)            = delete;
    BackgroundThreadInterface& operator=(const BackgroundThreadInterface&) = delete;
    BackgroundThreadInterface(BackgroundThreadInterface&&)                 = delete;
    BackgroundThreadInterface& operator=(BackgroundThreadInterface&&)      = delete;

    /// Waits for outbound work, shutdown, or timer activity.
    [[nodiscard]] virtual BackgroundEvents wait_until(
        std::optional<std::chrono::steady_clock::time_point> deadline) = 0;

    /// Requests background-thread shutdown and wakes a pending wait.
    virtual void request_shutdown() noexcept = 0;

    /// Removes all currently available worker submissions from the outbound queue.
    [[nodiscard]] virtual OutboundBatch dequeue_outbound_batch() = 0;

    /// Adds verified messages to the inbound queue and notifies worker waiters.
    virtual void enqueue_inbound_batch(InboundBatch messages) = 0;

protected:
    BackgroundThreadInterface() = default;
};

} // namespace exasol::udf::v2::internal
