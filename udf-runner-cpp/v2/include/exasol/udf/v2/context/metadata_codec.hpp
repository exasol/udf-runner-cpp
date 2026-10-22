#pragma once

#include <exasol/udf/v2/interface/result.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace exasol::udf::v2::context
{

/// Identifies a supported high-level JSON document.
enum class JsonDocument
{
    CallMetadata,
    ColumnMetadata,
    ConnectionInformation,
    ImportSpecification,
    ExportSpecification
};

/// Iterator scheduling mode declared by call metadata.
enum class IteratorType
{
    ExactlyOnce,
    Multiple
};

/// Exasol logical type declared by a column definition.
enum class ExasolType
{
    DoublePrecision,
    Decimal,
    Date,
    Timestamp,
    TimestampWithLocalTimeZone,
    Char,
    Varchar,
    Boolean,
    HashType,
    Geometry,
    IntervalYearToMonth,
    IntervalDayToSecond
};

/// Validated definition of one Exasol column.
struct ColumnDefinition
{
    std::string name;
    ExasolType type;
    std::string type_name;
    std::optional<std::uint32_t> size;
    std::optional<std::string> size_unit;
    std::optional<std::uint32_t> precision;
    std::optional<std::uint32_t> scale;
    std::optional<std::uint32_t> fractional_second_precision;
    std::optional<std::string> character_set;
    std::optional<std::uint32_t> srid;
};

/// Parsed and validated call metadata.
struct DecodedCallMetadata
{
    std::string database_name;
    std::string database_version;
    std::string session_id;
    std::uint32_t statement_id{};
    std::uint32_t node_count{};
    std::uint32_t node_id{};
    std::string vm_id;
    std::string maximal_memory_limit;
    std::string script_schema;
    std::optional<std::string> current_user;
    std::optional<std::string> current_schema;
    std::optional<std::string> scope_user;
    IteratorType input_iter_type{IteratorType::ExactlyOnce};
    IteratorType output_iter_type{IteratorType::ExactlyOnce};
    bool single_call_mode{false};
};

/// Parsed and validated input and output column metadata.
struct DecodedColumnMetadata
{
    std::vector<ColumnDefinition> input_columns;
    std::vector<ColumnDefinition> output_columns;
};

/// Connection details returned by a nested connection lookup.
struct ConnectionInformation
{
    std::string kind;
    std::string address;
    std::string user;
    std::string password;
};

/// Parsed import specification.
struct ImportSpecification
{
    bool is_subselect{false};
    std::optional<ConnectionInformation> connection_information;
    std::optional<std::string> connection_name;
    std::vector<ColumnDefinition> subselect_column_specification;
    std::vector<std::pair<std::string, std::string>> parameters;
};

/// Parsed export specification.
struct ExportSpecification
{
    bool has_truncate{false};
    bool has_replace{false};
    std::optional<std::string> created_by;
    std::vector<std::string> source_column_names;
    std::optional<ConnectionInformation> connection_information;
    std::optional<std::string> connection_name;
    std::vector<std::pair<std::string, std::string>> parameters;
};

/// Parses and validates high-level JSON metadata into typed values.
class MetadataCodec final
{
public:
    /// Validates a JSON document against its high-level metadata contract.
    [[nodiscard]] static interface::Result<void> validate(JsonDocument document,
                                                          std::string_view json);
    /// Decodes call metadata.
    [[nodiscard]] static interface::Result<DecodedCallMetadata> decode_call_metadata(
        std::string_view json);
    /// Decodes column metadata and all contained column definitions.
    [[nodiscard]] static interface::Result<DecodedColumnMetadata> decode_column_metadata(
        std::string_view json);
    /// Decodes connection information returned by a nested call.
    [[nodiscard]] static interface::Result<ConnectionInformation> decode_connection_information(
        std::string_view json);
    /// Decodes an import specification, including columns and parameters.
    [[nodiscard]] static interface::Result<ImportSpecification> decode_import_specification(
        std::string_view json);
    /// Decodes an export specification, including source columns and parameters.
    [[nodiscard]] static interface::Result<ExportSpecification> decode_export_specification(
        std::string_view json);
};

} // namespace exasol::udf::v2::context
