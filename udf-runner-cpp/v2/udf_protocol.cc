#include "udf_protocol.hpp"

namespace exasol::udf::protocol {

bool VerifyFrameBuffer(const void* data, std::size_t size) {
    using IsolatedVerifier =
        exasol::udf::v2::third_party::flatbuffers::Verifier;
    IsolatedVerifier verifier(static_cast<const uint8_t*>(data), size);
    return verifier.VerifyBuffer<Frame>();
}

}  // namespace exasol::udf::protocol
