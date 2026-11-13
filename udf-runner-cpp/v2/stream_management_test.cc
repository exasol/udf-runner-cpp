#include <exasol/udf/v2/internal/call_impl.hpp>
#include <exasol/udf/v2/internal/context_impl.hpp>
#include <exasol/udf/v2/internal/context_state.hpp>
#include <exasol/udf/v2/internal/stream_registry.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <iterator>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <variant>

#include <gtest/gtest.h>

namespace exasol::udf::v2::internal
{
namespace
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

    class FakeCallMessageView final : public interface::CallMessageView
    {
    public:
        explicit FakeCallMessageView(const std::uint64_t stream_id,
                                     const bool open  = false,
                                     const bool close = false)
            : stream_id_(stream_id), open_(open), close_(close)
        {
        }

        [[nodiscard]] std::uint64_t stream_id() const noexcept override
        {
            return stream_id_;
        }
        [[nodiscard]] std::unique_ptr<interface::OpenCallView> open_call() const override
        {
            return open_ ? std::make_unique<FakeOpenCallView>() : nullptr;
        }
        [[nodiscard]] std::unique_ptr<interface::PayloadsView> payloads() const override
        {
            return nullptr;
        }
        [[nodiscard]] std::unique_ptr<interface::DataSchemaView> data_schema() noexcept override
        {
            return nullptr;
        }
        [[nodiscard]] std::unique_ptr<interface::ResetView> reset() const override
        {
            return nullptr;
        }
        [[nodiscard]] std::unique_ptr<interface::RecordBatchView> record_batch() noexcept override
        {
            return nullptr;
        }
        [[nodiscard]] std::unique_ptr<interface::ErrorView> error() const override
        {
            return nullptr;
        }
        [[nodiscard]] std::unique_ptr<interface::CloseCallView> close_call() const override
        {
            return close_ ? std::make_unique<FakeCloseCallView>() : nullptr;
        }

    private:
        std::uint64_t stream_id_;
        bool open_;
        bool close_;
    };

    class FakeCloseConnectionView final : public interface::CloseConnectionView
    {
    };

    class FakeControlMessageView final : public interface::ControlMessageView
    {
    public:
        explicit FakeControlMessageView(const std::uint64_t stream_id, const bool close = false)
            : stream_id_(stream_id), close_(close)
        {
        }
        [[nodiscard]] std::uint64_t stream_id() const noexcept override
        {
            return stream_id_;
        }
        [[nodiscard]] std::unique_ptr<interface::ServerCapabilitiesView> server_capabilities()
            const override
        {
            return nullptr;
        }
        [[nodiscard]] std::unique_ptr<interface::KeepAliveView> keep_alive() const override
        {
            return nullptr;
        }
        [[nodiscard]] std::unique_ptr<interface::PayloadsView> payloads() const override
        {
            return nullptr;
        }
        [[nodiscard]] std::unique_ptr<interface::ErrorView> error() const override
        {
            return nullptr;
        }
        [[nodiscard]] std::unique_ptr<interface::CloseConnectionView> close_connection()
            const override
        {
            return close_ ? std::make_unique<FakeCloseConnectionView>() : nullptr;
        }

    private:
        std::uint64_t stream_id_;
        bool close_;
    };

    class FakeBackgroundThread final : public BackgroundThreadInterface
    {
    public:
        [[nodiscard]] BackgroundThreadResult wait_until(
            const std::optional<std::chrono::steady_clock::time_point> deadline) override
        {
            if (!inbound_.empty())
            {
                return {.event = Event::InboundAvailable};
            }
            if (deadline)
            {
                return {.event = Event::DeadlineExpired};
            }
            return {.event = Event::ShutdownRequested};
        }
        bool enqueue_outbound_messages(OutboundBatch&& messages) override
        {
            if (reject_outbound)
            {
                return false;
            }
            outbound_.insert(outbound_.end(), std::make_move_iterator(messages.begin()),
                             std::make_move_iterator(messages.end()));
            return true;
        }
        bool enqueue_outbound_message(OutboundMessage&& message) override
        {
            if (reject_outbound)
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
            shutdown_requested = true;
        }

        void push_inbound(InboundMessage message)
        {
            inbound_.push_back(std::move(message));
        }
        [[nodiscard]] std::size_t outbound_size() const noexcept
        {
            return outbound_.size();
        }

