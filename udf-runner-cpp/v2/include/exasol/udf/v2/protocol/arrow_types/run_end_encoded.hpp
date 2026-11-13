#pragma once

#include <arrow/array/array_run_end.h>
#include <arrow/result.h>

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace exasol::udf::v2::protocol::arrow_types::correlation
{

/// Encodes repeated uint64 values from their values and run lengths.
arrow::Result<std::shared_ptr<arrow::RunEndEncodedArray>> create_run_end(
    std::span<const std::uint64_t> values, std::span<const std::uint64_t> counts);

/// Decodes uint64 values from RunEndEncoded or plain uint64 storage.
arrow::Result<std::vector<std::uint64_t>> decode_run_end(const arrow::Array& array);

} // namespace exasol::udf::v2::protocol::arrow_types::correlation
