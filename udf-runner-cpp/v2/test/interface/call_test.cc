#include "../common/test_doubles.hpp"

#include <gtest/gtest.h>

#include <type_traits>

namespace exasol::udf::v2::interface
{

static_assert(std::is_abstract_v<Call>);

TEST(CallTest, AbstractInterfaceOperationsCanBeImplemented)
{
    test::CallDouble call;
    EXPECT_EQ(call.receive_status(), MessageStatus::MessageAvailable);
    [[maybe_unused]] auto builder = call.create_message_builder();
    EXPECT_TRUE(call.receive());
    EXPECT_TRUE(call.receive(Timeout{}));
    EXPECT_TRUE(call.send(call.create_message_builder()));
    EXPECT_TRUE(call.send(call.create_message_builder(), Timeout{}));
}

} // namespace exasol::udf::v2::interface
