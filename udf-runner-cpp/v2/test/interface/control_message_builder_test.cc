#include <exasol/udf/v2/interface/message_builder/control_message_builder.hpp>

#include <gtest/gtest.h>

namespace exasol::udf::v2
{

TEST(ControlMessageBuilderTest, BuildsCompositeMessage)
{
    ControlMessageBuilder builder;
    builder.server_capabilities(2, 1, std::endian::little, 4)
        .keep_alive()
        .add_payload("control", "value")
        .add_payload("second_control", "value", true)
        .error("E_CONTROL", "control failure")
        .close_connection();

    EXPECT_TRUE(builder.message().server_capabilities.has_value());
    EXPECT_TRUE(builder.message().error.has_value());
    EXPECT_TRUE(builder.message().close_connection);
    const auto message = std::move(builder).take();
    EXPECT_TRUE(message.keep_alive);
}

} // namespace exasol::udf::v2
