#include <exasol/udf/v2/context/background_io_thread.hpp>

#include <exasol/udf/v2/event_fd_factory.hpp>
#include <exasol/udf/v2/protocol_codec.hpp>
#include <exasol/udf/v2/socket/unix_socket.hpp>
#include <exasol/udf/v2/udf_protocol.hpp>

#include <fcntl.h>
#include <poll.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace exasol::udf::v2::context
{
namespace
{

    constexpr std::size_t frame_prefix_size  = sizeof(std::uint32_t);
    constexpr std::size_t maximum_frame_size = 64U * 1024U * 1024U;
    using DecodedFrame = std::shared_ptr<const exasol::udf::protocol::DecodedFrame>;

    struct PendingFrame
    {
        DecodedFrame frame;
    };

    struct CallState
    {
        std::deque<PendingFrame> inbound;
        bool accepted{false};
        bool closed{false};
    };

    struct ContextState
    {
        mutable std::mutex mutex;
        std::condition_variable changed;
        std::unordered_map<std::uint64_t, CallState> calls;
        std::deque<std::uint64_t> unaccepted_calls;
        std::deque<PendingFrame> control;
        interface::OperationStatus terminal{interface::OperationStatus::Ok};

        [[nodiscard]] bool has_message() const
        {
            if (!control.empty() || !unaccepted_calls.empty())
            {
                return true;
            }
            return std::any_of(calls.begin(), calls.end(),
                               [](const auto& entry) { return !entry.second.inbound.empty(); });
        }
    };

    [[nodiscard]] interface::OperationStatus terminal_status(const ContextState& state)
    {
        return state.terminal == interface::OperationStatus::Ok
                   ? interface::OperationStatus::PeerClosed
                   : state.terminal;
    }

    [[nodiscard]] interface::MessageStatus message_status(const ContextState& state)
    {
        if (state.has_message())
        {
            return interface::MessageStatus::MessageAvailable;
        }
        switch (state.terminal)
        {
            case interface::OperationStatus::Ok:
                return interface::MessageStatus::NoMessage;
            case interface::OperationStatus::TimedOut:
                return interface::MessageStatus::TimedOut;
            case interface::OperationStatus::Cancelled:
                return interface::MessageStatus::Cancelled;
            case interface::OperationStatus::PeerClosed:
                return interface::MessageStatus::PeerClosed;
            case interface::OperationStatus::ProtocolError:
            case interface::OperationStatus::TransportError:
                return interface::MessageStatus::ConnectionError;
        }
        return interface::MessageStatus::ConnectionError;
    }

    [[nodiscard]] std::optional<std::chrono::steady_clock::time_point> deadline(
        interface::Timeout timeout)
    {
        return timeout ? std::optional(std::chrono::steady_clock::now() + *timeout) : std::nullopt;
    }

    class Call final : public interface::Call
    {
    public:
        Call(std::shared_ptr<ContextState> state,
             BackgroundIoThreadInterface& backend,
             const std::uint64_t stream_id)
            : state_(std::move(state)), backend_(backend), stream_id_(stream_id)
        {
        }

        [[nodiscard]] interface::MessageStatus receive_status() const override
        {
            const std::lock_guard lock(state_->mutex);
            const auto iterator = state_->calls.find(stream_id_);
            if (iterator == state_->calls.end())
            {
                return interface::MessageStatus::ConnectionError;
            }
            if (!iterator->second.inbound.empty())
            {
                return interface::MessageStatus::MessageAvailable;
            }
            return state_->terminal == interface::OperationStatus::Ok
                       ? interface::MessageStatus::NoMessage
                       : message_status(*state_);
        }

        [[nodiscard]] interface::Result<interface::CallMessageView> receive(
            interface::Timeout timeout) override
        {
            std::unique_lock lock(state_->mutex);
            const auto end   = deadline(timeout);
            const auto ready = [this] {
                const auto iterator = state_->calls.find(stream_id_);
                return iterator == state_->calls.end() || !iterator->second.inbound.empty() ||
                       state_->terminal != interface::OperationStatus::Ok;
            };
            if (!ready() && end && !state_->changed.wait_until(lock, *end, ready))
            {
                return interface::Result<interface::CallMessageView>::failure(
                    interface::OperationStatus::TimedOut);
            }
            if (!ready())
            {
                state_->changed.wait(lock, ready);
            }
            const auto iterator = state_->calls.find(stream_id_);
            if (iterator == state_->calls.end())
            {
                return interface::Result<interface::CallMessageView>::failure(
                    interface::OperationStatus::ProtocolError);
            }
            if (iterator->second.inbound.empty())
            {
                return interface::Result<interface::CallMessageView>::failure(
                    terminal_status(*state_));
            }
            auto frame = std::move(iterator->second.inbound.front().frame);
            iterator->second.inbound.pop_front();
            return interface::Result<interface::CallMessageView>::success(
                interface::CallMessageView(std::move(frame)));
        }

        [[nodiscard]] interface::Result<void> send(interface::CallMessageBuilder message,
                                                   interface::Timeout timeout) override
        {
            return backend_.send_call(stream_id_, std::move(message), timeout);
        }

    private:
        std::shared_ptr<ContextState> state_;
        BackgroundIoThreadInterface& backend_;
        std::uint64_t stream_id_;
    };

    class ControlStream final : public interface::ControlStream
    {
    public:
        ControlStream(std::shared_ptr<ContextState> state, BackgroundIoThreadInterface& backend)
            : state_(std::move(state)), backend_(backend)
        {
        }

        [[nodiscard]] interface::MessageStatus receive_status() const override
        {
            const std::lock_guard lock(state_->mutex);
            if (!state_->control.empty())
            {
                return interface::MessageStatus::MessageAvailable;
            }
            return state_->terminal == interface::OperationStatus::Ok
                       ? interface::MessageStatus::NoMessage
                       : message_status(*state_);
        }

        [[nodiscard]] interface::Result<interface::ControlMessageView> receive(
            interface::Timeout timeout) override
        {
            std::unique_lock lock(state_->mutex);
            const auto end   = deadline(timeout);
            const auto ready = [this] {
                return !state_->control.empty() ||
                       state_->terminal != interface::OperationStatus::Ok;
            };
            if (!ready() && end && !state_->changed.wait_until(lock, *end, ready))
            {
                return interface::Result<interface::ControlMessageView>::failure(
                    interface::OperationStatus::TimedOut);
            }
            if (!ready())
            {
                state_->changed.wait(lock, ready);
            }
            if (state_->control.empty())
            {
                return interface::Result<interface::ControlMessageView>::failure(
                    terminal_status(*state_));
            }
            auto frame = std::move(state_->control.front().frame);
            state_->control.pop_front();
            return interface::Result<interface::ControlMessageView>::success(
                interface::ControlMessageView(std::move(frame)));
        }

        [[nodiscard]] interface::Result<void> send(
            interface::ControlMessageBuilder message) override
        {
            return backend_.send_control(std::move(message));
        }

    private:
        std::shared_ptr<ContextState> state_;
        BackgroundIoThreadInterface& backend_;
    };

    class BackgroundIoThread final : public BackgroundIoThreadInterface
    {
    public:
        explicit BackgroundIoThread(socket::OwnedFileDescriptor connection)
            : socket_(socket::UnixSocket::adopt_native_handle(std::move(connection))),
              state_(std::make_shared<ContextState>()),
              control_stream_(state_, *this),
              stop_fd_(make_linux_event_fd())
        {
            const int flags = ::fcntl(socket_.native_handle(), F_GETFL, 0);
            if (flags == -1 || ::fcntl(socket_.native_handle(), F_SETFL, flags | O_NONBLOCK) == -1)
            {
                throw std::system_error(errno, std::generic_category(),
                                        "set context socket non-blocking");
            }
        }

        ~BackgroundIoThread() override
        {
            request_stop();
            join();
        }

        void start() override
        {
            std::lock_guard lock(thread_mutex_);
            if (!thread_.joinable())
            {
                thread_ = std::thread([this] { run(); });
            }
        }

        void request_stop() noexcept override
        {
            stop_requested_.store(true, std::memory_order_release);
            try
            {
                stop_fd_->write_notification();
            }
            catch (const std::system_error&)
            {
            }
            state_->changed.notify_all();
        }

        void join() noexcept override
        {
            std::lock_guard lock(thread_mutex_);
            if (thread_.joinable())
            {
                thread_.join();
            }
        }

        [[nodiscard]] interface::MessageStatus receive_status() const override
        {
            const std::lock_guard lock(state_->mutex);
            return message_status(*state_);
        }

        [[nodiscard]] bool is_new_inbound_call_available() const override
        {
            const std::lock_guard lock(state_->mutex);
            return !state_->unaccepted_calls.empty();
        }

        [[nodiscard]] interface::MessageStatus wait_for_message(interface::Timeout timeout) override
        {
            std::unique_lock lock(state_->mutex);
            const auto end   = deadline(timeout);
            const auto ready = [this] {
                return state_->has_message() || state_->terminal != interface::OperationStatus::Ok;
            };
            if (!ready() && end && !state_->changed.wait_until(lock, *end, ready))
            {
                return interface::MessageStatus::TimedOut;
            }
            if (!ready())
            {
                state_->changed.wait(lock, ready);
            }
            return message_status(*state_);
        }

        [[nodiscard]] interface::Result<std::unique_ptr<interface::Call>> accept_call() override
        {
            std::unique_lock lock(state_->mutex);
            state_->changed.wait(lock, [this] {
                return !state_->unaccepted_calls.empty() ||
                       state_->terminal != interface::OperationStatus::Ok;
            });
            if (state_->unaccepted_calls.empty())
            {
                return interface::Result<std::unique_ptr<interface::Call>>::failure(
                    terminal_status(*state_));
            }
            const auto stream_id = state_->unaccepted_calls.front();
            state_->unaccepted_calls.pop_front();
            state_->calls.at(stream_id).accepted = true;
            return interface::Result<std::unique_ptr<interface::Call>>::success(
                std::make_unique<Call>(state_, *this, stream_id));
        }

        [[nodiscard]] interface::Result<std::unique_ptr<interface::Call>> open_call(
            interface::CallMessageBuilder message) override
        {
            auto opening = std::move(message).take();
            if (!opening.open_call.has_value())
            {
                return interface::Result<std::unique_ptr<interface::Call>>::failure(
                    interface::OperationStatus::ProtocolError,
                    interface::ErrorInfo{"missing_open_call",
                                         "an opening call message is required"});
            }
            const auto stream_id = next_stream_id_++;
            auto encoded         = protocol::encode_call(stream_id, std::move(opening));
            if (!encoded)
            {
                return interface::Result<std::unique_ptr<interface::Call>>::failure(encoded.status,
                                                                                    encoded.error);
            }
            enqueue(std::move(encoded.value->bytes));
            {
                std::lock_guard lock(state_->mutex);
                state_->calls.emplace(stream_id, CallState{}).first->second.accepted = true;
            }
            return interface::Result<std::unique_ptr<interface::Call>>::success(
                std::make_unique<Call>(state_, *this, stream_id));
        }

        [[nodiscard]] interface::Result<void> send_call(const std::uint64_t stream_id,
                                                        interface::CallMessageBuilder message,
                                                        interface::Timeout) override
        {
            auto value = std::move(message).take();
            if (value.open_call.has_value())
            {
                return interface::Result<void>::failure(
                    interface::OperationStatus::ProtocolError,
                    interface::ErrorInfo{"open_call_after_open", "call is already open"});
            }
            auto encoded = protocol::encode_call(stream_id, std::move(value));
            if (!encoded)
            {
                return interface::Result<void>::failure(encoded.status, encoded.error);
            }
            enqueue(std::move(encoded.value->bytes));
            return interface::Result<void>::success();
        }

        [[nodiscard]] interface::Result<void> send_control(
            interface::ControlMessageBuilder message) override
        {
            auto encoded = protocol::encode_control(std::move(message).take());
            if (!encoded)
            {
                return interface::Result<void>::failure(encoded.status, encoded.error);
            }
            enqueue(std::move(encoded.value->bytes));
            return interface::Result<void>::success();
        }

        [[nodiscard]] interface::ControlStream& control_stream() override
        {
            return control_stream_;
        }

    private:
        void run() noexcept
        {
            try
            {
                while (!stop_requested_.load(std::memory_order_acquire))
                {
                    const bool has_outbound = has_outbound_frames();
                    pollfd descriptors[]    = {
                        {socket_.native_handle(),
                         static_cast<short>(POLLIN | POLLERR | POLLHUP |
                                            (has_outbound ? POLLOUT : 0)),
                         0},
                        {stop_fd_->native_handle(), POLLIN, 0},
                    };
                    const int result = ::poll(descriptors, 2, -1);
                    if (result == -1)
                    {
                        if (errno == EINTR)
                        {
                            continue;
                        }
                        fail(interface::OperationStatus::TransportError);
                        return;
                    }
                    if ((descriptors[1].revents & POLLIN) != 0)
                    {
                        stop_fd_->read_notification();
                        if (stop_requested_.load(std::memory_order_acquire))
                        {
                            return;
                        }
                    }
                    if ((descriptors[0].revents & (POLLERR | POLLHUP)) != 0)
                    {
                        fail(interface::OperationStatus::PeerClosed);
                        return;
                    }
                    if ((descriptors[0].revents & POLLIN) != 0)
                    {
                        read_available();
                    }
                    if ((descriptors[0].revents & POLLOUT) != 0)
                    {
                        write_available();
                    }
                }
            }
            catch (const std::system_error&)
            {
                fail(interface::OperationStatus::TransportError);
            }
            socket_.close();
        }

        void read_available()
        {
            std::byte buffer[16U * 1024U];
            for (;;)
            {
                const auto count = socket_.read_some(std::span<std::byte>(buffer));
                if (count == 0)
                {
                    fail(interface::OperationStatus::PeerClosed);
                    return;
                }
                receive_buffer_.insert(receive_buffer_.end(), buffer, buffer + count);
                decode_available();
                if (count < std::size(buffer))
                {
                    return;
                }
            }
        }

        void decode_available()
        {
            while (receive_buffer_.size() >= frame_prefix_size)
            {
                std::uint32_t payload_size = 0;
                std::memcpy(&payload_size, receive_buffer_.data(), sizeof(payload_size));
                const std::size_t frame_size = frame_prefix_size + payload_size;
                if (frame_size < frame_prefix_size || frame_size > maximum_frame_size)
                {
                    fail(interface::OperationStatus::ProtocolError);
                    return;
                }
                if (receive_buffer_.size() < frame_size)
                {
                    return;
                }
                const auto decoded =
                    exasol::udf::protocol::DecodedFrame::decode(std::span<const std::uint8_t>(
                        reinterpret_cast<const std::uint8_t*>(receive_buffer_.data()), frame_size));
                if (!decoded)
                {
                    fail(interface::OperationStatus::ProtocolError);
                    return;
                }
                dispatch(std::make_shared<const exasol::udf::protocol::DecodedFrame>(*decoded));
                receive_buffer_.erase(receive_buffer_.begin(),
                                      receive_buffer_.begin() + frame_size);
            }
        }

        void dispatch(DecodedFrame frame)
        {
            const auto stream_id = frame->frame().stream_id();
            std::lock_guard lock(state_->mutex);
            if (stream_id == 0)
            {
                state_->control.push_back({std::move(frame)});
            }
            else
            {
                auto& call           = state_->calls[stream_id];
                const bool was_empty = call.inbound.empty();
                call.inbound.push_back({std::move(frame)});
                if (was_empty && !call.accepted)
                {
                    state_->unaccepted_calls.push_back(stream_id);
                }
            }
            state_->changed.notify_all();
        }

        void fail(const interface::OperationStatus status)
        {
            {
                std::lock_guard lock(state_->mutex);
                if (state_->terminal == interface::OperationStatus::Ok)
                {
                    state_->terminal = status;
                }
            }
            state_->changed.notify_all();
        }

        void enqueue(std::vector<std::byte> frame)
        {
            {
                std::lock_guard lock(outbound_mutex_);
                outbound_.push_back(std::move(frame));
            }
            stop_fd_->write_notification();
        }

        [[nodiscard]] bool has_outbound_frames() const
        {
            const std::lock_guard lock(outbound_mutex_);
            return !outbound_.empty();
        }

        void write_available()
        {
            std::lock_guard lock(outbound_mutex_);
            while (!outbound_.empty())
            {
                try
                {
                    const auto written = socket_.write_some(
                        std::span<const std::byte>(outbound_.front()).subspan(outbound_offset_));
                    outbound_offset_ += written;
                    if (outbound_offset_ == outbound_.front().size())
                    {
                        outbound_.pop_front();
                        outbound_offset_ = 0;
                    }
                }
                catch (const std::system_error& error)
                {
                    if (error.code() == std::errc::resource_unavailable_try_again)
                    {
                        return;
                    }
                    throw;
                }
            }
        }

        socket::UnixSocket socket_;
        std::shared_ptr<ContextState> state_;
        ControlStream control_stream_;
        std::unique_ptr<EventFd> stop_fd_;
        std::vector<std::byte> receive_buffer_;
        mutable std::mutex outbound_mutex_;
        std::deque<std::vector<std::byte>> outbound_;
        std::size_t outbound_offset_{0};
        std::uint64_t next_stream_id_{1};
        std::atomic_bool stop_requested_{false};
        std::mutex thread_mutex_;
        std::thread thread_;
    };

} // namespace

std::unique_ptr<BackgroundIoThreadInterface> make_background_io_thread(
    socket::OwnedFileDescriptor connection)
{
    return std::make_unique<BackgroundIoThread>(std::move(connection));
}

} // namespace exasol::udf::v2::context
