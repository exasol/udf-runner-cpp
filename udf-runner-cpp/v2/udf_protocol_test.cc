#include "udf_protocol.hpp"

#include <gtest/gtest.h>

TEST(UdfProtocolTest, EncodesAndDecodesOpenCallFrame)
{
    exasol::udf::v2::third_party::flatbuffers::FlatBufferBuilder builder;
    const auto call_name = builder.CreateString("example");
    const auto open_call = exasol::udf::protocol::CreateOpenCall(builder, call_name);
    const auto message   = exasol::udf::protocol::CreateStreamMessage(builder, 0, 0, open_call);
    const auto frame     = exasol::udf::protocol::CreateFrame(builder, 7, message);
    builder.Finish(frame);

    ASSERT_TRUE(
        exasol::udf::protocol::verify_frame_buffer(builder.GetBufferPointer(), builder.GetSize()));
    const auto* decoded = exasol::udf::protocol::GetFrame(builder.GetBufferPointer());
    ASSERT_NE(decoded, nullptr);
    EXPECT_EQ(decoded->stream_id(), 7);
    ASSERT_NE(decoded->message(), nullptr);
    ASSERT_NE(decoded->message()->open_call(), nullptr);
    EXPECT_EQ(decoded->message()->open_call()->call_name()->str(), "example");
}
