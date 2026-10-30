#include <exasol/udf/v2/internal/control_message_builder_impl.hpp>
#include <exasol/udf/v2/internal/message_builder_factory.hpp>

#include <gtest/gtest.h>

#include <type_traits>

namespace exasol::udf::v2::internal
{

static_assert(std::is_abstract_v<interface::ControlMessageBuilder>);

TEST(ControlMessageBuilderTest, AddPayloadIsNotImplemented)
{
    EXPECT_DEATH(
        {
            auto builder = MessageBuilderFactory::create_control_builder();
            builder->add_payload("control", "value");
        },
        "");
}

TEST(ControlMessageBuilderTest, ServerCapabilitiesIsNotImplemented)
{
    EXPECT_DEATH(
        {
            auto builder = MessageBuilderFactory::create_control_builder();
            builder->server_capabilities(2, 1, std::endian::little, 4);
        },
        "");
}

TEST(ControlMessageBuilderTest, BuildsCompositeMessage)
{
    auto builder = MessageBuilderFactory::create_control_builder();
    builder->keep_alive().error("E_CONTROL", "control failure").close_connection();

    const auto* builder_impl = dynamic_cast<const ControlMessageBuilderImpl*>(builder.get());
    ASSERT_NE(builder_impl, nullptr);
    const auto view = builder_impl->encoding_view();
    EXPECT_EQ(view.stream_id, 0);
    EXPECT_TRUE(view.message.error.has_value());
    EXPECT_TRUE(view.message.close_connection);
    EXPECT_TRUE(view.message.keep_alive);
}

} // namespace exasol::udf::v2::internal
