#include <exasol/udf/v2/internal/call_impl.hpp>

#include "../common/interface_mocks.hpp"
#include "context_state_mock.hpp"
#include "stream_management_test_utils.hpp"

#include <chrono>
#include <memory>
#include <utility>

#include <gtest/gtest.h>

namespace exasol::udf::v2::internal
{
namespace
{

    using test::MockCallMessageBuilder;
    using test_support::k_first_client_stream_id;

    TEST(CallImplTest, ReportsContextClosedAfterSharedStateExpires)
    {
        std::unique_ptr<CallImpl> call;
        {
            auto state = std::make_shared<test_support::MockContextState>();
            call       = std::make_unique<CallImpl>(state, k_first_client_stream_id);
        }

        EXPECT_EQ(call->receive_status(), interface::MessageStatus::ContextClosed);
        EXPECT_EQ(call->receive().status, interface::OperationStatus::ContextClosed);
        EXPECT_EQ(call->receive(interface::Timeout{std::chrono::seconds{1}}).status,
                  interface::OperationStatus::ContextClosed);
        EXPECT_EQ(call->create_message_builder(), nullptr);
        std::unique_ptr<interface::CallMessageBuilder> message =
            std::make_unique<testing::NiceMock<MockCallMessageBuilder>>();
        EXPECT_EQ(call->send(std::move(message)).status, interface::OperationStatus::ContextClosed);
        EXPECT_NE(message, nullptr);
    }

} // namespace
} // namespace exasol::udf::v2::internal
