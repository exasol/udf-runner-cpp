#include <exasol/udf/v2/internal/stream_registry.hpp>

#include "stream_management_test_utils.hpp"

#include <gtest/gtest.h>

namespace exasol::udf::v2::internal
{
namespace
{

    using test_support::make_call_message_view;
    using test_support::make_control_message_view;

    TEST(StreamRegistryTest, AllocatesStreamIdsByEndpointRole)
    {
        StreamRegistry client{EndpointRole::Client};
        StreamRegistry server{EndpointRole::Server};

        EXPECT_EQ(client.reserve_local_stream_id(), 1);
        EXPECT_EQ(client.reserve_local_stream_id(), 3);
        EXPECT_EQ(server.reserve_local_stream_id(), 2);
        EXPECT_EQ(server.reserve_local_stream_id(), 4);
    }

    TEST(StreamRegistryTest, RejectsNewLocalOwnedStreamAndDiscardsDelayedTraffic)
    {
        StreamRegistry registry{EndpointRole::Client};

        EXPECT_EQ(registry.dispatch(make_call_message_view(1)), DispatchResult::ProtocolError);
        ASSERT_EQ(registry.reserve_local_stream_id(), 1);

        EXPECT_EQ(registry.dispatch(make_call_message_view(1)), DispatchResult::Discarded);
    }

    TEST(StreamRegistryTest, DeliversPeerCloseOnceAndThenClosesCall)
    {
        StreamRegistry registry{EndpointRole::Client};
        ASSERT_EQ(registry.dispatch(make_call_message_view(2, true)), DispatchResult::Routed);
        ASSERT_EQ(registry.dispatch(make_call_message_view(2, false, true)),
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
        ASSERT_EQ(registry.dispatch(make_call_message_view(4, true)), DispatchResult::Routed);
        ASSERT_EQ(registry.dispatch(make_call_message_view(4, false, true)),
                  DispatchResult::Routed);
        static_cast<void>(registry.take_next_unaccepted_call());
        static_cast<void>(registry.take_call_message(4));
        static_cast<void>(registry.take_call_message(4));

        EXPECT_EQ(registry.dispatch(make_call_message_view(2)), DispatchResult::Discarded);
        EXPECT_EQ(registry.dispatch(make_call_message_view(2, true)),
                  DispatchResult::ProtocolError);
        EXPECT_EQ(registry.dispatch(make_call_message_view(6)), DispatchResult::ProtocolError);
    }

    TEST(StreamRegistryTest, DiscardsDelayedTrafficOnPreviouslyUsedLocalStreams)
    {
        StreamRegistry registry{EndpointRole::Client};
        ASSERT_EQ(registry.reserve_local_stream_id(), 1);
        ASSERT_TRUE(registry.register_local_call(1, false));
        ASSERT_TRUE(registry.mark_local_close_accepted(1));

        EXPECT_EQ(registry.dispatch(make_call_message_view(1)), DispatchResult::Routed);
        EXPECT_EQ(registry.dispatch(make_call_message_view(1, false, true)),
                  DispatchResult::Routed);
        EXPECT_EQ(registry.take_call_message(1).result, CallMessageResult::Message);
        auto peer_close = registry.take_call_message(1);
        ASSERT_EQ(peer_close.result, CallMessageResult::Message);
        EXPECT_NE(peer_close.message->close_call(), nullptr);

        EXPECT_EQ(registry.dispatch(make_call_message_view(1)), DispatchResult::Discarded);
        EXPECT_EQ(registry.dispatch(make_call_message_view(1, true)),
                  DispatchResult::ProtocolError);
        EXPECT_EQ(registry.dispatch(make_call_message_view(3)), DispatchResult::ProtocolError);
    }

    TEST(StreamRegistryTest, TracksPendingCallMessagesAcrossReadsAndCleanup)
    {
        StreamRegistry registry{EndpointRole::Client};
        EXPECT_FALSE(registry.has_any_pending_message());

        ASSERT_EQ(registry.dispatch(make_call_message_view(2, true)), DispatchResult::Routed);
        ASSERT_EQ(registry.dispatch(make_call_message_view(2)), DispatchResult::Routed);
        ASSERT_EQ(registry.dispatch(make_call_message_view(4, true)), DispatchResult::Routed);
        EXPECT_TRUE(registry.has_any_pending_message());

        EXPECT_EQ(registry.take_call_message(2).result, CallMessageResult::Message);
        EXPECT_TRUE(registry.has_any_pending_message());
        EXPECT_EQ(registry.take_call_message(2).result, CallMessageResult::Message);
        EXPECT_TRUE(registry.has_any_pending_message());
        EXPECT_EQ(registry.take_call_message(4).result, CallMessageResult::Message);
        EXPECT_FALSE(registry.has_any_pending_message());

        ASSERT_EQ(registry.dispatch(make_call_message_view(6, true)), DispatchResult::Routed);
        ASSERT_EQ(registry.dispatch(make_control_message_view(0)), DispatchResult::Routed);
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

        EXPECT_EQ(registry.dispatch(make_call_message_view(0)), DispatchResult::ProtocolError);
        EXPECT_EQ(registry.dispatch(make_control_message_view(1)), DispatchResult::ProtocolError);
        EXPECT_EQ(registry.dispatch(make_control_message_view(0)), DispatchResult::Routed);
        EXPECT_NE(registry.take_control_message(), nullptr);
    }

} // namespace
} // namespace exasol::udf::v2::internal
