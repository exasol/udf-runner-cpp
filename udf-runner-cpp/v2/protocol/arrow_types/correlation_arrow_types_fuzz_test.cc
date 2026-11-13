#include <exasol/udf/v2/protocol/arrow_types/range_run.hpp>
#include <exasol/udf/v2/protocol/arrow_types/run_end_encoded.hpp>

#include <arrow/array/array_primitive.h>
#include <arrow/array/array_run_end.h>
#include <arrow/array/builder_primitive.h>
#include <arrow/type.h>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory>
#include <vector>

namespace
{

namespace correlation = exasol::udf::v2::protocol::arrow_types::correlation;

constexpr std::size_t max_rows = 32;

[[noreturn]] void fuzz_failure()
{
    std::abort();
}

class Input
{
public:
    Input(const std::uint8_t* data, std::size_t size) : data(data), size(size)
    {
    }

    std::uint8_t read_byte()
    {
        if (position >= size)
        {
            return 0;
        }
        return data[position++];
    }

    std::uint64_t read_uint64()
    {
        std::uint64_t value = 0;
        for (unsigned int shift = 0; shift < 64; shift += 8)
        {
            value |= static_cast<std::uint64_t>(read_byte()) << shift;
        }
        return value;
    }

    std::int64_t read_int64()
    {
        return std::bit_cast<std::int64_t>(read_uint64());
    }

private:
    const std::uint8_t* data;
    std::size_t size;
    std::size_t position = 0;
};

std::shared_ptr<arrow::Array> make_run_ends(const std::vector<std::int64_t>& ends)
{
    arrow::Int64Builder builder;
    if (!builder.AppendValues(ends).ok())
    {
        fuzz_failure();
    }
    std::shared_ptr<arrow::Array> array;
    if (!builder.Finish(&array).ok())
    {
        fuzz_failure();
    }
    return array;
}

std::shared_ptr<arrow::Array> make_values(const std::vector<std::uint64_t>& values,
                                          const std::vector<bool>& nulls)
{
    arrow::UInt64Builder builder;
    for (std::size_t index = 0; index < values.size(); ++index)
    {
        const auto status = nulls[index] ? builder.AppendNull() : builder.Append(values[index]);
        if (!status.ok())
        {
            fuzz_failure();
        }
    }
    std::shared_ptr<arrow::Array> array;
    if (!builder.Finish(&array).ok())
    {
        fuzz_failure();
    }
    return array;
}

void fuzz_valid_round_trip(Input& input)
{
    const auto count = static_cast<std::size_t>(input.read_byte()) % (max_rows + 1);
    std::vector<std::uint64_t> row_ids;
    std::vector<std::uint64_t> group_ids;
    row_ids.reserve(count);
    group_ids.reserve(count);
    for (std::size_t index = 0; index < count; ++index)
    {
        row_ids.push_back(input.read_uint64());
        group_ids.push_back(input.read_uint64());
    }

    auto groups = correlation::create_run_end_encoded_correlation_ids(group_ids);
    auto rows   = correlation::create_row_id_range_run(row_ids, group_ids);
    if (!groups.ok() || !rows.ok())
    {
        fuzz_failure();
    }
    auto decoded_groups = correlation::decode_run_end_encoded_correlation_ids(*groups.ValueOrDie());
    auto decoded_rows   = correlation::decode_row_id_range_run(*rows.ValueOrDie());
    if (!decoded_groups.ok() || decoded_groups.ValueOrDie() != group_ids || !decoded_rows.ok() ||
        decoded_rows.ValueOrDie() != row_ids)
    {
        fuzz_failure();
    }

    const auto storage =
        std::static_pointer_cast<arrow::RunEndEncodedArray>(rows.ValueOrDie()->storage());
    const auto ends           = std::static_pointer_cast<arrow::Int64Array>(storage->run_ends());
    const auto values         = std::static_pointer_cast<arrow::UInt64Array>(storage->values());
    std::int64_t previous_end = 0;
    for (std::int64_t run = 0; run < ends->length(); ++run)
    {
        const auto end = ends->Value(run);
        if (end <= previous_end || end > static_cast<std::int64_t>(count) ||
            values->Value(run) != row_ids[static_cast<std::size_t>(previous_end)])
        {
            fuzz_failure();
        }
        for (auto index = previous_end + 1; index < end; ++index)
        {
            const auto current = static_cast<std::size_t>(index);
            if (group_ids[current] != group_ids[current - 1])
            {
                fuzz_failure();
            }
        }
        previous_end = end;
    }
    if (previous_end != static_cast<std::int64_t>(count))
    {
        fuzz_failure();
    }
}

void fuzz_malformed_runs(Input& input)
{
    const auto logical_length = static_cast<std::int64_t>(input.read_byte() % (max_rows + 1));
    const auto count          = static_cast<std::size_t>(input.read_byte() % (max_rows + 1));
    std::vector<std::int64_t> ends;
    std::vector<std::uint64_t> values;
    std::vector<bool> nulls;
    ends.reserve(count);
    values.reserve(count);
    nulls.reserve(count);
    for (std::size_t index = 0; index < count; ++index)
    {
        ends.push_back(input.read_int64());
        values.push_back(input.read_uint64());
        nulls.push_back((input.read_byte() & 1U) != 0);
    }

    auto storage = std::make_shared<arrow::RunEndEncodedArray>(
        arrow::run_end_encoded(arrow::int64(), arrow::uint64()), logical_length,
        make_run_ends(ends), make_values(values, nulls));
    auto repeated = correlation::decode_run_end_encoded_correlation_ids(*storage);
    auto extension =
        arrow::ExtensionType::WrapArray(std::make_shared<correlation::RangeRunType>(), storage);
    auto ranges = correlation::decode_row_id_range_run(*extension);

    std::vector<std::uint64_t> expected_repeated;
    std::vector<std::uint64_t> expected_ranges;
    bool valid_repeated      = true;
    bool valid_ranges        = true;
    std::size_t physical_run = 0;
    for (std::int64_t position = 0; position < logical_length; ++position)
    {
        while (physical_run < count && ends[physical_run] <= position)
        {
            ++physical_run;
        }
        if (physical_run == count || nulls[physical_run])
        {
            valid_repeated = false;
            break;
        }
        expected_repeated.push_back(values[physical_run]);
    }

    std::int64_t previous_end = 0;
    for (std::size_t run = 0; run < count; ++run)
    {
        const auto end = ends[run];
        if (nulls[run] || end <= previous_end || end > logical_length)
        {
            valid_ranges = false;
            break;
        }
        const auto length = static_cast<std::uint64_t>(end - previous_end);
        if (values[run] > std::numeric_limits<std::uint64_t>::max() - (length - 1))
        {
            valid_ranges = false;
        }
        if (valid_ranges)
        {
            for (std::uint64_t offset = 0; offset < length; ++offset)
            {
                expected_ranges.push_back(values[run] + offset);
            }
        }
        previous_end = end;
    }
    if (previous_end != logical_length)
    {
        valid_ranges = false;
    }
    if (repeated.ok() && (!valid_repeated || repeated.ValueOrDie() != expected_repeated))
    {
        fuzz_failure();
    }
    if (ranges.ok() && (!valid_ranges || ranges.ValueOrDie() != expected_ranges))
    {
        fuzz_failure();
    }
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    Input input(data, size);
    if ((input.read_byte() & 1U) == 0)
    {
        fuzz_valid_round_trip(input);
    }
    else
    {
        fuzz_malformed_runs(input);
    }
    return 0;
}