        bool reject_outbound{};
        bool shutdown_requested{};

    private:
        InboundBatch inbound_;
        OutboundBatch outbound_;
    };

    TEST(StreamRegistryTest, AllocatesStreamIdsByEndpointRole)
    {
        StreamRegistry client{EndpointRole::Client};
        StreamRegistry server{EndpointRole::Server};

        EXPECT_EQ(client.reserve_local_stream_id(), 1);
        EXPECT_EQ(client.reserve_local_stream_id(), 3);
        EXPECT_EQ(server.reserve_local_stream_id(), 2);
        EXPECT_EQ(server.reserve_local_stream_id(), 4);
    }

    TEST(StreamRegistryTest, RejectsWrongParityBeforeHighWaterMarkHandling)
    {
        StreamRegistry registry{EndpointRole::Client};
        ASSERT_EQ(registry.reserve_local_stream_id(), 1);

        EXPECT_EQ(registry.dispatch(std::make_unique<FakeCallMessageView>(1)),
                  DispatchResult::ProtocolError);
    }

    TEST(StreamRegistryTest, DeliversPeerCloseOnceAndThenClosesCall)
    {
        StreamRegistry registry{EndpointRole::Client};
        ASSERT_EQ(registry.dispatch(std::make_unique<FakeCallMessageView>(2, true)),
                  DispatchResult::Routed);
        ASSERT_EQ(registry.dispatch(std::make_unique<FakeCallMessageView>(2, false, true)),
                  DispatchResult::Routed);
        ASSERT_EQ(registry.take_next_unaccepted_call(), 2);

        auto opening = registry.take_call_message(2);
        ASSERT_EQ(opening.result, CallMessageResult::Message);
        EXPECT_NE(opening.message->open_call(), nullptr);

        auto closing = registry.take_call_message(2);
        ASSERT_EQ(closing.result, CallMessageResult::Message);
        EXPECT_NE(closing.message->close_call(), nullptr);
        EXPECT_EQ(registry.take_call_message(2).result, CallMessageResult::PeerClosed);
    }

    TEST(StreamRegistryTest, DiscardsDelayedTrafficOnlyOnPreviouslyUsedPeerStreams)
    {
        StreamRegistry registry{EndpointRole::Client};
        ASSERT_EQ(registry.dispatch(std::make_unique<FakeCallMessageView>(4, true)),
                  DispatchResult::Routed);
        ASSERT_EQ(registry.dispatch(std::make_unique<FakeCallMessageView>(4, false, true)),
                  DispatchResult::Routed);
        static_cast<void>(registry.take_next_unaccepted_call());
        static_cast<void>(registry.take_call_message(4));
        static_cast<void>(registry.take_call_message(4));

        EXPECT_EQ(registry.dispatch(std::make_unique<FakeCallMessageView>(2)),
                  DispatchResult::Discarded);
        EXPECT_EQ(registry.dispatch(std::make_unique<FakeCallMessageView>(2, true)),
                  DispatchResult::ProtocolError);
        EXPECT_EQ(registry.dispatch(std::make_unique<FakeCallMessageView>(6)),
                  DispatchResult::ProtocolError);
    }

    TEST(StreamRegistryTest, EnforcesLocalCloseState)
    {
        StreamRegistry registry{EndpointRole::Client};
        ASSERT_EQ(registry.reserve_local_stream_id(), 1);
        ASSERT_TRUE(registry.register_local_call(1, false));

        EXPECT_EQ(registry.call_send_status(1), interface::OperationStatus::Ok);
        EXPECT_TRUE(registry.mark_local_close_accepted(1));
        EXPECT_EQ(registry.call_send_status(1), interface::OperationStatus::ProtocolError);
        EXPECT_FALSE(registry.mark_local_close_accepted(1));
    }

    TEST(StreamRegistryTest, EnforcesMessageTypeAndStreamIdRules)
    {
        StreamRegistry registry{EndpointRole::Client};

        EXPECT_EQ(registry.dispatch(std::make_unique<FakeCallMessageView>(0)),
                  DispatchResult::ProtocolError);
        EXPECT_EQ(registry.dispatch(std::make_unique<FakeControlMessageView>(1)),
                  DispatchResult::ProtocolError);
        EXPECT_EQ(registry.dispatch(std::make_unique<FakeControlMessageView>(0)),
                  DispatchResult::Routed);
        EXPECT_NE(registry.take_control_message(), nullptr);
    }

