#include "udf_protocol.hpp"

#include <flatbuffers/flatbuffers.h>

void isolated_flatbuffers_first() {
    exasol::udf::v2::third_party::flatbuffers::FlatBufferBuilder isolated_builder;
    ::flatbuffers::FlatBufferBuilder ordinary_builder;
    (void)ordinary_builder;
    (void)isolated_builder;
}
