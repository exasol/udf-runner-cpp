#include <exasol/udf/v2/context/high_level_context.hpp>

#include "test/common/test_doubles.hpp"

#include <gtest/gtest.h>

namespace exasol::udf::v2::context
{

TEST(HighLevelContextTest, InitializesProtocolCapabilities)
{
    test::ContextDouble context;
    HighLevelContext high_level(context);

    ASSERT_TRUE(high_level.initialize(4));
    ASSERT_TRUE(context.last_message.has_value());
    ASSERT_TRUE(context.last_message->server_capabilities.has_value());
    EXPECT_EQ(context.last_message->server_capabilities->major, 2U);
    EXPECT_EQ(context.last_message->server_capabilities->number_of_supported_workers, 4U);
    ASSERT_TRUE(context.last_message->payloads.has_value());
    ASSERT_EQ(context.last_message->payloads->size(), 2U);
    EXPECT_EQ(context.last_message->payloads->at(0).name, "high_level_name");
    EXPECT_EQ(context.last_message->payloads->at(0).value, protocol_name);
    EXPECT_EQ(context.last_message->payloads->at(1).name, "high_level_version");
    EXPECT_EQ(context.last_message->payloads->at(1).value, protocol_version);
}

TEST(HighLevelContextTest, OpensNamedCallWithPayloads)
{
    test::ContextDouble context;
    HighLevelContext high_level(context);

    const interface::Payloads payloads{{"script_name", "example", false}};
    ASSERT_TRUE(high_level.open_call("run", payloads));
    ASSERT_TRUE(context.last_opening.has_value());
    ASSERT_TRUE(context.last_opening->open_call.has_value());
    EXPECT_EQ(context.last_opening->open_call->call_name, "run");
    ASSERT_TRUE(context.last_opening->payloads.has_value());
    ASSERT_EQ(context.last_opening->payloads->size(), 1U);
    EXPECT_EQ(context.last_opening->payloads->front().name, "script_name");
}

TEST(HighLevelContextTest, RejectsZeroWorkers)
{
    test::ContextDouble context;
    HighLevelContext high_level(context);

    const auto result = high_level.initialize(0);
    EXPECT_EQ(result.status, interface::OperationStatus::ProtocolError);
    EXPECT_EQ(result.error->code(), "invalid_worker_count");
}

} // namespace exasol::udf::v2::context
