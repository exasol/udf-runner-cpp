#pragma once

#include <exasol/udf/v2/interface/status.hpp>
#include <exasol/udf/v2/internal/background_thread_interface.hpp>

#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <unordered_set>

namespace exasol::udf::v2::internal
{

enum class EndpointRole
{
    Client,
    Server
};

enum class DispatchResult
{
    Routed,
    Discarded,
    ProtocolError,
    ConnectionShutdown
};

enum class CallMessageResult
{
    Message,
    Empty,
    PeerClosed
};

struct CallMessageRead
{
    CallMessageResult result{CallMessageResult::Empty};
    std::unique_ptr<interface::CallMessageView> message;
};

/// Context-owned routing and logical stream state, accessed only by its worker thread.
class StreamRegistry final
{
public:
    explicit StreamRegistry(EndpointRole endpoint_role);

    [[nodiscard]] std::optional<std::uint64_t> reserve_local_stream_id();
    [[nodiscard]] bool register_local_call(std::uint64_t stream_id, bool local_close_accepted);
    void rollback_local_call(std::uint64_t stream_id) noexcept;

    [[nodiscard]] DispatchResult dispatch(InboundMessage&& message);

    [[nodiscard]] bool has_unaccepted_call() const noexcept;
    [[nodiscard]] std::optional<std::uint64_t> take_next_unaccepted_call();
    [[nodiscard]] bool has_any_pending_message() const noexcept;
    [[nodiscard]] interface::MessageStatus call_receive_status(
        std::uint64_t stream_id) const noexcept;
    [[nodiscard]] interface::MessageStatus control_receive_status() const noexcept;
    [[nodiscard]] CallMessageRead take_call_message(std::uint64_t stream_id);
    [[nodiscard]] std::unique_ptr<interface::ControlMessageView> take_control_message();

    [[nodiscard]] interface::OperationStatus call_send_status(
        std::uint64_t stream_id) const noexcept;
    [[nodiscard]] bool mark_local_close_accepted(std::uint64_t stream_id) noexcept;
    void clear() noexcept;

    [[nodiscard]] EndpointRole endpoint_role() const noexcept;
    [[nodiscard]] std::uint64_t local_high_water_mark() const noexcept;
    [[nodiscard]] std::uint64_t peer_high_water_mark() const noexcept;

private:
    struct CallState
    {
        std::deque<std::unique_ptr<interface::CallMessageView>> pending_messages;
        bool local_close_accepted{};
        bool peer_close_received{};
    };

    [[nodiscard]] bool has_local_endpoint_parity(std::uint64_t stream_id) const noexcept;
    EndpointRole endpoint_role_;
    std::uint64_t next_local_stream_id_;
    std::uint64_t local_high_water_mark_{};
    std::uint64_t peer_high_water_mark_{};
    bool local_id_space_exhausted_{};
    std::unordered_set<std::uint64_t> opened_local_stream_ids_;
    std::map<std::uint64_t, CallState> active_calls_;
    std::deque<std::uint64_t> unaccepted_call_ids_;
    std::deque<std::unique_ptr<interface::ControlMessageView>> control_pending_;
};

} // namespace exasol::udf::v2::internal
