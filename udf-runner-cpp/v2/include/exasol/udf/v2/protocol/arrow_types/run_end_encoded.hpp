#pragma once

#include <arrow/array/array_run_end.h>
#include <arrow/result.h>

#include <exasol/udf/v2/protocol/arrow_types/run_array_view.hpp>

#include <cstdint>
#include <memory>
#include <span>

namespace exasol::udf::v2::protocol::arrow_types::correlation
{

/// Encodes repeated uint64 values from their values and run lengths.
arrow::Result<std::shared_ptr<arrow::RunEndEncodedArray>> make_run_end(
    std::span<const std::uint64_t> values, std::span<const std::uint64_t> counts);

} // namespace exasol::udf::v2::protocol::arrow_types::correlation
