// [impl~udf-v2-frame-api~1 -> dsn~udf-v2-frame-schema-implementation~1]
#ifndef EXASOL_UDF_V2_UDF_PROTOCOL_HPP_
#define EXASOL_UDF_V2_UDF_PROTOCOL_HPP_

#include <cstddef>

// The generated API uses FlatBuffers runtime types. Rewrite the runtime
// namespace only while parsing the generated header, so the global
// ::flatbuffers namespace is never part of this library's API or ABI.
#define flatbuffers exasol::udf::v2::third_party::flatbuffers
#include "udf_protocol_generated.h"
#undef flatbuffers

namespace exasol::udf::protocol
{

bool verify_frame_buffer(const void* data, std::size_t size);

} // namespace exasol::udf::protocol

#endif // EXASOL_UDF_V2_UDF_PROTOCOL_HPP_
