#include <exasol/udf/v2/internal/control_message_builder_impl.hpp>
#include <exasol/udf/v2/internal/message_builder_factory.hpp>

#include <gtest/gtest.h>

#include <type_traits>

namespace exasol::udf::v2::internal
{

static_assert(std::is_abstract_v<interface::ControlMessageBuilder>);

TEST(ControlMessageBuilderTest, StoresServerCapabilities)
{
    auto builder = MessageBuilderFactory::create_control_builder();
    builder->server_capabilities(2, 1, std::endian::little, 4)
        .server_capabilities(3, 2, std::endian::big, 8);

    const auto* builder_impl = dynamic_cast<const ControlMessageBuilderImpl*>(builder.get());
    ASSERT_NE(builder_impl, nullptr);
    const auto& capabilities = builder_impl->message().server_capabilities();
    ASSERT_TRUE(capabilities.has_value());
    EXPECT_EQ(capabilities->major(), 3);
    EXPECT_EQ(capabilities->minor(), 2);
    EXPECT_EQ(capabilities->endianness(), std::endian::big);
    EXPECT_EQ(capabilities->number_of_supported_workers(), 8);
}

TEST(ControlMessageBuilderTest, StoresTextAndBinaryPayloadsInOrder)
{
    auto builder = MessageBuilderFactory::create_control_builder();
    builder->add_payload("text", "value").add_payload("binary", "\x01\x02", true);

    const auto* builder_impl = dynamic_cast<const ControlMessageBuilderImpl*>(builder.get());
    ASSERT_NE(builder_impl, nullptr);
    const auto& payloads = builder_impl->message().payloads();
    ASSERT_EQ(payloads.size(), 2);
    EXPECT_EQ(payloads[0].name(), "text");
    EXPECT_EQ(payloads[0].value(), "value");
    EXPECT_FALSE(payloads[0].is_binary());
    EXPECT_EQ(payloads[1].name(), "binary");
    EXPECT_EQ(payloads[1].value(), std::string("\x01\x02", 2));
    EXPECT_TRUE(payloads[1].is_binary());
}

TEST(ControlMessageBuilderTest, BuildsCompositeMessage)
{
    auto builder = MessageBuilderFactory::create_control_builder();
    builder->server_capabilities(2, 1, std::endian::little, 4)
        .keep_alive()
        .add_payload("control", "value")
        .error("E_CONTROL", "control failure")
        .close_connection();

    const auto* builder_impl = dynamic_cast<const ControlMessageBuilderImpl*>(builder.get());
    ASSERT_NE(builder_impl, nullptr);
    const auto& message = builder_impl->message();
    EXPECT_EQ(message.stream_id(), 0);
    EXPECT_TRUE(message.server_capabilities().has_value());
    ASSERT_EQ(message.payloads().size(), 1);
    EXPECT_EQ(message.payloads()[0].name(), "control");
    EXPECT_EQ(message.payloads()[0].value(), "value");
    EXPECT_FALSE(message.payloads()[0].is_binary());
    EXPECT_TRUE(message.error().has_value());
    EXPECT_TRUE(message.is_close_connection());
    EXPECT_TRUE(message.is_keep_alive());
}

} // namespace exasol::udf::v2::internal
