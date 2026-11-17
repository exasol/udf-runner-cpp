#include <protocol/arrow_types/arrow_types_test_helpers.hpp>

#include <exasol/udf/v2/protocol/arrow_types/run_end_encoded.hpp>

#include <arrow/array/array_primitive.h>
#include <arrow/array/array_run_end.h>
#include <arrow/type.h>
#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

namespace exasol::udf::v2::protocol::arrow_types::correlation
{
namespace
{

    TEST(RunEndEncodedTest, EncodesAndDecodesRepeatedIds)
    {
        const std::vector<std::uint64_t> ids{10, 10, 20, 20};
        const std::vector<std::uint64_t> run_values{10, 20};
        const std::vector<std::uint64_t> counts{2, 2};
        auto result = make_run_end(run_values, counts);
        ASSERT_TRUE(result.ok()) << result.status();
        const auto& encoded = result.ValueOrDie();
        const auto ends     = std::static_pointer_cast<arrow::Int64Array>(encoded->run_ends());
        const auto values   = std::static_pointer_cast<arrow::UInt64Array>(encoded->values());
        ASSERT_EQ(ends->length(), 2);
        EXPECT_EQ(ends->Value(0), 2);
        EXPECT_EQ(ends->Value(1), 4);
        EXPECT_EQ(values->Value(0), 10);
        EXPECT_EQ(values->Value(1), 20);

        RunArrayView<arrow::RunEndEncodedArray> view(encoded);
        expect_all_values(view, ids);
        EXPECT_THROW(static_cast<void>(view.find(-1)), std::out_of_range);
        EXPECT_THROW(static_cast<void>(view.find(5)), std::out_of_range);
    }

    TEST(RunEndEncodedTest, SupportsEmptyRuns)
    {
        const std::vector<std::uint64_t> no_values;
        auto empty = make_run_end(no_values, no_values);
        ASSERT_TRUE(empty.ok()) << empty.status();
        EXPECT_EQ(empty.ValueOrDie()->run_ends()->length(), 0);
        EXPECT_EQ(empty.ValueOrDie()->values()->length(), 0);
        RunArrayView<arrow::RunEndEncodedArray> view(empty.ValueOrDie());
        expect_all_values(view, no_values);
    }

    TEST(RunEndEncodedTest, PreservesExplicitRuns)
    {
        const std::vector<std::uint64_t> values{10, 10};
        const std::vector<std::uint64_t> counts{1, 2};
        auto result = make_run_end(values, counts);
        ASSERT_TRUE(result.ok()) << result.status();
        const auto ends =
            std::static_pointer_cast<arrow::Int64Array>(result.ValueOrDie()->run_ends());
        const auto encoded_values =
            std::static_pointer_cast<arrow::UInt64Array>(result.ValueOrDie()->values());
        ASSERT_EQ(ends->length(), 2);
        ASSERT_EQ(encoded_values->length(), 2);
        EXPECT_EQ(ends->Value(0), 1);
        EXPECT_EQ(ends->Value(1), 3);
        EXPECT_EQ(encoded_values->Value(0), 10);
        EXPECT_EQ(encoded_values->Value(1), 10);
        RunArrayView<arrow::RunEndEncodedArray> view(result.ValueOrDie());
        expect_all_values(view, {10, 10, 10});
    }

    TEST(RunEndEncodedTest, RejectsMismatchedLengths)
    {
        const std::vector<std::uint64_t> values{10, 10};
        const std::vector<std::uint64_t> mismatched{1};
        EXPECT_FALSE(make_run_end(values, mismatched).ok());
    }

    TEST(RunEndEncodedTest, RejectsZeroCount)
    {
        const std::vector<std::uint64_t> values{10, 10};
        const std::vector<std::uint64_t> zero_count{1, 0};
        EXPECT_FALSE(make_run_end(values, zero_count).ok());
    }

    TEST(RunEndEncodedTest, RejectsTotalLengthOverflow)
    {
        const std::vector<std::uint64_t> values{10, 10};
        const std::vector<std::uint64_t> excessive{
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()), 1};
        EXPECT_FALSE(make_run_end(values, excessive).ok());
    }

} // namespace
} // namespace exasol::udf::v2::protocol::arrow_types::correlation
