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
    const auto run_count = static_cast<std::size_t>(input.read_byte()) % (max_rows + 1);
    std::vector<std::uint64_t> range_values;
    std::vector<std::uint64_t> range_counts;
    std::vector<std::uint64_t> repeated_values;
    std::vector<std::uint64_t> repeated_counts;
    std::vector<std::uint64_t> expected_ranges;
    std::vector<std::uint64_t> expected_repeated;
    for (std::size_t run = 0; run < run_count; ++run)
    {
        const auto range_value    = input.read_uint64();
        const auto repeated_value = input.read_uint64();
        auto range_count          = static_cast<std::uint64_t>(input.read_byte() % 4) + 1;
        const auto repeated_count = static_cast<std::uint64_t>(input.read_byte() % 4) + 1;
        if (range_value > std::numeric_limits<std::uint64_t>::max() - (range_count - 1))
        {
            range_count = 1;
        }
        range_values.push_back(range_value);
        range_counts.push_back(range_count);
        repeated_values.push_back(repeated_value);
        repeated_counts.push_back(repeated_count);
        for (std::uint64_t offset = 0; offset < range_count; ++offset)
        {
            expected_ranges.push_back(range_value + offset);
        }
        expected_repeated.insert(expected_repeated.end(), repeated_count, repeated_value);
    }

    auto repeated = correlation::create_run_end(repeated_values, repeated_counts);
    auto ranges   = correlation::create_range_run(range_values, range_counts);
    if (!repeated.ok() || !ranges.ok())
    {
        fuzz_failure();
    }
    auto decoded_repeated = correlation::decode_run_end(*repeated.ValueOrDie());
    auto decoded_ranges   = correlation::decode_range_run(*ranges.ValueOrDie());
    if (!decoded_repeated.ok() || decoded_repeated.ValueOrDie() != expected_repeated ||
        !decoded_ranges.ok() || decoded_ranges.ValueOrDie() != expected_ranges)
    {
        fuzz_failure();
    }

    const auto storage =
        std::static_pointer_cast<arrow::RunEndEncodedArray>(ranges.ValueOrDie()->storage());
    const auto range_ends   = std::static_pointer_cast<arrow::Int64Array>(storage->run_ends());
    const auto range_starts = std::static_pointer_cast<arrow::UInt64Array>(storage->values());
    const auto repeated_ends =
        std::static_pointer_cast<arrow::Int64Array>(repeated.ValueOrDie()->run_ends());
    const auto repeated_starts =
        std::static_pointer_cast<arrow::UInt64Array>(repeated.ValueOrDie()->values());
    if (range_ends->length() != static_cast<std::int64_t>(run_count) ||
        repeated_ends->length() != static_cast<std::int64_t>(run_count))
    {
        fuzz_failure();
    }
    std::int64_t range_total    = 0;
    std::int64_t repeated_total = 0;
    for (std::size_t run = 0; run < run_count; ++run)
    {
        range_total += static_cast<std::int64_t>(range_counts[run]);
        repeated_total += static_cast<std::int64_t>(repeated_counts[run]);
        if (range_ends->Value(static_cast<std::int64_t>(run)) != range_total ||
            range_starts->Value(static_cast<std::int64_t>(run)) != range_values[run] ||
            repeated_ends->Value(static_cast<std::int64_t>(run)) != repeated_total ||
            repeated_starts->Value(static_cast<std::int64_t>(run)) != repeated_values[run])
        {
            fuzz_failure();
        }
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
    auto repeated = correlation::decode_run_end(*storage);
    auto extension =
        arrow::ExtensionType::WrapArray(std::make_shared<correlation::RangeRunType>(), storage);
    auto ranges = correlation::decode_range_run(*extension);

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
