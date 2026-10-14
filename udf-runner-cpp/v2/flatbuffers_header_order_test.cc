#include <flatbuffers/flatbuffers.h>

#include <exasol/udf/v2/udf_protocol.hpp>

void ordinary_flatbuffers_first()
{
    ::flatbuffers::FlatBufferBuilder ordinary_builder;
    exasol::udf::v2::third_party::flatbuffers::FlatBufferBuilder isolated_builder;
    (void)ordinary_builder;
    (void)isolated_builder;
}

int main()
{
    ordinary_flatbuffers_first();
}
