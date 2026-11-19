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

class FakeBackgroundThread final : public BackgroundThreadInterface
{
public:
    [[nodiscard]] BackgroundThreadResult wait_until(
        const std::optional<std::chrono::steady_clock::time_point> deadline) override
    {
        using enum Event;

        last_deadline_ = deadline;
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
        if (reject_outbound_)
        {
            return false;
        }
        outbound_.insert(outbound_.end(), std::make_move_iterator(messages.begin()),
                         std::make_move_iterator(messages.end()));
        return true;
    }
    bool enqueue_outbound_message(OutboundMessage& message) override
    {
        if (reject_outbound_)
        {
            return false;
        }
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
        shutdown_requested_ = true;
    }

    void push_inbound(InboundMessage message)
    {
        inbound_.push_back(std::move(message));
    }
    void set_reject_outbound(const bool reject) noexcept
    {
        reject_outbound_ = reject;
    }
    void set_next_wait_result(BackgroundThreadResult result)
    {
        next_wait_result_.emplace(std::move(result));
    }
    [[nodiscard]] const std::optional<std::chrono::steady_clock::time_point>& last_deadline()
        const noexcept
    {
        return last_deadline_;
    }
    [[nodiscard]] std::size_t outbound_size() const noexcept
    {
        return outbound_.size();
    }

private:
    bool reject_outbound_{};
    bool shutdown_requested_{};
    std::optional<BackgroundThreadResult> next_wait_result_;
    std::optional<std::chrono::steady_clock::time_point> last_deadline_;
    InboundBatch inbound_;
    OutboundBatch outbound_;
};

} // namespace exasol::udf::v2::internal::test_support
