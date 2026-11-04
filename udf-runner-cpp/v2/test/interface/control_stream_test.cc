#include "../common/test_doubles.hpp"

#include <gtest/gtest.h>

#include <type_traits>

namespace exasol::udf::v2::interface
{

static_assert(std::is_abstract_v<ControlStream>);

TEST(ControlStreamTest, AbstractInterfaceOperationsCanBeImplemented)
{
    test::ControlStreamDouble control_stream;
    EXPECT_EQ(control_stream.receive_status(), MessageStatus::NoMessage);
    [[maybe_unused]] auto builder = control_stream.create_message_builder();
    EXPECT_TRUE(control_stream.receive());
    EXPECT_TRUE(control_stream.receive(Timeout{}));
    EXPECT_TRUE(control_stream.send(control_stream.create_message_builder()));
}

} // namespace exasol::udf::v2::interface
