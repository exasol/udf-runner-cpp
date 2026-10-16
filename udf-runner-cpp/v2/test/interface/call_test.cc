#include "../common/test_doubles.hpp"

#include <gtest/gtest.h>

namespace exasol::udf::v2
{

TEST(CallTest, ConvenienceMethodsDelegateToVirtualOperations)
{
    test::CallDouble call;
    EXPECT_EQ(call.receive_status(), MessageStatus::MessageAvailable);
    EXPECT_TRUE(call.receive());
    EXPECT_TRUE(call.send(CallMessageBuilder{}));
}

} // namespace exasol::udf::v2
