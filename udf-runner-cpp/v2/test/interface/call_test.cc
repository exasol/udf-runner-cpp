#include "../common/test_doubles.hpp"

#include <gtest/gtest.h>

namespace exasol::udf::v2
{

TEST(CallTest, ConvenienceMethodsDelegateToVirtualOperations)
{
    test::CallDouble call;
    EXPECT_EQ(call.receive_status(), MessageStatus::MessageAvailable);
    EXPECT_TRUE(call.receive());
    EXPECT_TRUE(call.receive(Timeout{}));
    EXPECT_TRUE(call.send(CallMessageBuilder{}));
    EXPECT_TRUE(call.send(CallMessageBuilder{}, Timeout{}));
}

} // namespace exasol::udf::v2
