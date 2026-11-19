#include <exasol/udf/v2/internal/stream_registry.hpp>

#include "stream_management_test_utils.hpp"

#include <gtest/gtest.h>

namespace exasol::udf::v2::internal
{
namespace
{

    using test_support::k_control_stream_id;
    using test_support::k_first_client_stream_id;
    using test_support::k_first_server_stream_id;
    using test_support::k_second_client_stream_id;
    using test_support::k_second_server_stream_id;
    using test_support::k_third_server_stream_id;
    using test_support::make_call_message_view;
    using test_support::make_control_message_view;

    TEST(StreamRegistryTest, AllocatesStreamIdsByEndpointRole)
    {
        StreamRegistry client{EndpointRole::Client};
        StreamRegistry server{EndpointRole::Server};

        EXPECT_EQ(client.reserve_local_stream_id(), k_first_client_stream_id);
        EXPECT_EQ(client.reserve_local_stream_id(), k_second_client_stream_id);
        EXPECT_EQ(server.reserve_local_stream_id(), k_first_server_stream_id);
        EXPECT_EQ(server.reserve_local_stream_id(), k_second_server_stream_id);
    }

    TEST(StreamRegistryTest, RejectsNewLocalOwnedStreamAndDiscardsDelayedTraffic)
    {
        StreamRegistry registry{EndpointRole::Client};

        EXPECT_EQ(registry.dispatch(make_call_message_view(k_first_client_stream_id)),
                  DispatchResult::ProtocolError);
        ASSERT_EQ(registry.reserve_local_stream_id(), k_first_client_stream_id);

        EXPECT_EQ(registry.dispatch(make_call_message_view(k_first_client_stream_id)),
                  DispatchResult::Discarded);
    }

    TEST(StreamRegistryTest, DeliversPeerCloseOnceAndThenClosesCall)
    {
        StreamRegistry registry{EndpointRole::Client};
        ASSERT_EQ(registry.dispatch(make_call_message_view(k_first_server_stream_id, true)),
                  DispatchResult::Routed);
        ASSERT_EQ(registry.dispatch(make_call_message_view(k_first_server_stream_id, false, true)),
                  DispatchResult::Routed);
        ASSERT_EQ(registry.take_next_unaccepted_call(), k_first_server_stream_id);

        auto opening = registry.take_call_message(k_first_server_stream_id);
        ASSERT_EQ(opening.result, CallMessageResult::Message);
        EXPECT_NE(opening.message->open_call(), nullptr);

        auto closing = registry.take_call_message(k_first_server_stream_id);
        ASSERT_EQ(closing.result, CallMessageResult::Message);
        EXPECT_NE(closing.message->close_call(), nullptr);
        EXPECT_EQ(registry.take_call_message(k_first_server_stream_id).result,
                  CallMessageResult::PeerClosed);
    }

    TEST(StreamRegistryTest, RoutesCallMessageToActiveStream)
    {
        StreamRegistry registry{EndpointRole::Client};
        ASSERT_EQ(registry.dispatch(make_call_message_view(k_first_server_stream_id, true)),
                  DispatchResult::Routed);
        ASSERT_EQ(registry.take_next_unaccepted_call(), k_first_server_stream_id);

        const auto opening = registry.take_call_message(k_first_server_stream_id);
        ASSERT_EQ(opening.result, CallMessageResult::Message);
        ASSERT_NE(opening.message->open_call(), nullptr);

        EXPECT_EQ(registry.dispatch(make_call_message_view(k_first_server_stream_id)),
                  DispatchResult::Routed);

        const auto message = registry.take_call_message(k_first_server_stream_id);
        ASSERT_EQ(message.result, CallMessageResult::Message);
        EXPECT_EQ(message.message->stream_id(), k_first_server_stream_id);
        EXPECT_EQ(message.message->open_call(), nullptr);
        EXPECT_EQ(message.message->close_call(), nullptr);
    }

    TEST(StreamRegistryTest, DiscardsDelayedTrafficOnPreviouslyUsedPeerStreams)
    {
        StreamRegistry registry{EndpointRole::Client};
        ASSERT_EQ(registry.dispatch(make_call_message_view(k_second_server_stream_id, true)),
                  DispatchResult::Routed);
        ASSERT_EQ(registry.dispatch(make_call_message_view(k_second_server_stream_id, false, true)),
                  DispatchResult::Routed);
        static_cast<void>(registry.take_next_unaccepted_call());
        static_cast<void>(registry.take_call_message(k_second_server_stream_id));
        static_cast<void>(registry.take_call_message(k_second_server_stream_id));

        EXPECT_EQ(registry.dispatch(make_call_message_view(k_first_server_stream_id)),
                  DispatchResult::Discarded);
        EXPECT_EQ(registry.dispatch(make_call_message_view(k_first_server_stream_id, true)),
                  DispatchResult::ProtocolError);
        EXPECT_EQ(registry.dispatch(make_call_message_view(k_third_server_stream_id)),
                  DispatchResult::ProtocolError);
    }

