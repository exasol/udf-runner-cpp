#include <exasol/udf/v2/protocol/arrow_types/range_run.hpp>
#include <exasol/udf/v2/protocol/arrow_types/run_end_encoded.hpp>

#include <arrow/array/array_primitive.h>
#include <arrow/array/array_run_end.h>
#include <arrow/array/builder_primitive.h>
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
    // RangeRun arrays.
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

    TEST(RunArrayViewRunEndEncodedTest, IteratesSlicedRuns)
    {
        auto encoded = make_run_end(std::vector<std::uint64_t>{10, 20, 30},
                                    std::vector<std::uint64_t>{2, 3, 2});
        ASSERT_TRUE(encoded.ok()) << encoded.status();
        auto sliced =
            std::static_pointer_cast<arrow::RunEndEncodedArray>(encoded.ValueOrDie()->Slice(1, 5));
        RunArrayView<arrow::RunEndEncodedArray> view(sliced);
        expect_all_values(view, {10, 20, 20, 20, 30});
    }

    TEST(RunArrayViewRunEndEncodedTest, PostfixIncrementPreservesPreviousPosition)
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

    TEST(RunArrayViewRunEndEncodedTest, RejectsEndIteratorOperations)
    {
        auto encoded = make_run_end(std::vector<std::uint64_t>{10}, std::vector<std::uint64_t>{1});
        ASSERT_TRUE(encoded.ok()) << encoded.status();
        RunArrayView<arrow::RunEndEncodedArray> view(encoded.ValueOrDie());
        auto end = view.end();
        EXPECT_THROW(static_cast<void>(*end), std::out_of_range);
        EXPECT_THROW(++end, std::out_of_range);
    }

    TEST(RunArrayViewRunEndEncodedTest, RejectsDefaultIteratorOperations)
    {
        RunArrayView<arrow::RunEndEncodedArray>::Iterator default_iterator;
        EXPECT_THROW(static_cast<void>(*default_iterator), std::out_of_range);
        EXPECT_THROW(++default_iterator, std::out_of_range);
    }

    TEST(RunArrayViewRunEndEncodedTest, DistinguishesIteratorsFromDifferentArrays)
    {
        auto encoded = make_run_end(std::vector<std::uint64_t>{10}, std::vector<std::uint64_t>{1});
        ASSERT_TRUE(encoded.ok()) << encoded.status();
        RunArrayView<arrow::RunEndEncodedArray> view(encoded.ValueOrDie());
        auto other = make_run_end(std::vector<std::uint64_t>{10}, std::vector<std::uint64_t>{1});
        ASSERT_TRUE(other.ok()) << other.status();
        RunArrayView<arrow::RunEndEncodedArray> other_view(other.ValueOrDie());
        EXPECT_FALSE(view.begin() == other_view.begin());
    }

    TEST(RunArrayViewRunEndEncodedTest, RejectsMalformedRuns)
    {
        const auto malformed = std::make_shared<arrow::RunEndEncodedArray>(
            arrow::run_end_encoded(arrow::int64(), arrow::uint64()), 3, make_int64({2, 2}),
            make_uint64({10, 20}));
        EXPECT_THROW((RunArrayView<arrow::RunEndEncodedArray>(malformed)), std::invalid_argument);
    }

    TEST(RunArrayViewRunEndEncodedTest, RejectsMismatchedStoredRuns)
    {
        const auto malformed = std::make_shared<arrow::RunEndEncodedArray>(
            arrow::run_end_encoded(arrow::int64(), arrow::uint64()), 2, make_int64({2}),
            make_uint64({10, 20}));
        EXPECT_THROW((RunArrayView<arrow::RunEndEncodedArray>(malformed)), std::invalid_argument);
    }

    TEST(RunArrayViewRunEndEncodedTest, RejectsUncoveredLogicalArray)
    {
        const auto malformed = std::make_shared<arrow::RunEndEncodedArray>(
            arrow::run_end_encoded(arrow::int64(), arrow::uint64()), 3, make_int64({2}),
            make_uint64({10}));
        EXPECT_THROW((RunArrayView<arrow::RunEndEncodedArray>(malformed)), std::invalid_argument);
    }

    TEST(RunArrayViewRunEndEncodedTest, RejectsOffsetBeyondRuns)
    {
        const auto malformed = std::make_shared<arrow::RunEndEncodedArray>(
            arrow::run_end_encoded(arrow::int64(), arrow::uint64()), 1, make_int64({2}),
            make_uint64({10}), 3);
        EXPECT_THROW((RunArrayView<arrow::RunEndEncodedArray>(malformed)), std::invalid_argument);
    }

    TEST(RunArrayViewRunEndEncodedTest, RejectsNegativeLogicalOffset)
    {
        const auto malformed = std::make_shared<arrow::RunEndEncodedArray>(
            arrow::run_end_encoded(arrow::int64(), arrow::uint64()), 1, make_int64({2}),
            make_uint64({10}), -1);
        EXPECT_THROW((RunArrayView<arrow::RunEndEncodedArray>(malformed)), std::invalid_argument);
    }

    TEST(RunArrayViewRunEndEncodedTest, RejectsNullRunEnd)
    {
        arrow::Int64Builder builder;
        ASSERT_TRUE(builder.AppendNull().ok());
        std::shared_ptr<arrow::Array> ends;
        ASSERT_TRUE(builder.Finish(&ends).ok());
        const auto malformed = std::make_shared<arrow::RunEndEncodedArray>(
            arrow::run_end_encoded(arrow::int64(), arrow::uint64()), 1, ends, make_uint64({10}));
        EXPECT_THROW((RunArrayView<arrow::RunEndEncodedArray>(malformed)), std::invalid_argument);
    }

    TEST(RunArrayViewRunEndEncodedTest, RejectsWrongValueType)
    {
        auto wrong_values = arrow::RunEndEncodedArray::Make(2, make_int64({2}), make_int64({10}));
        ASSERT_TRUE(wrong_values.ok()) << wrong_values.status();
        EXPECT_THROW((RunArrayView<arrow::RunEndEncodedArray>(wrong_values.ValueOrDie())),
                     std::invalid_argument);
    }

    TEST(RunArrayViewRunEndEncodedTest, RejectsWrongRunEndType)
    {
        auto encoded = arrow::RunEndEncodedArray::Make(2, make_int32({2}), make_uint64({10}));
        ASSERT_TRUE(encoded.ok()) << encoded.status();
        EXPECT_THROW((RunArrayView<arrow::RunEndEncodedArray>(encoded.ValueOrDie())),
                     std::invalid_argument);
    }

    TEST(RunArrayViewRunEndEncodedTest, RejectsNullEncodedValues)
    {
        arrow::UInt64Builder builder;
        ASSERT_TRUE(builder.AppendNull().ok());
        std::shared_ptr<arrow::Array> values;
        ASSERT_TRUE(builder.Finish(&values).ok());
        auto encoded = std::make_shared<arrow::RunEndEncodedArray>(
            arrow::run_end_encoded(arrow::int64(), arrow::uint64()), 1, make_int64({1}), values);
        EXPECT_THROW((RunArrayView<arrow::RunEndEncodedArray>(encoded)), std::invalid_argument);
    }

    TEST(RunArrayViewRunEndEncodedTest, RejectsNullArray)
    {
        EXPECT_THROW((RunArrayView<arrow::RunEndEncodedArray>(nullptr)), std::invalid_argument);
    }

    TEST(RunArrayViewRangeRunTest, PostfixIncrementPreservesPreviousPosition)
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

    TEST(RunArrayViewRangeRunTest, RejectsEndIteratorOperations)
    {
        auto encoded =
            make_range_run(std::vector<std::uint64_t>{10}, std::vector<std::uint64_t>{1});
        ASSERT_TRUE(encoded.ok()) << encoded.status();
        RunArrayView<RangeRunArray> view(encoded.ValueOrDie());
        auto end = view.end();
        EXPECT_THROW(static_cast<void>(*end), std::out_of_range);
        EXPECT_THROW(++end, std::out_of_range);
    }

    TEST(RunArrayViewRangeRunTest, RejectsDefaultIteratorOperations)
    {
        RunArrayView<RangeRunArray>::Iterator default_iterator;
        EXPECT_THROW(static_cast<void>(*default_iterator), std::out_of_range);
        EXPECT_THROW(++default_iterator, std::out_of_range);
    }

    TEST(RunArrayViewRangeRunTest, DistinguishesIteratorsFromDifferentArrays)
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

    TEST(RunArrayViewRangeRunTest, RejectsNullArray)
    {
        EXPECT_THROW((RunArrayView<RangeRunArray>(nullptr)), std::invalid_argument);
    }

    TEST(RunArrayViewRangeRunTest, IteratesSlicedRuns)
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

    TEST(RunArrayViewRangeRunTest, RejectsMalformedRuns)
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

    TEST(RunArrayViewRangeRunTest, RejectsWrongArrayExtension)
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

#ifndef NDEBUG
    TEST(RunArrayViewRangeRunTest, RejectsDecodedValueOverflow)
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

} // namespace
} // namespace exasol::udf::v2::protocol::arrow_types::correlation
