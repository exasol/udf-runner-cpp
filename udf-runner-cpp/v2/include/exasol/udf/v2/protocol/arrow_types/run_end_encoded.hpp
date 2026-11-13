#pragma once

#include <arrow/array/array_run_end.h>
#include <arrow/result.h>

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace exasol::udf::v2::protocol::arrow_types::correlation
{

/// Encodes repeated unsigned correlation IDs with signed 64-bit run ends.
arrow::Result<std::shared_ptr<arrow::RunEndEncodedArray>> create_run_end_encoded_correlation_ids(
    std::span<const std::uint64_t> ids);

/// Decodes unsigned correlation IDs from RunEndEncoded or plain uint64 storage.
arrow::Result<std::vector<std::uint64_t>> decode_run_end_encoded_correlation_ids(
    const arrow::Array& array);

} // namespace exasol::udf::v2::protocol::arrow_types::correlation