    TEST(ContextImplTest, DispatchesInboundCallAndKeepsCallStateAfterAccept)
    {
        auto background = std::make_unique<FakeBackgroundThread>();
        background->push_inbound(std::make_unique<FakeCallMessageView>(2, true));
        background->push_inbound(std::make_unique<FakeCallMessageView>(2, false, true));
        ContextImpl context{EndpointRole::Client, std::move(background)};

        auto result = context.accept_call();
        ASSERT_TRUE(result);
        auto& call   = **result.value;
        auto opening = call.receive();
        ASSERT_TRUE(opening);
        EXPECT_NE((*opening.value)->open_call(), nullptr);
        auto close = call.receive();
        ASSERT_TRUE(close);
        EXPECT_NE((*close.value)->close_call(), nullptr);
        EXPECT_EQ(call.receive().status, interface::OperationStatus::PeerClosed);
    }

    TEST(CallImplTest, ReportsContextClosedAfterSharedStateExpires)
    {
        std::unique_ptr<CallImpl> call;
        {
            auto state = std::make_shared<ContextState>(EndpointRole::Client,
                                                        std::make_unique<FakeBackgroundThread>());
            call       = std::make_unique<CallImpl>(state, 1);
        }

        EXPECT_EQ(call->receive_status(), interface::MessageStatus::ContextClosed);
        EXPECT_EQ(call->receive().status, interface::OperationStatus::ContextClosed);
    }

    TEST(ContextImplTest, RetainsBuilderOwnershipWhenBackgroundRejectsCallMessage)
    {
        auto background      = std::make_unique<FakeBackgroundThread>();
        auto* background_ptr = background.get();
        ContextImpl context{EndpointRole::Client, std::move(background)};
        auto opening = context.create_opening_call_message_builder();
        ASSERT_NE(opening, nullptr);
        opening->open_call("test");
        auto opened = context.open_call(std::move(opening));
        ASSERT_TRUE(opened);

        auto builder = (*opened.value)->create_message_builder();
        ASSERT_NE(builder, nullptr);
        background_ptr->reject_outbound = true;
        auto result                     = (*opened.value)->send(std::move(builder));

        EXPECT_EQ(result.status, interface::OperationStatus::TransportError);
        EXPECT_NE(builder, nullptr);
    }

    TEST(ContextImplTest, HandlesPeerConnectionCloseAsContextClosed)
    {
        auto background = std::make_unique<FakeBackgroundThread>();
        background->push_inbound(std::make_unique<FakeControlMessageView>(0, true));
        auto* background_ptr = background.get();
        ContextImpl context{EndpointRole::Client, std::move(background)};

        EXPECT_EQ(context.receive_status(), interface::MessageStatus::ContextClosed);
        EXPECT_EQ(background_ptr->outbound_size(), 1);
        EXPECT_EQ(context.control_stream().receive().status,
                  interface::OperationStatus::ContextClosed);
    }

    TEST(ContextImplTest, RejectsBuildersCreatedByAnotherContextWithTheSameStreamId)
    {
        ContextImpl first{EndpointRole::Client, std::make_unique<FakeBackgroundThread>()};
        ContextImpl second{EndpointRole::Client, std::make_unique<FakeBackgroundThread>()};
        auto first_opening = first.create_opening_call_message_builder();
        first_opening->open_call("first");
        auto first_call     = first.open_call(std::move(first_opening));
        auto second_opening = second.create_opening_call_message_builder();
        second_opening->open_call("second");
        auto second_call = second.open_call(std::move(second_opening));
        ASSERT_TRUE(first_call);
        ASSERT_TRUE(second_call);

        auto foreign_builder = (*first_call.value)->create_message_builder();
        auto result          = (*second_call.value)->send(std::move(foreign_builder));

        EXPECT_EQ(result.status, interface::OperationStatus::ProtocolError);
        EXPECT_NE(foreign_builder, nullptr);
    }

} // namespace
} // namespace exasol::udf::v2::internal
