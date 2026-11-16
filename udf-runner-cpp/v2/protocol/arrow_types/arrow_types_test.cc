#include <exasol/udf/v2/protocol/arrow_types/range_run.hpp>
#include <exasol/udf/v2/protocol/arrow_types/run_end_encoded.hpp>

#include <arrow/array/array_primitive.h>
#include <arrow/array/array_run_end.h>
#include <arrow/array/builder_primitive.h>
#include <arrow/c/bridge.h>
#include <arrow/type.h>
#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace exasol::udf::v2::protocol::arrow_types::correlation
{
namespace
{

    std::shared_ptr<arrow::Array> make_uint64(const std::vector<std::uint64_t>& values)
    {
        arrow::UInt64Builder builder;
        EXPECT_TRUE(builder.AppendValues(values).ok());
        std::shared_ptr<arrow::Array> array;
        EXPECT_TRUE(builder.Finish(&array).ok());
        return array;
    }

    std::shared_ptr<arrow::Array> make_int64(const std::vector<std::int64_t>& values)
    {
        arrow::Int64Builder builder;
        EXPECT_TRUE(builder.AppendValues(values).ok());
        std::shared_ptr<arrow::Array> array;
        EXPECT_TRUE(builder.Finish(&array).ok());
        return array;
    }

    std::shared_ptr<arrow::Array> make_int32(const std::vector<std::int32_t>& values)
    {
        arrow::Int32Builder builder;
        EXPECT_TRUE(builder.AppendValues(values).ok());
        std::shared_ptr<arrow::Array> array;
        EXPECT_TRUE(builder.Finish(&array).ok());
        return array;
    }

    // Supplies a configurable name and storage type to test rejection of incompatible
    // RangeRun arrays and conflicting extension registrations.
    class TestRangeRunType final : public arrow::ExtensionType
    {
    public:
        TestRangeRunType(std::string name, std::shared_ptr<arrow::DataType> storage_type)
            : arrow::ExtensionType(std::move(storage_type)), name(std::move(name))
        {
        }

        std::string extension_name() const override
        {
            return this->name;
        }

        bool ExtensionEquals(const arrow::ExtensionType& other) const override
        {
            return this->name == other.extension_name() &&
                   storage_type()->Equals(other.storage_type());
        }

        std::shared_ptr<arrow::Array> MakeArray(
            std::shared_ptr<arrow::ArrayData> data) const override
        {
            return std::make_shared<RangeRunArray>(std::move(data));
        }

        arrow::Result<std::shared_ptr<arrow::DataType>> Deserialize(
            [[maybe_unused]] std::shared_ptr<arrow::DataType> storage_type,
            [[maybe_unused]] const std::string& serialized_data) const override
        {
            return arrow::Status::NotImplemented("test extension type cannot be deserialized");
        }

        std::string Serialize() const override
        {
            return {};
        }

    private:
        std::string name;
    };

    template <typename ArrayType>
    void expect_all_values(const RunArrayView<ArrayType>& view,
                           const std::vector<std::uint64_t>& expected)
    {
        EXPECT_EQ((std::vector<std::uint64_t>(view.begin(), view.end())), expected);
        for (std::size_t position = 0; position < expected.size(); ++position)
        {
            SCOPED_TRACE(position);
            EXPECT_EQ(*view.find(static_cast<std::int64_t>(position)), expected[position]);
        }
        EXPECT_EQ(view.find(static_cast<std::int64_t>(expected.size())), view.end());
    }

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

    TEST(RunEndEncodedTest, IteratesSlicedRuns)
    {
        auto encoded = make_run_end(std::vector<std::uint64_t>{10, 20, 30},
                                    std::vector<std::uint64_t>{2, 3, 2});
        ASSERT_TRUE(encoded.ok()) << encoded.status();
        auto sliced =
            std::static_pointer_cast<arrow::RunEndEncodedArray>(encoded.ValueOrDie()->Slice(1, 5));
        RunArrayView<arrow::RunEndEncodedArray> view(sliced);
        expect_all_values(view, {10, 20, 20, 20, 30});
    }

    TEST(RunEndEncodedTest, PostfixIncrementPreservesPreviousPosition)
    {
        auto encoded =
            make_run_end(std::vector<std::uint64_t>{10, 20}, std::vector<std::uint64_t>{2, 1});
        ASSERT_TRUE(encoded.ok()) << encoded.status();
        RunArrayView<arrow::RunEndEncodedArray> view(encoded.ValueOrDie());
        auto iterator = view.begin();
        auto previous = iterator++;
        EXPECT_EQ(*previous, 10);
        EXPECT_EQ(*iterator, 10);
        previous = iterator++;
        EXPECT_EQ(*previous, 10);
        EXPECT_EQ(*iterator, 20);
    }

    TEST(RunEndEncodedTest, RejectsEndIteratorOperations)
    {
        auto encoded = make_run_end(std::vector<std::uint64_t>{10}, std::vector<std::uint64_t>{1});
        ASSERT_TRUE(encoded.ok()) << encoded.status();
        RunArrayView<arrow::RunEndEncodedArray> view(encoded.ValueOrDie());
        auto end = view.end();
        EXPECT_THROW(static_cast<void>(*end), std::out_of_range);
        EXPECT_THROW(++end, std::out_of_range);
    }

    TEST(RunEndEncodedTest, RejectsDefaultIteratorOperations)
    {
        RunArrayView<arrow::RunEndEncodedArray>::Iterator default_iterator;
        EXPECT_THROW(static_cast<void>(*default_iterator), std::out_of_range);
        EXPECT_THROW(++default_iterator, std::out_of_range);
    }

    TEST(RunEndEncodedTest, DistinguishesIteratorsFromDifferentArrays)
    {
        auto encoded = make_run_end(std::vector<std::uint64_t>{10}, std::vector<std::uint64_t>{1});
        ASSERT_TRUE(encoded.ok()) << encoded.status();
        RunArrayView<arrow::RunEndEncodedArray> view(encoded.ValueOrDie());
        auto other = make_run_end(std::vector<std::uint64_t>{10}, std::vector<std::uint64_t>{1});
        ASSERT_TRUE(other.ok()) << other.status();
        RunArrayView<arrow::RunEndEncodedArray> other_view(other.ValueOrDie());
        EXPECT_FALSE(view.begin() == other_view.begin());
    }

    TEST(RunEndEncodedTest, RejectsMalformedRuns)
    {
        const auto malformed = std::make_shared<arrow::RunEndEncodedArray>(
            arrow::run_end_encoded(arrow::int64(), arrow::uint64()), 3, make_int64({2, 2}),
            make_uint64({10, 20}));
        EXPECT_THROW((RunArrayView<arrow::RunEndEncodedArray>(malformed)), std::invalid_argument);
    }

    TEST(RunEndEncodedTest, RejectsMismatchedStoredRuns)
    {
        const auto malformed = std::make_shared<arrow::RunEndEncodedArray>(
            arrow::run_end_encoded(arrow::int64(), arrow::uint64()), 2, make_int64({2}),
            make_uint64({10, 20}));
        EXPECT_THROW((RunArrayView<arrow::RunEndEncodedArray>(malformed)), std::invalid_argument);
    }

    TEST(RunEndEncodedTest, RejectsUncoveredLogicalArray)
    {
        const auto malformed = std::make_shared<arrow::RunEndEncodedArray>(
            arrow::run_end_encoded(arrow::int64(), arrow::uint64()), 3, make_int64({2}),
            make_uint64({10}));
        EXPECT_THROW((RunArrayView<arrow::RunEndEncodedArray>(malformed)), std::invalid_argument);
    }

    TEST(RunEndEncodedTest, RejectsOffsetBeyondRuns)
    {
        const auto malformed = std::make_shared<arrow::RunEndEncodedArray>(
            arrow::run_end_encoded(arrow::int64(), arrow::uint64()), 1, make_int64({2}),
            make_uint64({10}), 3);
        EXPECT_THROW((RunArrayView<arrow::RunEndEncodedArray>(malformed)), std::invalid_argument);
    }

    TEST(RunEndEncodedTest, RejectsNegativeLogicalOffset)
    {
        const auto malformed = std::make_shared<arrow::RunEndEncodedArray>(
            arrow::run_end_encoded(arrow::int64(), arrow::uint64()), 1, make_int64({2}),
            make_uint64({10}), -1);
        EXPECT_THROW((RunArrayView<arrow::RunEndEncodedArray>(malformed)), std::invalid_argument);
    }

    TEST(RunEndEncodedTest, RejectsNullRunEnd)
    {
        arrow::Int64Builder builder;
        ASSERT_TRUE(builder.AppendNull().ok());
        std::shared_ptr<arrow::Array> ends;
        ASSERT_TRUE(builder.Finish(&ends).ok());
        const auto malformed = std::make_shared<arrow::RunEndEncodedArray>(
            arrow::run_end_encoded(arrow::int64(), arrow::uint64()), 1, ends, make_uint64({10}));
        EXPECT_THROW((RunArrayView<arrow::RunEndEncodedArray>(malformed)), std::invalid_argument);
    }

    TEST(RunEndEncodedTest, RejectsWrongValueType)
    {
        auto wrong_values = arrow::RunEndEncodedArray::Make(2, make_int64({2}), make_int64({10}));
        ASSERT_TRUE(wrong_values.ok()) << wrong_values.status();
        EXPECT_THROW((RunArrayView<arrow::RunEndEncodedArray>(wrong_values.ValueOrDie())),
                     std::invalid_argument);
    }

    TEST(RunEndEncodedTest, RejectsWrongRunEndType)
    {
        auto encoded = arrow::RunEndEncodedArray::Make(2, make_int32({2}), make_uint64({10}));
        ASSERT_TRUE(encoded.ok()) << encoded.status();
        EXPECT_THROW((RunArrayView<arrow::RunEndEncodedArray>(encoded.ValueOrDie())),
                     std::invalid_argument);
    }

    TEST(RunEndEncodedTest, RejectsNullEncodedValues)
    {
        arrow::UInt64Builder builder;
        ASSERT_TRUE(builder.AppendNull().ok());
        std::shared_ptr<arrow::Array> values;
        ASSERT_TRUE(builder.Finish(&values).ok());
        auto encoded = std::make_shared<arrow::RunEndEncodedArray>(
            arrow::run_end_encoded(arrow::int64(), arrow::uint64()), 1, make_int64({1}), values);
        EXPECT_THROW((RunArrayView<arrow::RunEndEncodedArray>(encoded)), std::invalid_argument);
    }

    TEST(RunEndEncodedTest, RejectsNullArray)
    {
        EXPECT_THROW((RunArrayView<arrow::RunEndEncodedArray>(nullptr)), std::invalid_argument);
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

    TEST(RangeRunTest, PreservesExplicitBoundaries)
    {
        const std::vector<std::uint64_t> ids{101, 102, 103, 104};
        const std::vector<std::uint64_t> run_values{101, 103};
        const std::vector<std::uint64_t> counts{2, 2};
        auto result = make_range_run(run_values, counts);
        ASSERT_TRUE(result.ok()) << result.status();
        const auto& encoded = result.ValueOrDie();
        const auto storage =
            std::static_pointer_cast<arrow::RunEndEncodedArray>(encoded->storage());
        const auto ends   = std::static_pointer_cast<arrow::Int64Array>(storage->run_ends());
        const auto values = std::static_pointer_cast<arrow::UInt64Array>(storage->values());
        ASSERT_EQ(ends->length(), 2);
        EXPECT_EQ(ends->Value(0), 2);
        EXPECT_EQ(ends->Value(1), 4);
        EXPECT_EQ(values->Value(0), 101);
        EXPECT_EQ(values->Value(1), 103);

        RunArrayView<RangeRunArray> view(encoded);
        expect_all_values(view, ids);
        EXPECT_THROW(static_cast<void>(view.find(-1)), std::out_of_range);
        EXPECT_THROW(static_cast<void>(view.find(5)), std::out_of_range);
    }

    TEST(RangeRunTest, MakesArrayWithInferredStorageType)
    {
        auto array = RangeRunType::Make(5, make_int64({3, 5}), make_uint64({10, 20}));
        ASSERT_TRUE(array.ok()) << array.status();
        RunArrayView<RangeRunArray> view(array.ValueOrDie());
        expect_all_values(view, {10, 11, 12, 20, 21});
    }

    TEST(RangeRunTest, MakesArrayWithExplicitStorageTypeAndOffset)
    {
        RangeRunType type;
        auto array = RangeRunType::Make(type.storage_type(), 4, make_int64({3, 5}),
                                        make_uint64({10, 20}), 1);
        ASSERT_TRUE(array.ok()) << array.status();
        RunArrayView<RangeRunArray> view(array.ValueOrDie());
        expect_all_values(view, {11, 12, 20, 21});
    }

    TEST(RangeRunTest, MakesEmptyArray)
    {
        auto array = RangeRunType::Make(0, make_int64({}), make_uint64({}));
        ASSERT_TRUE(array.ok()) << array.status();
        RunArrayView<RangeRunArray> view(array.ValueOrDie());
        expect_all_values(view, {});
    }

    TEST(RangeRunTest, MakeRejectsWrongStorageType)
    {
        RangeRunType type;
        EXPECT_FALSE(
            RangeRunType::Make(arrow::uint64(), 1, make_int64({1}), make_uint64({10})).ok());
        EXPECT_FALSE(RangeRunType::Make(arrow::run_end_encoded(arrow::int32(), arrow::uint64()), 1,
                                        make_int32({1}), make_uint64({10}))
                         .ok());
        EXPECT_FALSE(RangeRunType::Make(arrow::run_end_encoded(arrow::int64(), arrow::int64()), 1,
                                        make_int64({1}), make_int64({10}))
                         .ok());
        EXPECT_FALSE(RangeRunType::Make(1, make_int32({1}), make_uint64({10})).ok());
        EXPECT_FALSE(RangeRunType::Make(1, make_int64({1}), make_int64({10})).ok());
        EXPECT_FALSE(
            RangeRunType::Make(type.storage_type(), 1, make_int32({1}), make_uint64({10})).ok());
    }

    TEST(RangeRunTest, MakeRejectsInvalidChildren)
    {
        const auto ends   = make_int64({2});
        const auto values = make_uint64({10});
        EXPECT_FALSE(RangeRunType::Make(2, nullptr, values).ok());
        EXPECT_FALSE(RangeRunType::Make(2, ends, nullptr).ok());
        EXPECT_FALSE(RangeRunType::Make(nullptr, 2, ends, values).ok());
        RangeRunType type;
        EXPECT_FALSE(RangeRunType::Make(type.storage_type(), 2, nullptr, values).ok());
        EXPECT_FALSE(RangeRunType::Make(type.storage_type(), 2, ends, nullptr).ok());
        EXPECT_FALSE(RangeRunType::Make(2, make_int64({2, 2}), make_uint64({10, 20})).ok());
        EXPECT_FALSE(
            RangeRunType::Make(2, ends, make_uint64({std::numeric_limits<std::uint64_t>::max()}))
                .ok());

        arrow::UInt64Builder builder;
        ASSERT_TRUE(builder.AppendNull().ok());
        std::shared_ptr<arrow::Array> null_value;
        ASSERT_TRUE(builder.Finish(&null_value).ok());
        EXPECT_FALSE(RangeRunType::Make(1, make_int64({1}), null_value).ok());

        arrow::Int64Builder ends_builder;
        ASSERT_TRUE(ends_builder.AppendNull().ok());
        std::shared_ptr<arrow::Array> null_end;
        ASSERT_TRUE(ends_builder.Finish(&null_end).ok());
        EXPECT_FALSE(RangeRunType::Make(1, null_end, make_uint64({10})).ok());
    }

    TEST(RangeRunTest, ExpandsOverlappingConsecutiveRanges)
    {
        const std::vector<std::uint64_t> values{1, 3};
        const std::vector<std::uint64_t> counts{3, 4};
        auto result = make_range_run(values, counts);
        ASSERT_TRUE(result.ok()) << result.status();
        const auto storage =
            std::static_pointer_cast<arrow::RunEndEncodedArray>(result.ValueOrDie()->storage());
        const auto ends = std::static_pointer_cast<arrow::Int64Array>(storage->run_ends());
        const auto encoded_values = std::static_pointer_cast<arrow::UInt64Array>(storage->values());
        ASSERT_EQ(ends->length(), 2);
        ASSERT_EQ(encoded_values->length(), 2);
        EXPECT_EQ(ends->Value(0), 3);
        EXPECT_EQ(ends->Value(1), 7);
        EXPECT_EQ(encoded_values->Value(0), 1);
        EXPECT_EQ(encoded_values->Value(1), 3);
        RunArrayView<RangeRunArray> view(result.ValueOrDie());
        expect_all_values(view, {1, 2, 3, 3, 4, 5, 6});
    }

    TEST(RangeRunTest, RejectsMismatchedLengths)
    {
        const std::vector<std::uint64_t> ids{1, 2, 3};
        const std::vector<std::uint64_t> mismatched{1, 1};
        EXPECT_FALSE(make_range_run(ids, mismatched).ok());
    }

    TEST(RangeRunTest, RejectsZeroCount)
    {
        const std::vector<std::uint64_t> ids{1, 2, 3};
        const std::vector<std::uint64_t> zero_count{1, 1, 0};
        EXPECT_FALSE(make_range_run(ids, zero_count).ok());
    }

    TEST(RangeRunTest, RejectsTotalLengthOverflow)
    {
        const std::vector<std::uint64_t> ids{1, 2, 3};
        const std::vector<std::uint64_t> excessive{
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()), 1, 1};
        EXPECT_FALSE(make_range_run(ids, excessive).ok());
    }

    TEST(RangeRunTest, RejectsValueOverflow)
    {
        const std::vector max_value{std::numeric_limits<std::uint64_t>::max()};
        const std::vector two{std::uint64_t{2}};
        EXPECT_FALSE(make_range_run(max_value, two).ok());
    }

    TEST(RangeRunTest, SupportsEmptyRuns)
    {
        const std::vector<std::uint64_t> no_values;
        auto empty = make_range_run(no_values, no_values);
        ASSERT_TRUE(empty.ok()) << empty.status();
        const auto empty_storage =
            std::static_pointer_cast<arrow::RunEndEncodedArray>(empty.ValueOrDie()->storage());
        EXPECT_EQ(empty_storage->run_ends()->length(), 0);
        EXPECT_EQ(empty_storage->values()->length(), 0);
        RunArrayView<RangeRunArray> view(empty.ValueOrDie());
        expect_all_values(view, no_values);
    }

    TEST(RangeRunTest, SupportsSingleRange)
    {
        const std::vector<std::uint64_t> ids{42, 43, 44};
        const std::vector<std::uint64_t> start{42};
        const std::vector<std::uint64_t> count{3};
        auto encoded = make_range_run(start, count);
        ASSERT_TRUE(encoded.ok()) << encoded.status();
        const auto storage =
            std::static_pointer_cast<arrow::RunEndEncodedArray>(encoded.ValueOrDie()->storage());
        const auto ends   = std::static_pointer_cast<arrow::Int64Array>(storage->run_ends());
        const auto values = std::static_pointer_cast<arrow::UInt64Array>(storage->values());
        ASSERT_EQ(ends->length(), 1);
        ASSERT_EQ(values->length(), 1);
        EXPECT_EQ(ends->Value(0), 3);
        EXPECT_EQ(values->Value(0), 42);
        RunArrayView<RangeRunArray> view(encoded.ValueOrDie());
        expect_all_values(view, ids);
    }

    TEST(RangeRunTest, PostfixIncrementPreservesPreviousPosition)
    {
        auto encoded =
            make_range_run(std::vector<std::uint64_t>{10, 20}, std::vector<std::uint64_t>{2, 1});
        ASSERT_TRUE(encoded.ok()) << encoded.status();
        RunArrayView<RangeRunArray> view(encoded.ValueOrDie());
        auto iterator = view.begin();
        auto previous = iterator++;
        EXPECT_EQ(*previous, 10);
        EXPECT_EQ(*iterator, 11);
        previous = iterator++;
        EXPECT_EQ(*previous, 11);
        EXPECT_EQ(*iterator, 20);
    }

    TEST(RangeRunTest, RejectsEndIteratorOperations)
    {
        auto encoded =
            make_range_run(std::vector<std::uint64_t>{10}, std::vector<std::uint64_t>{1});
        ASSERT_TRUE(encoded.ok()) << encoded.status();
        RunArrayView<RangeRunArray> view(encoded.ValueOrDie());
        auto end = view.end();
        EXPECT_THROW(static_cast<void>(*end), std::out_of_range);
        EXPECT_THROW(++end, std::out_of_range);
    }

    TEST(RangeRunTest, RejectsDefaultIteratorOperations)
    {
        RunArrayView<RangeRunArray>::Iterator default_iterator;
        EXPECT_THROW(static_cast<void>(*default_iterator), std::out_of_range);
        EXPECT_THROW(++default_iterator, std::out_of_range);
    }

    TEST(RangeRunTest, DistinguishesIteratorsFromDifferentArrays)
    {
        auto encoded =
            make_range_run(std::vector<std::uint64_t>{10}, std::vector<std::uint64_t>{1});
        ASSERT_TRUE(encoded.ok()) << encoded.status();
        RunArrayView<RangeRunArray> view(encoded.ValueOrDie());
        auto other = make_range_run(std::vector<std::uint64_t>{10}, std::vector<std::uint64_t>{1});
        ASSERT_TRUE(other.ok()) << other.status();
        RunArrayView<RangeRunArray> other_view(other.ValueOrDie());
        EXPECT_FALSE(view.begin() == other_view.begin());
    }

    TEST(RangeRunTest, RejectsNullArray)
    {
        EXPECT_THROW((RunArrayView<RangeRunArray>(nullptr)), std::invalid_argument);
    }

    TEST(RangeRunTest, IteratesSlicedRuns)
    {
        auto encoded =
            make_range_run(std::vector<std::uint64_t>{42, 100}, std::vector<std::uint64_t>{3, 3});
        ASSERT_TRUE(encoded.ok()) << encoded.status();
        const auto& array = encoded.ValueOrDie();
        RunArrayView<RangeRunArray> within_run(
            std::static_pointer_cast<RangeRunArray>(array->Slice(1, 2)));
        expect_all_values(within_run, {43, 44});

        RunArrayView<RangeRunArray> across_runs(
            std::static_pointer_cast<RangeRunArray>(array->Slice(2, 3)));
        expect_all_values(across_runs, {44, 100, 101});

        RunArrayView<RangeRunArray> from_zero(
            std::static_pointer_cast<RangeRunArray>(array->Slice(0, 2)));
        expect_all_values(from_zero, {42, 43});

        RunArrayView<RangeRunArray> empty(
            std::static_pointer_cast<RangeRunArray>(array->Slice(3, 0)));
        expect_all_values(empty, {});
    }

    TEST(RangeRunTest, RejectsMalformedRuns)
    {
        const auto malformed = std::make_shared<arrow::RunEndEncodedArray>(
            arrow::run_end_encoded(arrow::int64(), arrow::uint64()), 3, make_int64({2, 2}),
            make_uint64({10, 20}));
        const auto encoded =
            arrow::ExtensionType::WrapArray(std::make_shared<RangeRunType>(), malformed);
        EXPECT_THROW(
            (RunArrayView<RangeRunArray>(std::static_pointer_cast<RangeRunArray>(encoded))),
            std::invalid_argument);
    }

    TEST(RangeRunTest, RejectsWrongExtensionStorage)
    {
        RangeRunType type;
        EXPECT_FALSE(type.Deserialize(arrow::uint64(), "").ok());
        EXPECT_FALSE(
            type.Deserialize(arrow::run_end_encoded(arrow::int32(), arrow::uint64()), "").ok());
        EXPECT_FALSE(
            type.Deserialize(arrow::run_end_encoded(arrow::int64(), arrow::int64()), "").ok());
    }

    TEST(RangeRunTest, ExtensionEqualityChecksNameAndStorage)
    {
        RangeRunType type;
        EXPECT_TRUE(type.ExtensionEquals(RangeRunType{}));
        EXPECT_FALSE(type.ExtensionEquals(TestRangeRunType("other", type.storage_type())));
        EXPECT_FALSE(
            type.ExtensionEquals(TestRangeRunType("exasol.udf.range_run", arrow::uint64())));
    }

    TEST(RangeRunTest, RejectsWrongArrayExtension)
    {
        auto storage = make_run_end(std::vector<std::uint64_t>{10}, std::vector<std::uint64_t>{1});
        ASSERT_TRUE(storage.ok()) << storage.status();
        auto wrong_name = arrow::ExtensionType::WrapArray(
            std::make_shared<TestRangeRunType>("other", storage.ValueOrDie()->type()),
            storage.ValueOrDie());
        EXPECT_THROW(
            (RunArrayView<RangeRunArray>(std::static_pointer_cast<RangeRunArray>(wrong_name))),
            std::invalid_argument);

        auto wrong_storage = arrow::ExtensionType::WrapArray(
            std::make_shared<TestRangeRunType>("exasol.udf.range_run", arrow::uint64()),
            make_uint64({10}));
        EXPECT_THROW(
            (RunArrayView<RangeRunArray>(std::static_pointer_cast<RangeRunArray>(wrong_storage))),
            std::invalid_argument);
    }

    TEST(RangeRunTest, RejectsUnexpectedExtensionMetadata)
    {
        RangeRunType type;
        EXPECT_FALSE(type.Deserialize(type.storage_type(), "unexpected").ok());
    }

#ifndef NDEBUG
    TEST(RangeRunTest, RejectsDecodedValueOverflow)
    {
        auto storage = arrow::RunEndEncodedArray::Make(
            2, make_int64({2}), make_uint64({std::numeric_limits<std::uint64_t>::max()}));
        ASSERT_TRUE(storage.ok()) << storage.status();
        const auto encoded =
            arrow::ExtensionType::WrapArray(std::make_shared<RangeRunType>(), storage.ValueOrDie());
        EXPECT_THROW(
            (RunArrayView<RangeRunArray>(std::static_pointer_cast<RangeRunArray>(encoded))),
            std::invalid_argument);
    }
#endif

    TEST(RangeRunTest, RegistersExtensionIdempotently)
    {
        ASSERT_TRUE(register_range_run_type().ok());
        EXPECT_TRUE(register_range_run_type().ok());
    }

    TEST(RangeRunTest, RoundTripsThroughArrowCData)
    {
        ASSERT_TRUE(register_range_run_type().ok());
        const std::vector<std::uint64_t> ids{1, 2, 3, 7, 8};
        const std::vector<std::uint64_t> starts{1, 7};
        const std::vector<std::uint64_t> counts{3, 2};
        auto encoded = make_range_run(starts, counts);
        ASSERT_TRUE(encoded.ok()) << encoded.status();

        ArrowArray c_array{};
        ArrowSchema c_schema{};
        ASSERT_TRUE(arrow::ExportArray(*encoded.ValueOrDie(), &c_array, &c_schema).ok());
        // Arrow 25 exports one unused parent buffer for REE, while its importer
        // requires the C Data layout's zero parent buffers.
        ASSERT_EQ(c_array.n_buffers, 1);
        ASSERT_EQ(c_array.buffers[0], nullptr);
        c_array.n_buffers = 0;
        auto imported     = arrow::ImportArray(&c_array, &c_schema);
        ASSERT_TRUE(imported.ok()) << imported.status();
        const auto& array = imported.ValueOrDie();
        ASSERT_EQ(array->type_id(), arrow::Type::EXTENSION);
        const auto& extension = dynamic_cast<const arrow::ExtensionArray&>(*array);
        EXPECT_EQ(extension.extension_type()->extension_name(), "exasol.udf.range_run");
        EXPECT_EQ(extension.extension_type()->storage_type()->id(), arrow::Type::RUN_END_ENCODED);
        RunArrayView<RangeRunArray> view(std::static_pointer_cast<RangeRunArray>(array));
        expect_all_values(view, ids);
    }

    TEST(RangeRunTest, RejectsConflictingRegistration)
    {
        constexpr auto* name = "exasol.udf.range_run";
        const auto original  = arrow::GetExtensionType(name);
        if (original != nullptr)
        {
            ASSERT_TRUE(arrow::UnregisterExtensionType(name).ok());
        }
        ASSERT_TRUE(
            arrow::RegisterExtensionType(std::make_shared<TestRangeRunType>(name, arrow::uint64()))
                .ok());
        EXPECT_FALSE(register_range_run_type().ok());
        ASSERT_TRUE(arrow::UnregisterExtensionType(name).ok());
        if (original != nullptr)
        {
            ASSERT_TRUE(arrow::RegisterExtensionType(original).ok());
        }
    }

} // namespace
} // namespace exasol::udf::v2::protocol::arrow_types::correlation
