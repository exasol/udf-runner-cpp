#pragma once

#include <arrow/extension_type.h>
#include <arrow/result.h>

#include <exasol/udf/v2/protocol/arrow_types/run_array_view.hpp>

#include <cstdint>
#include <memory>
#include <span>
#include <string>

namespace exasol::udf::v2::protocol::arrow_types::correlation
{

/// Arrow extension array whose RunEndEncoded storage describes consecutive IDs.
class RangeRunArray final : public arrow::ExtensionArray
{
public:
    using arrow::ExtensionArray::ExtensionArray;
};

/// The exasol.udf.range_run extension over RunEndEncoded<int64, uint64>.
class RangeRunType final : public arrow::ExtensionType
{
public:
    RangeRunType();

    [[nodiscard]] std::string extension_name() const override;
    [[nodiscard]] bool ExtensionEquals(const arrow::ExtensionType& other) const override;
    [[nodiscard]] std::shared_ptr<arrow::Array> MakeArray(
        std::shared_ptr<arrow::ArrayData> data) const override;
    [[nodiscard]] arrow::Result<std::shared_ptr<arrow::DataType>> Deserialize(
        std::shared_ptr<arrow::DataType> storage_type,
        const std::string& serialized_data) const override;
    [[nodiscard]] std::string Serialize() const override;
};

/// Registers the extension for Arrow C Data imports; repeated calls are safe.
arrow::Status register_range_run_type();

/// Encodes consecutive uint64 ranges from their starting values and lengths.
/// Callers choose run boundaries, including any required group boundaries.
arrow::Result<std::shared_ptr<RangeRunArray>> create_range_run(
    std::span<const std::uint64_t> values, std::span<const std::uint64_t> counts);

} // namespace exasol::udf::v2::protocol::arrow_types::correlation
