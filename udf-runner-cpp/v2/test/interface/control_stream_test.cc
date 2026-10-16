#include "../common/test_doubles.hpp"

#include <gtest/gtest.h>

namespace exasol::udf::v2
{

TEST(ControlStreamTest, ConvenienceMethodsDelegateToVirtualOperations)
{
    test::ControlStreamDouble control_stream;
    EXPECT_EQ(control_stream.receive_status(), MessageStatus::NoMessage);
    EXPECT_TRUE(control_stream.receive());
    EXPECT_TRUE(control_stream.receive(Timeout{}));
    EXPECT_TRUE(control_stream.send(ControlMessageBuilder{}));
}

} // namespace exasol::udf::v2
