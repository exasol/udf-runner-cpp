#pragma once

#include <arrow/extension_type.h>
#include <arrow/result.h>

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

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

/// Encodes consecutive row IDs, starting a new run whenever the group ID
/// changes. For example, row IDs [101, 102, 103, 104] and group IDs
/// [10, 10, 20, 20] produce run ends [2, 4] and starting values [101, 103].
/// `group_ids` must have one value per row ID. Omit it when there is no
/// group-ID column.
arrow::Result<std::shared_ptr<RangeRunArray>> create_row_id_range_run(
    std::span<const std::uint64_t> ids, std::span<const std::uint64_t> group_ids = {});

/// Decodes range_run or plain uint64 row IDs.
arrow::Result<std::vector<std::uint64_t>> decode_row_id_range_run(const arrow::Array& array);

} // namespace exasol::udf::v2::protocol::arrow_types::correlation
