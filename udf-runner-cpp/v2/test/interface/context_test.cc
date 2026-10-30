#include "../common/test_doubles.hpp"

#include <gtest/gtest.h>

#include <type_traits>

namespace exasol::udf::v2::interface
{

static_assert(std::is_abstract_v<Context>);

TEST(ContextTest, ConvenienceMethodsDelegateToVirtualOperations)
{
    test::ContextDouble context;
    EXPECT_EQ(context.receive_status(), MessageStatus::NoMessage);
    EXPECT_FALSE(context.is_new_inbound_call_available());
    EXPECT_EQ(context.wait_for_message(), MessageStatus::TimedOut);
    EXPECT_EQ(context.wait_for_message(Timeout{}), MessageStatus::TimedOut);
    EXPECT_EQ(context.accept_call().status, OperationStatus::Cancelled);
    auto opening_message = context.create_call_message_builder();
    opening_message->open_call("nested");
    EXPECT_EQ(context.open_call(std::move(opening_message)).status, OperationStatus::Cancelled);
    EXPECT_NE(&context.control_stream(), nullptr);
}

} // namespace exasol::udf::v2::interface
