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

    class ForeignCallMessageBuilder final : public interface::CallMessageBuilder
    {
    public:
        using interface::CallMessageBuilderBase::add_payload;

        interface::CallMessageBuilderBase& add_payload(const std::string_view name,
                                                       const std::string_view value,
                                                       const bool binary) override
        {
            static_cast<void>(name);
            static_cast<void>(value);
            static_cast<void>(binary);
            return *this;
        }
        interface::CallMessageBuilderBase& data_schema(interface::ArrowSchemaPtr schema,
                                                       const bool has_group_id,
                                                       const bool has_row_id) override
        {
            static_cast<void>(schema);
            static_cast<void>(has_group_id);
            static_cast<void>(has_row_id);
            return *this;
        }
        interface::CallMessageBuilderBase& reset(const std::uint64_t row_id) override
        {
            static_cast<void>(row_id);
            return *this;
        }
        interface::CallMessageBuilderBase& record_batch(interface::ArrowArrayPtr array,
                                                        const bool is_end_of_group) override
        {
            static_cast<void>(array);
            static_cast<void>(is_end_of_group);
            return *this;
        }
        interface::CallMessageBuilderBase& error(const std::string_view code,
                                                 const std::string_view text) override
        {
            static_cast<void>(code);
            static_cast<void>(text);
            return *this;
        }
        interface::CallMessageBuilderBase& close_call() override
        {
            return *this;
        }
    };

    class ForeignControlMessageBuilder final : public interface::ControlMessageBuilder
    {
    public:
        using interface::ControlMessageBuilder::add_payload;

        interface::ControlMessageBuilder& server_capabilities(
            const std::uint32_t major,
            const std::uint32_t minor,
            const std::endian endianness,
            const std::uint32_t number_of_supported_workers) override
        {
            static_cast<void>(major);
            static_cast<void>(minor);
            static_cast<void>(endianness);
            static_cast<void>(number_of_supported_workers);
            return *this;
        }
        interface::ControlMessageBuilder& keep_alive() override
        {
            return *this;
        }
        interface::ControlMessageBuilder& add_payload(const std::string_view name,
                                                      const std::string_view value,
                                                      const bool binary) override
        {
            static_cast<void>(name);
            static_cast<void>(value);
            static_cast<void>(binary);
            return *this;
        }
        interface::ControlMessageBuilder& error(const std::string_view code,
                                                const std::string_view text) override
        {
            static_cast<void>(code);
            static_cast<void>(text);
            return *this;
        }
        interface::ControlMessageBuilder& close_connection() override
        {
            return *this;
        }
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

    TEST(StreamRegistryTest, AllocatesStreamIdsByEndpointRole)
    {
        StreamRegistry client{EndpointRole::Client};
        StreamRegistry server{EndpointRole::Server};

        EXPECT_EQ(client.reserve_local_stream_id(), 1);
        EXPECT_EQ(client.reserve_local_stream_id(), 3);
        EXPECT_EQ(server.reserve_local_stream_id(), 2);
        EXPECT_EQ(server.reserve_local_stream_id(), 4);
    }

    TEST(ContextStateTest, UsesInjectedClockToCreateDeadlineWithoutSleeping)
    {
        using Clock                = std::chrono::steady_clock;
        const auto now             = Clock::time_point{std::chrono::seconds{123}};
        const auto timeout         = std::chrono::seconds{5};
        auto background            = std::make_unique<FakeBackgroundThread>();
        const auto* background_ptr = background.get();
        ContextState context_state{EndpointRole::Client, std::move(background),
                                   [now] { return now; }};

        EXPECT_EQ(context_state.wait_for_message(interface::Timeout{timeout}),
                  interface::MessageStatus::TimedOut);
        ASSERT_TRUE(background_ptr->last_deadline().has_value());
        EXPECT_EQ(*background_ptr->last_deadline(), now + timeout);
    }

    TEST(StreamRegistryTest, RejectsNewLocalOwnedStreamAndDiscardsDelayedTraffic)
    {
        StreamRegistry registry{EndpointRole::Client};

        EXPECT_EQ(registry.dispatch(std::make_unique<FakeCallMessageView>(1)),
                  DispatchResult::ProtocolError);
        ASSERT_EQ(registry.reserve_local_stream_id(), 1);

        EXPECT_EQ(registry.dispatch(std::make_unique<FakeCallMessageView>(1)),
                  DispatchResult::Discarded);
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

    TEST(StreamRegistryTest, DiscardsDelayedTrafficOnPreviouslyUsedPeerStreams)
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

    TEST(StreamRegistryTest, DiscardsDelayedTrafficOnPreviouslyUsedLocalStreams)
    {
        StreamRegistry registry{EndpointRole::Client};
        ASSERT_EQ(registry.reserve_local_stream_id(), 1);
        ASSERT_TRUE(registry.register_local_call(1, false));
        ASSERT_TRUE(registry.mark_local_close_accepted(1));

        EXPECT_EQ(registry.dispatch(std::make_unique<FakeCallMessageView>(1)),
                  DispatchResult::Routed);
        EXPECT_EQ(registry.dispatch(std::make_unique<FakeCallMessageView>(1, false, true)),
                  DispatchResult::Routed);
        EXPECT_EQ(registry.take_call_message(1).result, CallMessageResult::Message);
        auto peer_close = registry.take_call_message(1);
        ASSERT_EQ(peer_close.result, CallMessageResult::Message);
        EXPECT_NE(peer_close.message->close_call(), nullptr);

        EXPECT_EQ(registry.dispatch(std::make_unique<FakeCallMessageView>(1)),
                  DispatchResult::Discarded);
        EXPECT_EQ(registry.dispatch(std::make_unique<FakeCallMessageView>(1, true)),
                  DispatchResult::ProtocolError);
        EXPECT_EQ(registry.dispatch(std::make_unique<FakeCallMessageView>(3)),
                  DispatchResult::ProtocolError);
    }

    TEST(StreamRegistryTest, TracksPendingCallMessagesAcrossReadsAndCleanup)
    {
        StreamRegistry registry{EndpointRole::Client};
        EXPECT_FALSE(registry.has_any_pending_message());

        ASSERT_EQ(registry.dispatch(std::make_unique<FakeCallMessageView>(2, true)),
                  DispatchResult::Routed);
        ASSERT_EQ(registry.dispatch(std::make_unique<FakeCallMessageView>(2)),
                  DispatchResult::Routed);
        ASSERT_EQ(registry.dispatch(std::make_unique<FakeCallMessageView>(4, true)),
                  DispatchResult::Routed);
        EXPECT_TRUE(registry.has_any_pending_message());

        EXPECT_EQ(registry.take_call_message(2).result, CallMessageResult::Message);
        EXPECT_TRUE(registry.has_any_pending_message());
        EXPECT_EQ(registry.take_call_message(2).result, CallMessageResult::Message);
        EXPECT_TRUE(registry.has_any_pending_message());
        EXPECT_EQ(registry.take_call_message(4).result, CallMessageResult::Message);
        EXPECT_FALSE(registry.has_any_pending_message());

        ASSERT_EQ(registry.dispatch(std::make_unique<FakeCallMessageView>(6, true)),
                  DispatchResult::Routed);
        ASSERT_EQ(registry.dispatch(std::make_unique<FakeControlMessageView>(0)),
                  DispatchResult::Routed);
        registry.discard_call_streams();
        EXPECT_TRUE(registry.has_any_pending_message());
        EXPECT_NE(registry.take_control_message(), nullptr);
        EXPECT_FALSE(registry.has_any_pending_message());
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
        ContextImpl context{
            std::make_shared<ContextState>(EndpointRole::Client, std::move(background))};

        auto result = context.accept_call();
        ASSERT_TRUE(result);
        auto& call = **result.value;
        EXPECT_EQ(call.receive_status(), interface::MessageStatus::MessageAvailable);
        auto opening = call.receive();
        ASSERT_TRUE(opening);
        EXPECT_NE((*opening.value)->open_call(), nullptr);
        auto close = call.receive();
        ASSERT_TRUE(close);
        EXPECT_NE((*close.value)->close_call(), nullptr);
        EXPECT_EQ(call.receive().status, interface::OperationStatus::PeerClosed);
    }

    TEST(ContextImplTest, SendOverloadsAcceptMessagesAndZeroTimeoutReceiveReturnsTimedOut)
    {
        auto background            = std::make_unique<FakeBackgroundThread>();
        const auto* background_ptr = background.get();
        ContextImpl context{
            std::make_shared<ContextState>(EndpointRole::Client, std::move(background))};
        auto opening = context.create_opening_call_message_builder();
        ASSERT_NE(opening, nullptr);
        opening->add_payload("opening", "value")
            .data_schema(nullptr, false, false)
            .reset(1)
            .record_batch(nullptr, false)
            .error("OPEN", "opening error")
            .data_schema(interface::ArrowSchemaPtr{new ArrowSchema{}}, true, true)
            .reset(2)
            .record_batch(interface::ArrowArrayPtr{new ArrowArray{}}, true)
            .error("OPEN_REPLACED", "replacement opening error");
        opening->open_call("test").add_payload("", "", true);
        auto opened = context.open_call(std::move(opening));
        ASSERT_TRUE(opened);
        const auto outbound_before_sends = background_ptr->outbound_size();

        auto no_wait_builder = (*opened.value)->create_message_builder();
        ASSERT_NE(no_wait_builder, nullptr);
        EXPECT_TRUE((*opened.value)->send(std::move(no_wait_builder)));
        EXPECT_EQ(background_ptr->outbound_size(), outbound_before_sends + 1);

        auto zero_timeout_builder = (*opened.value)->create_message_builder();
        ASSERT_NE(zero_timeout_builder, nullptr);
        EXPECT_TRUE((*opened.value)
                        ->send(std::move(zero_timeout_builder),
                               interface::Timeout{std::chrono::steady_clock::duration::zero()}));
        EXPECT_EQ(background_ptr->outbound_size(), outbound_before_sends + 2);

        EXPECT_EQ((*opened.value)->receive().status, interface::OperationStatus::TimedOut);
        EXPECT_EQ((*opened.value)->receive_status(), interface::MessageStatus::NoMessage);
        auto populated_builder = (*opened.value)->create_message_builder();
        ASSERT_NE(populated_builder, nullptr);
        populated_builder->add_payload("call", "value")
            .data_schema(nullptr, false, false)
            .reset(2)
            .record_batch(nullptr, false)
            .error("CALL", "call error")
            .data_schema(interface::ArrowSchemaPtr{new ArrowSchema{}}, true, true)
            .reset(3)
            .record_batch(interface::ArrowArrayPtr{new ArrowArray{}}, true)
            .add_payload("", "", true)
            .error("CALL_REPLACED", "replacement call error");
        EXPECT_TRUE((*opened.value)->send(std::move(populated_builder)));
        EXPECT_EQ((*opened.value)
                      ->receive(interface::Timeout{std::chrono::steady_clock::duration::zero()})
                      .status,
                  interface::OperationStatus::TimedOut);
        EXPECT_EQ(context.control_stream().receive().status, interface::OperationStatus::TimedOut);

        auto control_builder = context.control_stream().create_message_builder();
        ASSERT_NE(control_builder, nullptr);
        control_builder->server_capabilities(1, 0, std::endian::native, 1)
            .keep_alive()
            .add_payload("control", "value")
            .error("CONTROL", "control error")
            .server_capabilities(2, 1, std::endian::native, 2)
            .add_payload("", "", true)
            .error("CONTROL_REPLACED", "replacement control error");
        EXPECT_TRUE(context.control_stream().send(std::move(control_builder)));
    }

    TEST(ContextImplTest, ForwardsFailedOperationsAndWaitOverloads)
    {
        auto background = std::make_unique<FakeBackgroundThread>();
        background->set_next_wait_result({.event = Event::DeadlineExpired});
        ContextImpl context{
            std::make_shared<ContextState>(EndpointRole::Client, std::move(background))};

        EXPECT_FALSE(context.is_new_inbound_call_available());
        EXPECT_EQ(context.receive_status(), interface::MessageStatus::NoMessage);
        const auto rejected_open = context.open_call(nullptr);
        EXPECT_EQ(rejected_open.status, interface::OperationStatus::ProtocolError);
        EXPECT_EQ(context.wait_for_message(interface::Timeout{std::chrono::seconds{1}}),
                  interface::MessageStatus::TimedOut);
        EXPECT_EQ(context.wait_for_message(), interface::MessageStatus::Cancelled);

        EXPECT_EQ(context.accept_call().status, interface::OperationStatus::Cancelled);
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
        EXPECT_EQ(call->receive(interface::Timeout{std::chrono::seconds{1}}).status,
                  interface::OperationStatus::ContextClosed);
        EXPECT_EQ(call->create_message_builder(), nullptr);
        EXPECT_EQ(call->send(nullptr).status, interface::OperationStatus::ContextClosed);
    }

    TEST(ControlStreamImplTest, ReportsContextClosedAfterStateExpires)
    {
        ControlStreamImpl stream{std::weak_ptr<ContextStateInterface>{}};

        EXPECT_EQ(stream.receive_status(), interface::MessageStatus::ContextClosed);
        EXPECT_EQ(stream.create_message_builder(), nullptr);
        EXPECT_EQ(stream.receive().status, interface::OperationStatus::ContextClosed);
        EXPECT_EQ(stream.receive(interface::Timeout{std::chrono::seconds{1}}).status,
                  interface::OperationStatus::ContextClosed);
        EXPECT_EQ(stream.send(nullptr).status, interface::OperationStatus::ContextClosed);
    }

    TEST(ContextImplTest, RetainsBuilderOwnershipWhenBackgroundRejectsCallMessage)
    {
        auto background = std::make_unique<FakeBackgroundThread>();
        background->set_reject_outbound(true);
        ContextImpl context{
            std::make_shared<ContextState>(EndpointRole::Client, std::move(background))};
        auto opening = context.create_opening_call_message_builder();
        ASSERT_NE(opening, nullptr);
        opening->open_call("test");
        auto opened = context.open_call(std::move(opening));
        ASSERT_TRUE(opened);

        auto builder = (*opened.value)->create_message_builder();
        ASSERT_NE(builder, nullptr);
        auto result = (*opened.value)->send(std::move(builder));

        EXPECT_EQ(result.status, interface::OperationStatus::TransportError);
        EXPECT_NE(builder, nullptr);
    }

    TEST(ContextImplTest, RetainsOpeningBuilderWhenBackgroundRejectsIt)
    {
        auto background = std::make_unique<FakeBackgroundThread>();
        background->set_reject_outbound(true);
        ContextImpl context{
            std::make_shared<ContextState>(EndpointRole::Client, std::move(background))};
        auto opening = context.create_opening_call_message_builder();
        ASSERT_NE(opening, nullptr);
        opening->open_call("test");
        const auto result = context.open_call(std::move(opening));

        EXPECT_EQ(result.status, interface::OperationStatus::TransportError);
        EXPECT_NE(opening, nullptr);
    }

    TEST(ContextImplTest, DeliversPeerConnectionCloseOnceBeforeBecomingTerminal)
    {
        auto background = std::make_unique<FakeBackgroundThread>();
        background->push_inbound(std::make_unique<FakeControlMessageView>(0, true));
        const auto* background_ptr = background.get();
        ContextImpl context{
            std::make_shared<ContextState>(EndpointRole::Client, std::move(background))};

        EXPECT_EQ(context.receive_status(), interface::MessageStatus::MessageAvailable);
        EXPECT_EQ(background_ptr->outbound_size(), 1);
        auto& control_stream = context.control_stream();
        EXPECT_EQ(control_stream.receive_status(), interface::MessageStatus::MessageAvailable);
        auto close_message = control_stream.receive();
        ASSERT_TRUE(close_message);
        EXPECT_NE((*close_message.value)->close_connection(), nullptr);
        EXPECT_EQ(control_stream.receive().status, interface::OperationStatus::ContextClosed);
    }

    TEST(ContextImplTest, RejectsBuildersCreatedByAnotherContextWithTheSameStreamId)
    {
        ContextImpl first{std::make_shared<ContextState>(EndpointRole::Client,
                                                         std::make_unique<FakeBackgroundThread>())};
        ContextImpl second{std::make_shared<ContextState>(
            EndpointRole::Client, std::make_unique<FakeBackgroundThread>())};
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

    TEST(ContextStateTest, CreatesAndSendsControlMessagesUntilConnectionClose)
    {
        auto background            = std::make_unique<FakeBackgroundThread>();
        const auto* background_ptr = background.get();
        ContextState state{EndpointRole::Client, std::move(background)};

        auto message = state.create_control_message_builder();
        ASSERT_NE(message, nullptr);
        message->keep_alive();
        EXPECT_TRUE(state.send_control_message(std::move(message)));
        EXPECT_EQ(background_ptr->outbound_size(), 1);

        auto close = state.create_control_message_builder();
        ASSERT_NE(close, nullptr);
        close->close_connection();
        EXPECT_TRUE(state.send_control_message(std::move(close)));
        EXPECT_EQ(state.create_control_message_builder(), nullptr);
        EXPECT_EQ(state.send_control_message(std::move(message)).status,
                  interface::OperationStatus::ContextClosed);
        EXPECT_EQ(state.control_receive_status(), interface::MessageStatus::ContextClosed);
    }

    TEST(ContextStateTest, RejectsInvalidControlBuildersAndPreservesRejectedOwnership)
    {
        auto background      = std::make_unique<FakeBackgroundThread>();
        auto* background_ptr = background.get();
        ContextState state{EndpointRole::Client, std::move(background)};
        ContextState other{EndpointRole::Client, std::make_unique<FakeBackgroundThread>()};

        std::unique_ptr<interface::ControlMessageBuilder> null_message;
        EXPECT_EQ(state.send_control_message(std::move(null_message)).status,
                  interface::OperationStatus::ProtocolError);

        auto foreign = other.create_control_message_builder();
        EXPECT_EQ(state.send_control_message(std::move(foreign)).status,
                  interface::OperationStatus::ProtocolError);
        EXPECT_NE(foreign, nullptr);

        std::unique_ptr<interface::ControlMessageBuilder> wrong_implementation =
            std::make_unique<ForeignControlMessageBuilder>();
        EXPECT_EQ(state.send_control_message(std::move(wrong_implementation)).status,
                  interface::OperationStatus::ProtocolError);
        EXPECT_NE(wrong_implementation, nullptr);

        auto rejected = state.create_control_message_builder();
        ASSERT_NE(rejected, nullptr);
        background_ptr->set_reject_outbound(true);
        EXPECT_EQ(state.send_control_message(std::move(rejected)).status,
                  interface::OperationStatus::TransportError);
        EXPECT_NE(rejected, nullptr);
        EXPECT_EQ(state.receive_status(), interface::MessageStatus::ConnectionError);

        std::unique_ptr<interface::ControlMessageBuilder> expired_owner_builder;
        {
            ContextState source{EndpointRole::Client, std::make_unique<FakeBackgroundThread>()};
            expired_owner_builder = source.create_control_message_builder();
        }
        ContextState fresh_state{EndpointRole::Client, std::make_unique<FakeBackgroundThread>()};
        EXPECT_EQ(fresh_state.send_control_message(std::move(expired_owner_builder)).status,
                  interface::OperationStatus::ProtocolError);
        EXPECT_NE(expired_owner_builder, nullptr);
    }

    TEST(ContextStateTest, RejectsInvalidOpeningMessagesAndRetainsTheirOwnership)
    {
        auto background = std::make_unique<FakeBackgroundThread>();
        ContextState state{EndpointRole::Client, std::move(background)};

        std::unique_ptr<interface::OpeningCallMessageBuilder> null_message;
        EXPECT_EQ(state.open_call(std::move(null_message)).status,
                  interface::OperationStatus::ProtocolError);

        auto missing_open = state.create_opening_call_message_builder();
        ASSERT_NE(missing_open, nullptr);
        EXPECT_EQ(state.open_call(std::move(missing_open)).status,
                  interface::OperationStatus::ProtocolError);
        EXPECT_NE(missing_open, nullptr);

        ContextState other{EndpointRole::Client, std::make_unique<FakeBackgroundThread>()};
        auto foreign = other.create_opening_call_message_builder();
        ASSERT_NE(foreign, nullptr);
        foreign->open_call("foreign");
        EXPECT_EQ(state.open_call(std::move(foreign)).status,
                  interface::OperationStatus::ProtocolError);
        EXPECT_NE(foreign, nullptr);
    }

    TEST(ContextStateTest, HandlesCallSendValidationAndCloseState)
    {
        auto background = std::make_unique<FakeBackgroundThread>();
        ContextState state{EndpointRole::Client, std::move(background)};
        auto opening = state.create_opening_call_message_builder();
        ASSERT_NE(opening, nullptr);
        opening->open_call("call");
        const auto opened = state.open_call(std::move(opening));
        ASSERT_TRUE(opened);
        const auto stream_id = *opened.value;

        std::unique_ptr<interface::CallMessageBuilder> null_message;
        EXPECT_EQ(state.send_call_message(stream_id, std::move(null_message), std::nullopt).status,
                  interface::OperationStatus::ProtocolError);
        EXPECT_EQ(state
                      .send_call_message(stream_id + 2,
                                         state.create_call_message_builder(stream_id), std::nullopt)
                      .status,
                  interface::OperationStatus::PeerClosed);

        ContextState other{EndpointRole::Client, std::make_unique<FakeBackgroundThread>()};
        auto other_opening = other.create_opening_call_message_builder();
        ASSERT_NE(other_opening, nullptr);
        other_opening->open_call("other");
        ASSERT_TRUE(other.open_call(std::move(other_opening)));
        auto foreign = other.create_call_message_builder(stream_id);
        ASSERT_NE(foreign, nullptr);
        EXPECT_EQ(state.send_call_message(stream_id, std::move(foreign), std::nullopt).status,
                  interface::OperationStatus::ProtocolError);
        EXPECT_NE(foreign, nullptr);

        std::unique_ptr<interface::CallMessageBuilder> wrong_implementation =
            std::make_unique<ForeignCallMessageBuilder>();
        EXPECT_EQ(state.send_call_message(stream_id, std::move(wrong_implementation), std::nullopt)
                      .status,
                  interface::OperationStatus::ProtocolError);
        EXPECT_NE(wrong_implementation, nullptr);

        auto close = state.create_call_message_builder(stream_id);
        ASSERT_NE(close, nullptr);
        close->close_call();
        EXPECT_TRUE(state.send_call_message(stream_id, std::move(close), std::nullopt));
        EXPECT_EQ(state.call_receive_status(stream_id), interface::MessageStatus::NoMessage);
        EXPECT_EQ(state
                      .send_call_message(stream_id, state.create_call_message_builder(stream_id),
                                         std::nullopt)
                      .status,
                  interface::OperationStatus::ProtocolError);

        std::unique_ptr<interface::CallMessageBuilder> expired_owner_builder;
        {
            ContextState source{EndpointRole::Client, std::make_unique<FakeBackgroundThread>()};
            auto source_opening = source.create_opening_call_message_builder();
            source_opening->open_call("source");
            ASSERT_TRUE(source.open_call(std::move(source_opening)));
            expired_owner_builder = source.create_call_message_builder(1);
        }
        auto destination_opening = state.create_opening_call_message_builder();
        destination_opening->open_call("destination");
        const auto destination = state.open_call(std::move(destination_opening));
        ASSERT_TRUE(destination);
        EXPECT_EQ(state
                      .send_call_message(*destination.value, std::move(expired_owner_builder),
                                         std::nullopt)
                      .status,
                  interface::OperationStatus::ProtocolError);
        EXPECT_NE(expired_owner_builder, nullptr);
    }

    TEST(ContextStateTest, ClosedStateRejectsNewBuildersAndOperations)
    {
        ContextState state{EndpointRole::Client, std::make_unique<FakeBackgroundThread>()};
        state.shutdown();

        EXPECT_EQ(state.create_opening_call_message_builder(), nullptr);
        EXPECT_EQ(state.create_call_message_builder(1), nullptr);
        EXPECT_EQ(state.create_control_message_builder(), nullptr);
        EXPECT_EQ(state.accept_call().status, interface::OperationStatus::ContextClosed);
        EXPECT_EQ(state.open_call(nullptr).status, interface::OperationStatus::ContextClosed);
        EXPECT_EQ(state.wait_for_message(std::nullopt), interface::MessageStatus::ContextClosed);
        EXPECT_EQ(state.send_call_message(1, nullptr, std::nullopt).status,
                  interface::OperationStatus::ContextClosed);
        EXPECT_EQ(state.send_control_message(nullptr).status,
                  interface::OperationStatus::ContextClosed);
    }

    TEST(ContextStateTest, MapsBackgroundShutdownAndProtocolFailuresToTerminalStatuses)
    {
        auto background = std::make_unique<FakeBackgroundThread>();
        background->set_next_wait_result({.event = Event::ShutdownRequested});
        ContextState cancelled{EndpointRole::Client, std::move(background)};
        EXPECT_EQ(cancelled.wait_for_message(std::nullopt), interface::MessageStatus::Cancelled);
        EXPECT_EQ(cancelled.accept_call().status, interface::OperationStatus::Cancelled);

        auto failed_background = std::make_unique<FakeBackgroundThread>();
        failed_background->set_next_wait_result(
            {.event = Event::ShutdownRequested,
             .error = interface::ErrorInfo("IO", "background failed")});
        ContextState failed{EndpointRole::Client, std::move(failed_background)};
        EXPECT_EQ(failed.wait_for_message(std::nullopt), interface::MessageStatus::ConnectionError);
        EXPECT_EQ(failed.receive_control_message(std::nullopt).status,
                  interface::OperationStatus::TransportError);

        auto invalid_background = std::make_unique<FakeBackgroundThread>();
        invalid_background->push_inbound(std::make_unique<FakeCallMessageView>(0));
        ContextState invalid{EndpointRole::Client, std::move(invalid_background)};
        EXPECT_EQ(invalid.receive_status(), interface::MessageStatus::ConnectionError);
        EXPECT_EQ(invalid.is_new_inbound_call_available(), false);
        EXPECT_EQ(invalid.receive_call_message(1, std::nullopt).status,
                  interface::OperationStatus::ProtocolError);
    }

    TEST(ContextStateTest, RejectsMalformedInboundTrafficFromEveryBlockingOperation)
    {
        auto wait_background = std::make_unique<FakeBackgroundThread>();
        wait_background->push_inbound(std::make_unique<FakeControlMessageView>(1));
        ContextState wait_state{EndpointRole::Client, std::move(wait_background)};
        EXPECT_EQ(wait_state.wait_for_message(std::nullopt),
                  interface::MessageStatus::ConnectionError);

        auto accept_background = std::make_unique<FakeBackgroundThread>();
        accept_background->push_inbound(std::make_unique<FakeControlMessageView>(1));
        ContextState accept_state{EndpointRole::Client, std::move(accept_background)};
        EXPECT_EQ(accept_state.accept_call().status, interface::OperationStatus::ProtocolError);

        auto control_background = std::make_unique<FakeBackgroundThread>();
        control_background->push_inbound(std::make_unique<FakeControlMessageView>(1));
        ContextState control_state{EndpointRole::Client, std::move(control_background)};
        EXPECT_EQ(control_state.receive_control_message(std::nullopt).status,
                  interface::OperationStatus::ProtocolError);

        auto send_background      = std::make_unique<FakeBackgroundThread>();
        auto* send_background_ptr = send_background.get();
        ContextState send_state{EndpointRole::Client, std::move(send_background)};
        auto opening = send_state.create_opening_call_message_builder();
        opening->open_call("call");
        const auto call = send_state.open_call(std::move(opening));
        ASSERT_TRUE(call);
        send_background_ptr->push_inbound(std::make_unique<FakeCallMessageView>(0));
        auto message = send_state.create_call_message_builder(*call.value);
        EXPECT_EQ(
            send_state.send_call_message(*call.value, std::move(message), std::nullopt).status,
            interface::OperationStatus::ProtocolError);
        EXPECT_NE(message, nullptr);
    }

    TEST(ContextStateTest, ReceivesAcceptedInboundCallAndConnectionClose)
    {
        auto background = std::make_unique<FakeBackgroundThread>();
        background->push_inbound(std::make_unique<FakeCallMessageView>(2, true));
        ContextState state{EndpointRole::Client, std::move(background)};
        EXPECT_TRUE(state.is_new_inbound_call_available());
        const auto accepted = state.accept_call();
        ASSERT_TRUE(accepted);
        EXPECT_EQ(*accepted.value, 2);
        EXPECT_EQ(state.call_receive_status(2), interface::MessageStatus::MessageAvailable);
        EXPECT_TRUE(state.receive_call_message(2, std::nullopt));
        EXPECT_EQ(state.receive_call_message(2, std::nullopt).status,
                  interface::OperationStatus::Cancelled);

        auto close_background = std::make_unique<FakeBackgroundThread>();
        close_background->push_inbound(std::make_unique<FakeControlMessageView>(0, true));
        ContextState peer_closed{EndpointRole::Client, std::move(close_background)};
        EXPECT_EQ(peer_closed.control_receive_status(), interface::MessageStatus::MessageAvailable);
        auto close = peer_closed.receive_control_message(std::nullopt);
        ASSERT_TRUE(close);
        EXPECT_NE((*close.value)->close_connection(), nullptr);
        EXPECT_EQ(peer_closed.receive_control_message(std::nullopt).status,
                  interface::OperationStatus::ContextClosed);
    }

    TEST(ContextStateTest, HandlesTimeoutsUnknownCallsAndMessagesForActiveStreams)
    {
        auto background = std::make_unique<FakeBackgroundThread>();
        background->set_next_wait_result({.event = Event::InboundAvailable});
        auto* background_ptr = background.get();
        ContextState state{EndpointRole::Client, std::move(background)};

        EXPECT_EQ(state.wait_for_message(interface::Timeout{std::chrono::seconds{1}}),
                  interface::MessageStatus::TimedOut);
        background_ptr->push_inbound(std::make_unique<FakeControlMessageView>(0));
        EXPECT_TRUE(state.receive_control_message(std::nullopt));
        EXPECT_EQ(state.receive_control_message(interface::Timeout{std::chrono::seconds{0}}).status,
                  interface::OperationStatus::TimedOut);
        EXPECT_EQ(state.call_receive_status(999), interface::MessageStatus::PeerClosed);
        EXPECT_EQ(state.receive_call_message(999, std::nullopt).status,
                  interface::OperationStatus::PeerClosed);

        auto first_opening = state.create_opening_call_message_builder();
        ASSERT_NE(first_opening, nullptr);
        first_opening->open_call("first");
        const auto first_call = state.open_call(std::move(first_opening));
        ASSERT_TRUE(first_call);
        const auto second_call = [&state] {
            auto opening = state.create_opening_call_message_builder();
            opening->open_call("second");
            return state.open_call(std::move(opening));
        }();
        ASSERT_TRUE(second_call);

        auto wrong_stream_builder = state.create_call_message_builder(*first_call.value);
        EXPECT_EQ(state
                      .send_call_message(*second_call.value, std::move(wrong_stream_builder),
                                         std::nullopt)
                      .status,
                  interface::OperationStatus::ProtocolError);
        EXPECT_EQ(state
                      .receive_call_message(*first_call.value,
                                            interface::Timeout{std::chrono::seconds{0}})
                      .status,
                  interface::OperationStatus::TimedOut);

        auto opening_close = state.create_opening_call_message_builder();
        ASSERT_NE(opening_close, nullptr);
        opening_close->open_call("closed at open").close_call();
        const auto locally_closed = state.open_call(std::move(opening_close));
        ASSERT_TRUE(locally_closed);
        EXPECT_EQ(state
                      .send_call_message(*locally_closed.value,
                                         state.create_call_message_builder(*locally_closed.value),
                                         std::nullopt)
                      .status,
                  interface::OperationStatus::ProtocolError);
    }

    TEST(ContextStateTest, PeerConnectionCloseStopsCallReceiveAndRemainsAvailableOnControlStream)
    {
        auto background      = std::make_unique<FakeBackgroundThread>();
        auto* background_ptr = background.get();
        ContextState state{EndpointRole::Client, std::move(background)};
        auto opening = state.create_opening_call_message_builder();
        opening->open_call("active");
        const auto opened = state.open_call(std::move(opening));
        ASSERT_TRUE(opened);

        background_ptr->push_inbound(std::make_unique<FakeControlMessageView>(0, true));
        EXPECT_EQ(state.receive_call_message(*opened.value, std::nullopt).status,
                  interface::OperationStatus::ContextClosed);
        EXPECT_EQ(state.control_receive_status(), interface::MessageStatus::MessageAvailable);
        EXPECT_EQ(state.call_receive_status(*opened.value),
                  interface::MessageStatus::ContextClosed);
        auto close = state.receive_control_message(std::nullopt);
        ASSERT_TRUE(close);
        EXPECT_NE((*close.value)->close_connection(), nullptr);
    }

    TEST(ContextStateTest, AcceptsOpeningMessageThatAlsoClosesTheCall)
    {
        auto background = std::make_unique<FakeBackgroundThread>();
        background->push_inbound(std::make_unique<FakeCallMessageView>(2, true, true));
        ContextState state{EndpointRole::Client, std::move(background)};

        const auto accepted = state.accept_call();
        ASSERT_TRUE(accepted);
        EXPECT_EQ(*accepted.value, 2);
        auto opening_and_close = state.receive_call_message(2, std::nullopt);
        ASSERT_TRUE(opening_and_close);
        EXPECT_NE((*opening_and_close.value)->open_call(), nullptr);
        EXPECT_NE((*opening_and_close.value)->close_call(), nullptr);
        EXPECT_EQ(state.receive_call_message(2, std::nullopt).status,
                  interface::OperationStatus::PeerClosed);
    }

    TEST(ContextStateTest, IgnoresSpuriousInboundNotificationWhileAccepting)
    {
        auto background = std::make_unique<FakeBackgroundThread>();
        background->set_next_wait_result({.event = Event::InboundAvailable});
        ContextState state{EndpointRole::Client, std::move(background)};

        EXPECT_EQ(state.accept_call().status, interface::OperationStatus::Cancelled);
    }

    TEST(ContextStateTest, DeliversPeerConnectionCloseDispatchedDuringControlReceive)
    {
        auto background = std::make_unique<FakeBackgroundThread>();
        background->push_inbound(std::make_unique<FakeControlMessageView>(0, true));
        ContextState state{EndpointRole::Client, std::move(background)};

        auto result = state.receive_control_message(std::nullopt);
        ASSERT_TRUE(result);
        EXPECT_NE((*result.value)->close_connection(), nullptr);
    }

} // namespace
} // namespace exasol::udf::v2::internal
