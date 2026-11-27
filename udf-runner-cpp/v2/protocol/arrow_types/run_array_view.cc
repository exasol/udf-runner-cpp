#include <exasol/udf/v2/protocol/arrow_types/run_array_view.hpp>

#include <exasol/udf/v2/protocol/arrow_types/range_run.hpp>

#include <arrow/array/array_primitive.h>

#include <cstdint>
#include <limits>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace exasol::udf::v2::protocol::arrow_types::correlation
{
namespace
{
    template <typename ArrayType>
    const arrow::RunEndEncodedArray& storage(const ArrayType& array)
    {
        if constexpr (std::is_same_v<ArrayType, RangeRunArray>)
        {
            return static_cast<const arrow::RunEndEncodedArray&>(*array.storage());
        }
        else
        {
            return array;
        }
    }

    template <typename ArrayType>
    void validate_all_runs(const arrow::Int64Array& ends, const arrow::UInt64Array& values)
    {
        std::int64_t previous_end = 0;
        for (std::int64_t run = 0; run < ends.length(); ++run)
        {
            const auto end = ends.Value(run);
            if (end <= previous_end)
            {
                throw std::invalid_argument("run ends must increase");
            }
            if constexpr (std::is_same_v<ArrayType, RangeRunArray>)
            {
                const auto count = static_cast<std::uint64_t>(end - previous_end);
                if (values.Value(run) > std::numeric_limits<std::uint64_t>::max() - (count - 1))
                {
                    throw std::invalid_argument("range_run value overflow");
                }
            }
            previous_end = end;
        }
    }

    template <typename ArrayType>
    void validate(const ArrayType& array)
    {
        if constexpr (std::is_same_v<ArrayType, RangeRunArray>)
        {
            if (array.extension_type()->extension_name() != "exasol.udf.range_run" ||
                array.storage()->type_id() != arrow::Type::RUN_END_ENCODED)
            {
                throw std::invalid_argument("expected exasol.udf.range_run storage");
            }
        }

        const auto& encoded = storage(array);
        if (encoded.type_id() != arrow::Type::RUN_END_ENCODED ||
            encoded.run_ends()->type_id() != arrow::Type::INT64 ||
            encoded.values()->type_id() != arrow::Type::UINT64)
        {
            throw std::invalid_argument("expected RunEndEncoded<int64, uint64> storage");
        }
        const auto& ends   = static_cast<const arrow::Int64Array&>(*encoded.run_ends());
        const auto& values = static_cast<const arrow::UInt64Array&>(*encoded.values());
        if (ends.length() != values.length() || ends.null_count() != 0 || values.null_count() != 0)
        {
            throw std::invalid_argument("invalid encoded runs");
        }
#ifndef NDEBUG
        validate_all_runs<ArrayType>(ends, values);
#endif
        const auto previous_end = ends.length() == 0 ? 0 : ends.Value(ends.length() - 1);
        if (array.offset() < 0 || array.length() < 0 || array.offset() > previous_end ||
            array.length() > previous_end - array.offset())
        {
            throw std::invalid_argument("run ends do not cover the logical array");
        }
        const auto status = array.ValidateFull();
        if (!status.ok())
        {
            throw std::invalid_argument(status.ToString());
        }
    }

    template <typename ArrayType>
    std::int64_t find_run(const ArrayType& array, std::int64_t position)
    {
        const auto& ends = static_cast<const arrow::Int64Array&>(*storage(array).run_ends());
        const auto absolute_position = array.offset() + position;
        std::int64_t low             = 0;
        std::int64_t high            = ends.length();
        while (low < high)
        {
            const auto middle = std::midpoint(low, high);
            if (ends.Value(middle) <= absolute_position)
            {
                low = middle + 1;
            }
            else
            {
                high = middle;
            }
        }
        return low;
    }
} // namespace

template <typename ArrayType>
RunArrayView<ArrayType>::Iterator::Iterator(std::shared_ptr<ArrayType> array,
                                            std::int64_t position,
                                            std::int64_t run)
    : array(std::move(array)), position(position), run(run)
{
}

template <typename ArrayType>
std::uint64_t RunArrayView<ArrayType>::Iterator::operator*() const
{
    if (this->array == nullptr || this->position >= this->array->length())
    {
        throw std::out_of_range("cannot dereference the end iterator");
    }
    const auto& encoded = storage(*this->array);
    const auto& values  = static_cast<const arrow::UInt64Array&>(*encoded.values());
    const auto value    = values.Value(this->run);
    if constexpr (std::is_same_v<ArrayType, RangeRunArray>)
    {
        const auto& ends        = static_cast<const arrow::Int64Array&>(*encoded.run_ends());
        const auto previous_end = this->run == 0 ? 0 : ends.Value(this->run - 1);
        return value +
               static_cast<std::uint64_t>(this->array->offset() + this->position - previous_end);
    }
    return value;
}

template <typename ArrayType>
typename RunArrayView<ArrayType>::Iterator& RunArrayView<ArrayType>::Iterator::operator++()
{
    if (this->array == nullptr || this->position >= this->array->length())
    {
        throw std::out_of_range("cannot advance the end iterator");
    }
    ++this->position;
    if (this->position < this->array->length())
    {
        const auto& ends = static_cast<const arrow::Int64Array&>(*storage(*this->array).run_ends());
        if (ends.Value(this->run) <= this->array->offset() + this->position)
        {
            ++this->run;
        }
    }
    return *this;
}

template <typename ArrayType>
typename RunArrayView<ArrayType>::Iterator RunArrayView<ArrayType>::Iterator::operator++(int)
{
    auto previous = *this;
    ++*this;
    return previous;
}

template <typename ArrayType>
bool RunArrayView<ArrayType>::Iterator::operator==(const Iterator& other) const
{
    return this->array == other.array && this->position == other.position;
}

template <typename ArrayType>
RunArrayView<ArrayType>::RunArrayView(std::shared_ptr<ArrayType> array) : array(std::move(array))
{
    if (this->array == nullptr)
    {
        throw std::invalid_argument("encoded array must not be null");
    }
    validate(*this->array);
}

template <typename ArrayType>
typename RunArrayView<ArrayType>::Iterator RunArrayView<ArrayType>::begin() const
{
    return find(0);
}

template <typename ArrayType>
typename RunArrayView<ArrayType>::Iterator RunArrayView<ArrayType>::end() const
{
    return Iterator(this->array, this->array->length(), storage(*this->array).run_ends()->length());
}

template <typename ArrayType>
typename RunArrayView<ArrayType>::Iterator RunArrayView<ArrayType>::find(
    std::int64_t position) const
{
    if (position < 0 || position > this->array->length())
    {
        throw std::out_of_range("logical position is outside the array");
    }
    if (position == this->array->length())
    {
        return end();
    }
    return Iterator(this->array, position, find_run(*this->array, position));
}

template class RunArrayView<arrow::RunEndEncodedArray>;
template class RunArrayView<RangeRunArray>;

} // namespace exasol::udf::v2::protocol::arrow_types::correlation
