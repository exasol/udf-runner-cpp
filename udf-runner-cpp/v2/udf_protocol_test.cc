#include "udf_protocol.hpp"

#include <cassert>
#include <cstdint>

int main() {
    exasol::udf::v2::third_party::flatbuffers::FlatBufferBuilder builder;
    const auto call_name = builder.CreateString("example");
    const auto open_call = exasol::udf::protocol::CreateOpenCall(builder, call_name);
    const auto message = exasol::udf::protocol::CreateStreamMessage(
        builder, 0, 0, open_call);
    const auto frame = exasol::udf::protocol::CreateFrame(builder, 7, message);
    builder.Finish(frame);

    assert(exasol::udf::protocol::VerifyFrameBuffer(
        builder.GetBufferPointer(), builder.GetSize()));
    const auto* decoded = exasol::udf::protocol::GetFrame(builder.GetBufferPointer());
    assert(decoded->stream_id() == 7);
    assert(decoded->message()->open_call()->call_name()->str() == "example");
}
