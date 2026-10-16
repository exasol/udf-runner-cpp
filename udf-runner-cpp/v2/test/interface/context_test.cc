#include "../common/test_doubles.hpp"

#include <gtest/gtest.h>

namespace exasol::udf::v2::interface
{

TEST(ContextTest, ConvenienceMethodsDelegateToVirtualOperations)
{
    test::ContextDouble context;
    EXPECT_EQ(context.receive_status(), MessageStatus::NoMessage);
    EXPECT_FALSE(context.is_new_inbound_call_available());
    EXPECT_EQ(context.wait_for_message(), MessageStatus::TimedOut);
    EXPECT_EQ(context.wait_for_message(Timeout{}), MessageStatus::TimedOut);
    EXPECT_EQ(context.accept_call().status, OperationStatus::Cancelled);
    EXPECT_EQ(context.open_call(CallMessageBuilder{}).status, OperationStatus::Cancelled);
    EXPECT_NE(&context.control_stream(), nullptr);
}

} // namespace exasol::udf::v2::interface
