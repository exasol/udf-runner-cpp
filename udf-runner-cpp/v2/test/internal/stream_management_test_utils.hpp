#pragma once

#include <exasol/udf/v2/internal/background_thread_interface.hpp>
#include <exasol/udf/v2/interface/message_view/call_message_view.hpp>
#include <exasol/udf/v2/interface/message_view/control_message_view.hpp>

#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <iterator>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <gmock/gmock.h>

namespace exasol::udf::v2::internal::test_support
{

inline constexpr std::uint64_t k_control_stream_id       = 0;
inline constexpr std::uint64_t k_first_client_stream_id  = 1;
inline constexpr std::uint64_t k_second_client_stream_id = 3;
inline constexpr std::uint64_t k_first_server_stream_id  = 2;
inline constexpr std::uint64_t k_second_server_stream_id = 4;
inline constexpr std::uint64_t k_third_server_stream_id  = 6;
inline constexpr std::uint64_t k_unknown_stream_id       = 999;

/// Strict interaction tests use this mock to assert queue-boundary calls.
class MockBackgroundThread : public BackgroundThreadInterface
{
public:
    MOCK_METHOD(BackgroundThreadResult,
                wait_until,
                (std::optional<std::chrono::steady_clock::time_point> deadline),
                (override));
    MOCK_METHOD(bool, enqueue_outbound_messages, (OutboundBatch & messages), (override));
    MOCK_METHOD(bool, enqueue_outbound_message, (OutboundMessage & message), (override));
    MOCK_METHOD(InboundBatch, dequeue_inbound_messages, (), (override));
    void request_shutdown() noexcept override
    {
    }

    MockBackgroundThread()
    {
        ON_CALL(*this, wait_until(testing::_)).WillByDefault([](const auto deadline) {
            return BackgroundThreadResult{.event = deadline ? Event::DeadlineExpired
                                                            : Event::ShutdownRequested};
        });
        ON_CALL(*this, enqueue_outbound_messages(testing::_))
            .WillByDefault([this](OutboundBatch& messages) {
                return this->accept_outbound_messages(messages);
            });
        ON_CALL(*this, enqueue_outbound_message(testing::_))
            .WillByDefault([this](OutboundMessage& message) {
                return this->accept_outbound_message(message);
            });
        ON_CALL(*this, dequeue_inbound_messages()).WillByDefault([] { return InboundBatch{}; });
    }

    bool accept_outbound_messages(OutboundBatch& messages)
    {
        outbound_.insert(outbound_.end(), std::make_move_iterator(messages.begin()),
                         std::make_move_iterator(messages.end()));
        return true;
    }

    bool accept_outbound_message(OutboundMessage& message)
    {
        outbound_.push_back(std::move(message));
        return true;
    }

private:
    OutboundBatch outbound_;
};

class FakeOpenCallView final : public interface::OpenCallView
{
public:
    [[nodiscard]] std::optional<std::string_view> call_name() const noexcept override
    {
        return "test";
    }
};

class FakeCloseCallView final : public interface::CloseCallView
{
};

class MockCallMessageView : public interface::CallMessageView
{
public:
    explicit MockCallMessageView(const std::uint64_t stream_id) : stream_id_(stream_id)
    {
    }

    [[nodiscard]] std::uint64_t stream_id() const noexcept override
    {
        return stream_id_;
    }
    MOCK_METHOD(std::unique_ptr<interface::OpenCallView>, open_call, (), (const, override));
    MOCK_METHOD(std::unique_ptr<interface::PayloadsView>, payloads, (), (const, override));
    [[nodiscard]] std::unique_ptr<interface::DataSchemaView> data_schema() noexcept override
    {
        return nullptr;
    }
    MOCK_METHOD(std::unique_ptr<interface::ResetView>, reset, (), (const, override));
    [[nodiscard]] std::unique_ptr<interface::RecordBatchView> record_batch() noexcept override
    {
        return nullptr;
    }
    MOCK_METHOD(std::unique_ptr<interface::ErrorView>, error, (), (const, override));
    MOCK_METHOD(std::unique_ptr<interface::CloseCallView>, close_call, (), (const, override));

private:
    std::uint64_t stream_id_;
};

inline std::unique_ptr<MockCallMessageView> make_call_message_view(const std::uint64_t stream_id,
                                                                   const bool open  = false,
                                                                   const bool close = false)
{
    auto message = std::make_unique<testing::NiceMock<MockCallMessageView>>(stream_id);
    ON_CALL(*message, open_call()).WillByDefault([open] {
        return open ? std::make_unique<FakeOpenCallView>() : nullptr;
    });
    ON_CALL(*message, close_call()).WillByDefault([close] {
        return close ? std::make_unique<FakeCloseCallView>() : nullptr;
    });
    return message;
}

class FakeCloseConnectionView final : public interface::CloseConnectionView
{
};

class MockControlMessageView : public interface::ControlMessageView
{
public:
    explicit MockControlMessageView(const std::uint64_t stream_id) : stream_id_(stream_id)
    {
    }

