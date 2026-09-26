// [impl~udf-v2-frame-verification~1 -> dsn~udf-v2-frame-schema-implementation~1]
#include "udf_protocol.hpp"

namespace exasol::udf::protocol
{

bool verify_frame_buffer(const void* data, std::size_t size)
{
    using IsolatedVerifier = exasol::udf::v2::third_party::flatbuffers::Verifier;
    IsolatedVerifier verifier(static_cast<const uint8_t*>(data), size);
    return verifier.VerifyBuffer<Frame>();
}

} // namespace exasol::udf::protocol