    TEST(StreamRegistryTest, DiscardsDelayedTrafficOnPreviouslyUsedLocalStreams)
    {
        StreamRegistry registry{EndpointRole::Client};
        ASSERT_EQ(registry.reserve_local_stream_id(), k_first_client_stream_id);
        ASSERT_TRUE(registry.register_local_call(k_first_client_stream_id, false));
        ASSERT_TRUE(registry.mark_local_close_accepted(k_first_client_stream_id));

        EXPECT_EQ(registry.dispatch(make_call_message_view(k_first_client_stream_id)),
                  DispatchResult::Routed);
        EXPECT_EQ(registry.dispatch(make_call_message_view(k_first_client_stream_id, false, true)),
                  DispatchResult::Routed);
        EXPECT_EQ(registry.take_call_message(k_first_client_stream_id).result,
                  CallMessageResult::Message);
        auto peer_close = registry.take_call_message(k_first_client_stream_id);
        ASSERT_EQ(peer_close.result, CallMessageResult::Message);
        EXPECT_NE(peer_close.message->close_call(), nullptr);

        EXPECT_EQ(registry.dispatch(make_call_message_view(k_first_client_stream_id)),
                  DispatchResult::Discarded);
        EXPECT_EQ(registry.dispatch(make_call_message_view(k_first_client_stream_id, true)),
                  DispatchResult::ProtocolError);
        EXPECT_EQ(registry.dispatch(make_call_message_view(k_second_client_stream_id)),
                  DispatchResult::ProtocolError);
    }

    TEST(StreamRegistryTest, TracksPendingCallMessagesAcrossReadsAndCleanup)
    {
        StreamRegistry registry{EndpointRole::Client};
        EXPECT_FALSE(registry.has_any_pending_message());

        ASSERT_EQ(registry.dispatch(make_call_message_view(k_first_server_stream_id, true)),
                  DispatchResult::Routed);
        ASSERT_EQ(registry.dispatch(make_call_message_view(k_first_server_stream_id)),
                  DispatchResult::Routed);
        ASSERT_EQ(registry.dispatch(make_call_message_view(k_second_server_stream_id, true)),
                  DispatchResult::Routed);
        EXPECT_TRUE(registry.has_any_pending_message());

        EXPECT_EQ(registry.take_call_message(k_first_server_stream_id).result,
                  CallMessageResult::Message);
        EXPECT_TRUE(registry.has_any_pending_message());
        EXPECT_EQ(registry.take_call_message(k_first_server_stream_id).result,
                  CallMessageResult::Message);
        EXPECT_TRUE(registry.has_any_pending_message());
        EXPECT_EQ(registry.take_call_message(k_second_server_stream_id).result,
                  CallMessageResult::Message);
        EXPECT_FALSE(registry.has_any_pending_message());

        ASSERT_EQ(registry.dispatch(make_call_message_view(k_third_server_stream_id, true)),
                  DispatchResult::Routed);
        ASSERT_EQ(registry.dispatch(make_control_message_view(k_control_stream_id)),
                  DispatchResult::Routed);
        registry.discard_call_streams();
        EXPECT_TRUE(registry.has_any_pending_message());
        EXPECT_NE(registry.take_control_message(), nullptr);
        EXPECT_FALSE(registry.has_any_pending_message());
    }

    TEST(StreamRegistryTest, EnforcesLocalCloseState)
    {
        StreamRegistry registry{EndpointRole::Client};
        ASSERT_EQ(registry.reserve_local_stream_id(), k_first_client_stream_id);
        ASSERT_TRUE(registry.register_local_call(k_first_client_stream_id, false));

        EXPECT_EQ(registry.call_send_status(k_first_client_stream_id),
                  interface::OperationStatus::Ok);
        EXPECT_TRUE(registry.mark_local_close_accepted(k_first_client_stream_id));
        EXPECT_EQ(registry.call_send_status(k_first_client_stream_id),
                  interface::OperationStatus::ProtocolError);
        EXPECT_FALSE(registry.mark_local_close_accepted(k_first_client_stream_id));
    }

    TEST(StreamRegistryTest, EnforcesMessageTypeAndStreamIdRules)
    {
        StreamRegistry registry{EndpointRole::Client};

        EXPECT_EQ(registry.dispatch(make_call_message_view(k_control_stream_id)),
                  DispatchResult::ProtocolError);
        EXPECT_EQ(registry.dispatch(make_control_message_view(k_first_client_stream_id)),
                  DispatchResult::ProtocolError);
        EXPECT_EQ(registry.dispatch(make_control_message_view(k_control_stream_id)),
                  DispatchResult::Routed);
        EXPECT_NE(registry.take_control_message(), nullptr);
    }

} // namespace
} // namespace exasol::udf::v2::internal
