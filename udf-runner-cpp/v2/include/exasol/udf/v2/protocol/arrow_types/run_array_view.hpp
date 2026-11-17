#pragma once

#include <arrow/array/array_run_end.h>

#include <cstdint>
#include <iterator>
#include <memory>

namespace exasol::udf::v2::protocol::arrow_types::correlation
{

class RangeRunArray;

/// A lazy view of the logical uint64 values in an encoded correlation array.
/// Only arrow::RunEndEncodedArray and RangeRunArray are linked instantiations.
template <typename ArrayType>
class RunArrayView final
{
public:
    class Iterator final
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type        = std::uint64_t;
        using difference_type   = std::int64_t;
        using reference         = value_type;
        using pointer           = void;

        Iterator() = default;

        [[nodiscard]] value_type operator*() const;
        Iterator& operator++();
        Iterator operator++(int);
        [[nodiscard]] bool operator==(const Iterator& other) const;

    private:
        friend class RunArrayView;
        Iterator(std::shared_ptr<ArrayType> array, std::int64_t position, std::int64_t run);

        std::shared_ptr<ArrayType> array;
        std::int64_t position = 0;
        std::int64_t run      = 0;
    };

    /// Validates Arrow structure and, in debug builds, every run.
    /// Throws std::invalid_argument when a checked condition fails.
    explicit RunArrayView(std::shared_ptr<ArrayType> array);

    [[nodiscard]] Iterator begin() const;
    [[nodiscard]] Iterator end() const;
    /// Returns an iterator at the logical position; length() denotes end().
    /// Throws std::out_of_range when position is negative or exceeds length().
    [[nodiscard]] Iterator find(std::int64_t position) const;

private:
    std::shared_ptr<ArrayType> array;
};

extern template class RunArrayView<arrow::RunEndEncodedArray>;
extern template class RunArrayView<RangeRunArray>;

} // namespace exasol::udf::v2::protocol::arrow_types::correlation
