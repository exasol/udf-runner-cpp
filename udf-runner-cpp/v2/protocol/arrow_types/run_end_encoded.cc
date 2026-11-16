#include <exasol/udf/v2/protocol/arrow_types/run_end_encoded.hpp>

#include <arrow/array/builder_primitive.h>
#include <arrow/status.h>

#include <cstdint>
#include <limits>
#include <memory>

namespace exasol::udf::v2::protocol::arrow_types::correlation
{

arrow::Result<std::shared_ptr<arrow::RunEndEncodedArray>> make_run_end(
    std::span<const std::uint64_t> values, std::span<const std::uint64_t> counts)
{
    if (values.size() != counts.size())
    {
        return arrow::Status::Invalid("run-end values and counts must have equal length");
    }

    arrow::Int64Builder run_ends_builder;
    arrow::UInt64Builder values_builder;
    std::int64_t total = 0;
    for (std::size_t run = 0; run < values.size(); ++run)
    {
        const auto count = counts[run];
        if (count == 0 ||
            count > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max() - total))
        {
            return arrow::Status::Invalid("run-end count is zero or total exceeds int64 length");
        }
        total += static_cast<std::int64_t>(count);
        ARROW_RETURN_NOT_OK(run_ends_builder.Append(total));
        ARROW_RETURN_NOT_OK(values_builder.Append(values[run]));
    }

    std::shared_ptr<arrow::Array> run_ends;
    std::shared_ptr<arrow::Array> value_array;
    ARROW_RETURN_NOT_OK(run_ends_builder.Finish(&run_ends));
    ARROW_RETURN_NOT_OK(values_builder.Finish(&value_array));
    return arrow::RunEndEncodedArray::Make(total, run_ends, value_array);
}

} // namespace exasol::udf::v2::protocol::arrow_types::correlation