    [[nodiscard]] std::uint64_t stream_id() const noexcept override
    {
        return stream_id_;
    }
    MOCK_METHOD(std::unique_ptr<interface::ServerCapabilitiesView>,
                server_capabilities,
                (),
                (const, override));
    MOCK_METHOD(std::unique_ptr<interface::KeepAliveView>, keep_alive, (), (const, override));
    MOCK_METHOD(std::unique_ptr<interface::PayloadsView>, payloads, (), (const, override));
    MOCK_METHOD(std::unique_ptr<interface::ErrorView>, error, (), (const, override));
    MOCK_METHOD(std::unique_ptr<interface::CloseConnectionView>,
                close_connection,
                (),
                (const, override));

private:
    std::uint64_t stream_id_;
};

inline std::unique_ptr<MockControlMessageView> make_control_message_view(
    const std::uint64_t stream_id, const bool close = false)
{
    auto message = std::make_unique<testing::NiceMock<MockControlMessageView>>(stream_id);
    ON_CALL(*message, close_connection()).WillByDefault([close] {
        return close ? std::make_unique<FakeCloseConnectionView>() : nullptr;
    });
    return message;
}

class ForeignCallMessageBuilder : public interface::CallMessageBuilder
{
public:
    MOCK_METHOD(interface::CallMessageBuilderBase&,
                add_payload,
                (std::string_view name, std::string_view value, bool binary),
                (override));
    MOCK_METHOD(interface::CallMessageBuilderBase&,
                data_schema,
                (interface::ArrowSchemaPtr schema, bool has_group_id, bool has_row_id),
                (override));
    MOCK_METHOD(interface::CallMessageBuilderBase&, reset, (std::uint64_t row_id), (override));
    MOCK_METHOD(interface::CallMessageBuilderBase&,
                record_batch,
                (interface::ArrowArrayPtr array, bool is_end_of_group),
                (override));
    MOCK_METHOD(interface::CallMessageBuilderBase&,
                error,
                (std::string_view code, std::string_view text),
                (override));
    MOCK_METHOD(interface::CallMessageBuilderBase&, close_call, (), (override));
};

class ForeignControlMessageBuilder : public interface::ControlMessageBuilder
{
public:
    MOCK_METHOD(interface::ControlMessageBuilder&,
                server_capabilities,
                (std::uint32_t major,
                 std::uint32_t minor,
                 std::endian endianness,
                 std::uint32_t number_of_supported_workers),
                (override));
    MOCK_METHOD(interface::ControlMessageBuilder&, keep_alive, (), (override));
    MOCK_METHOD(interface::ControlMessageBuilder&,
                add_payload,
                (std::string_view name, std::string_view value, bool binary),
                (override));
    MOCK_METHOD(interface::ControlMessageBuilder&,
                error,
                (std::string_view code, std::string_view text),
                (override));
    MOCK_METHOD(interface::ControlMessageBuilder&, close_connection, (), (override));
};

/// Queue and event-sequence tests use this fake to model deterministic inbound work.
class StatefulBackgroundThreadFake final : public BackgroundThreadInterface
{
public:
    [[nodiscard]] BackgroundThreadResult wait_until(
        const std::optional<std::chrono::steady_clock::time_point> deadline) override
    {
        using enum Event;

        if (next_wait_result_)
        {
            auto result = std::move(*next_wait_result_);
            next_wait_result_.reset();
            return result;
        }
        if (!inbound_.empty())
        {
            return {.event = InboundAvailable};
        }
        if (deadline)
        {
            return {.event = DeadlineExpired};
        }
        return {.event = ShutdownRequested};
    }
    bool enqueue_outbound_messages(OutboundBatch& messages) override
    {
        outbound_.insert(outbound_.end(), std::make_move_iterator(messages.begin()),
                         std::make_move_iterator(messages.end()));
        return true;
    }
    bool enqueue_outbound_message(OutboundMessage& message) override
    {
        outbound_.push_back(std::move(message));
        return true;
    }
    [[nodiscard]] InboundBatch dequeue_inbound_messages() override
    {
        InboundBatch result;
        result.swap(inbound_);
        return result;
    }
    void request_shutdown() noexcept override
    {
    }

    void push_inbound(InboundMessage message)
    {
        inbound_.push_back(std::move(message));
    }
    void set_next_wait_result(BackgroundThreadResult result)
    {
        next_wait_result_.emplace(std::move(result));
    }

private:
    std::optional<BackgroundThreadResult> next_wait_result_;
    InboundBatch inbound_;
    OutboundBatch outbound_;
};

} // namespace exasol::udf::v2::internal::test_support
