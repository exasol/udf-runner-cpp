#include <exasol/udf/v2/context/metadata_codec.hpp>

#include <exasol/udf/v2/json_schema.hpp>

#include <array>
#include <limits>
#include <ranges>
#include <string>

namespace exasol::udf::v2::context
{
namespace
{

    using Json = exasol::udf::v2::third_party::nlohmann::json;

    [[nodiscard]] interface::Result<void> invalid_metadata(std::string message)
    {
        return interface::Result<void>::failure(
            interface::OperationStatus::ProtocolError,
            interface::ErrorInfo{"invalid_metadata", std::move(message)});
    }

    template <typename T>
    interface::Result<T> decode_error(const interface::Result<void>& validation)
    {
        return interface::Result<T>::failure(validation.status, validation.error);
    }

    [[nodiscard]] interface::Result<Json> parse_document(const JsonDocument document,
                                                         const std::string_view text)
    {
        if (auto validation = MetadataCodec::validate(document, text); !validation)
        {
            return decode_error<Json>(validation);
        }
        return interface::Result<Json>::success(Json::parse(text));
    }

    [[nodiscard]] ExasolType decode_type(const std::string_view type)
    {
        using enum ExasolType;
        static constexpr std::array<std::pair<std::string_view, ExasolType>, 12> types{
            {{"DOUBLE PRECISION", DoublePrecision},
             {"DECIMAL", Decimal},
             {"DATE", Date},
             {"TIMESTAMP", Timestamp},
             {"TIMESTAMP WITH LOCAL TIME ZONE", TimestampWithLocalTimeZone},
             {"CHAR", Char},
             {"VARCHAR", Varchar},
             {"BOOLEAN", Boolean},
             {"HASHTYPE", HashType},
             {"GEOMETRY", Geometry},
             {"INTERVAL YEAR TO MONTH", IntervalYearToMonth},
             {"INTERVAL DAY TO SECOND", IntervalDayToSecond}}};
        const auto* const found =
            std::ranges::find_if(types, [type](const auto& item) { return item.first == type; });
        return found->second;
    }

    [[nodiscard]] ColumnDefinition decode_column(const Json& value)
    {
        const auto optional_u32 =
            [&value](const std::string_view name) -> std::optional<std::uint32_t> {
            if (!value.contains(name))
            {
                return std::nullopt;
            }
            return value.at(name).get<std::uint32_t>();
        };
        const auto optional_string =
            [&value](const std::string_view name) -> std::optional<std::string> {
            if (!value.contains(name))
            {
                return std::nullopt;
            }
            return value.at(name).get<std::string>();
        };
        return ColumnDefinition{value.at("name").get<std::string>(),
                                decode_type(value.at("type").get<std::string_view>()),
                                value.at("type_name").get<std::string>(),
                                optional_u32("size"),
                                optional_string("size_unit"),
                                optional_u32("precision"),
                                optional_u32("scale"),
                                optional_u32("fractional_second_precision"),
                                optional_string("character_set"),
                                optional_u32("srid")};
    }

    template <typename T>
    [[nodiscard]] interface::Result<T> decode_parameters(const Json& value,
                                                         const std::string_view name,
                                                         T result)
    {
        if (value.contains(name))
        {
            for (const auto& parameter : value.at(name))
            {
                result.parameters.emplace_back(parameter.at("key").get<std::string>(),
                                               parameter.at("value").get<std::string>());
            }
        }
        return interface::Result<T>::success(std::move(result));
    }

    [[nodiscard]] std::optional<ConnectionInformation> decode_optional_connection(const Json& value)
    {
        if (!value.contains("connection_information"))
        {
            return std::nullopt;
        }
        const auto& connection = value.at("connection_information");
        return ConnectionInformation{
            connection.at("kind").get<std::string>(), connection.at("address").get<std::string>(),
            connection.at("user").get<std::string>(), connection.at("password").get<std::string>()};
    }

    [[nodiscard]] bool has_string(const Json& value, const std::string_view name)
    {
        return value.contains(name) && value.at(name).is_string();
    }

    [[nodiscard]] bool has_boolean(const Json& value, const std::string_view name)
    {
        return value.contains(name) && value.at(name).is_boolean();
    }

    [[nodiscard]] bool has_array(const Json& value, const std::string_view name)
    {
        return value.contains(name) && value.at(name).is_array();
    }

    [[nodiscard]] bool has_u32(const Json& value, const std::string_view name)
    {
        if (!value.contains(name) || !value.at(name).is_number_integer())
        {
            return false;
        }
        if (value.at(name).is_number_unsigned())
        {
            return value.at(name).get<std::uint64_t>() <= std::numeric_limits<std::uint32_t>::max();
        }
        return value.at(name).get<std::int64_t>() >= 0;
    }

    [[nodiscard]] bool has_decimal_string(const Json& value, const std::string_view name)
    {
        if (!has_string(value, name))
        {
            return false;
        }
        const auto text = value.at(name).get<std::string_view>();
        return !text.empty() && std::ranges::all_of(text, [](const char character) {
            return character >= '0' && character <= '9';
        });
    }

    [[nodiscard]] bool valid_optional_u32(const Json& value, const std::string_view name)
    {
        if (!value.contains(name))
        {
            return true;
        }
        if (!value.at(name).is_number_integer())
        {
            return false;
        }
        if (value.at(name).is_number_unsigned())
        {
            return value.at(name).get<std::uint64_t>() <= std::numeric_limits<std::uint32_t>::max();
        }
        return value.at(name).get<std::int64_t>() >= 0;
    }

    [[nodiscard]] bool valid_optional_enum(const Json& value,
                                           const std::string_view name,
                                           const std::array<std::string_view, 2>& values)
    {
        return !value.contains(name) ||
               (value.at(name).is_string() &&
                std::ranges::find(values, value.at(name).get<std::string_view>()) != values.end());
    }

    [[nodiscard]] bool valid_column(const Json& column)
    {
        static constexpr std::array<std::string_view, 12> types{"DOUBLE PRECISION",
                                                                "DECIMAL",
                                                                "DATE",
                                                                "TIMESTAMP",
                                                                "TIMESTAMP WITH LOCAL TIME ZONE",
                                                                "CHAR",
                                                                "VARCHAR",
                                                                "BOOLEAN",
                                                                "HASHTYPE",
                                                                "GEOMETRY",
                                                                "INTERVAL YEAR TO MONTH",
                                                                "INTERVAL DAY TO SECOND"};
        return column.is_object() && has_string(column, "name") && has_string(column, "type") &&
               std::ranges::find(types, column.at("type").get<std::string_view>()) != types.end() &&
               has_string(column, "type_name") && valid_optional_u32(column, "size") &&
               valid_optional_u32(column, "precision") && valid_optional_u32(column, "scale") &&
               valid_optional_u32(column, "fractional_second_precision") &&
               valid_optional_u32(column, "srid") &&
               valid_optional_enum(column, "size_unit", {"BYTE", "BIT"}) &&
               valid_optional_enum(column, "character_set", {"ASCII", "UTF8"});
    }

    [[nodiscard]] bool valid_columns(const Json& value, const std::string_view name)
    {
        return has_array(value, name) && std::ranges::all_of(value.at(name), valid_column);
    }

    [[nodiscard]] bool valid_connection(const Json& value)
    {
        return value.is_object() && has_string(value, "kind") && has_string(value, "address") &&
               has_string(value, "user") && has_string(value, "password");
    }

    [[nodiscard]] bool valid_parameters(const Json& value)
    {
        return value.is_array() && std::ranges::all_of(value, [](const Json& parameter) {
                   return parameter.is_object() && has_string(parameter, "key") &&
                          has_string(parameter, "value");
               });
    }

    [[nodiscard]] bool valid_optional_string(const Json& value, const std::string_view name)
    {
        return !value.contains(name) || value.at(name).is_string();
    }

    [[nodiscard]] bool valid_optional_connection(const Json& value)
    {
        return !value.contains("connection_information") ||
               valid_connection(value.at("connection_information"));
    }

    [[nodiscard]] bool valid_optional_columns(const Json& value, const std::string_view name)
    {
        return !value.contains(name) || valid_columns(value, name);
    }

    [[nodiscard]] bool valid_call_metadata(const Json& value)
    {
        const auto valid_iterator = [&value](const std::string_view name) {
            return has_string(value, name) &&
                   (value.at(name) == "EXACTLY_ONCE" || value.at(name) == "MULTIPLE");
        };
        return has_string(value, "database_name") && has_string(value, "database_version") &&
               has_decimal_string(value, "session_id") && has_u32(value, "statement_id") &&
               has_u32(value, "node_count") && has_u32(value, "node_id") &&
               has_decimal_string(value, "vm_id") &&
               has_decimal_string(value, "maximal_memory_limit") &&
               has_string(value, "script_schema") && valid_optional_string(value, "current_user") &&
               valid_optional_string(value, "current_schema") &&
               valid_optional_string(value, "scope_user") && valid_iterator("input_iter_type") &&
               valid_iterator("output_iter_type") && has_boolean(value, "single_call_mode");
    }

    [[nodiscard]] bool valid_import_specification(const Json& value)
    {
        return has_boolean(value, "is_subselect") &&
               valid_optional_string(value, "connection_name") &&
               valid_optional_connection(value) &&
               valid_optional_columns(value, "subselect_column_specification") &&
               (!value.contains("parameters") || valid_parameters(value.at("parameters")));
    }

    [[nodiscard]] bool valid_export_specification(const Json& value)
    {
        const auto valid_source_columns = [&value] {
            return !value.contains("source_column_names") ||
                   (has_array(value, "source_column_names") &&
                    std::ranges::all_of(value.at("source_column_names"),
                                        [](const Json& item) { return item.is_string(); }));
        };
        return has_boolean(value, "has_truncate") && has_boolean(value, "has_replace") &&
               valid_optional_string(value, "created_by") &&
               valid_optional_string(value, "connection_name") &&
               valid_optional_connection(value) && valid_source_columns() &&
               (!value.contains("parameters") || valid_parameters(value.at("parameters")));
    }

} // namespace

interface::Result<void> MetadataCodec::validate(const JsonDocument document,
                                                const std::string_view json)
{
    Json value;
    try
    {
        value = Json::parse(json);
    }
    catch (const Json::parse_error& error)
    {
        return invalid_metadata(error.what());
    }
    if (!value.is_object())
    {
        return invalid_metadata("metadata document must be an object");
    }
    using enum JsonDocument;
    bool valid = false;
    switch (document)
    {
        case CallMetadata:
            valid = valid_call_metadata(value);
            break;
        case ColumnMetadata:
            valid = valid_columns(value, "input_columns") && valid_columns(value, "output_columns");
            break;
        case ConnectionInformation:
            valid = valid_connection(value);
            break;
        case ImportSpecification:
            valid = valid_import_specification(value);
            break;
        case ExportSpecification:
            valid = valid_export_specification(value);
            break;
    }
    return valid ? interface::Result<void>::success()
                 : invalid_metadata("metadata document is missing required fields");
}

interface::Result<DecodedCallMetadata> MetadataCodec::decode_call_metadata(
    const std::string_view json)
{
    auto parsed = parse_document(JsonDocument::CallMetadata, json);
    if (!parsed)
    {
        return decode_error<DecodedCallMetadata>(
            interface::Result<void>::failure(parsed.status, parsed.error));
    }
    const auto& value   = *parsed.value;
    const auto iterator = [&value](const std::string_view name) {
        return value.at(name).get<std::string>() == "MULTIPLE" ? IteratorType::Multiple
                                                               : IteratorType::ExactlyOnce;
    };
    return interface::Result<DecodedCallMetadata>::success(DecodedCallMetadata{
        value.at("database_name").get<std::string>(),
        value.at("database_version").get<std::string>(), value.at("session_id").get<std::string>(),
        value.at("statement_id").get<std::uint32_t>(), value.at("node_count").get<std::uint32_t>(),
        value.at("node_id").get<std::uint32_t>(), value.at("vm_id").get<std::string>(),
        value.at("maximal_memory_limit").get<std::string>(),
        value.at("script_schema").get<std::string>(),
        value.contains("current_user") ? std::optional{value.at("current_user").get<std::string>()}
                                       : std::nullopt,
        value.contains("current_schema")
            ? std::optional{value.at("current_schema").get<std::string>()}
            : std::nullopt,
        value.contains("scope_user") ? std::optional{value.at("scope_user").get<std::string>()}
                                     : std::nullopt,
        iterator("input_iter_type"), iterator("output_iter_type"),
        value.at("single_call_mode").get<bool>()});
}

interface::Result<DecodedColumnMetadata> MetadataCodec::decode_column_metadata(
    const std::string_view json)
{
    auto parsed = parse_document(JsonDocument::ColumnMetadata, json);
    if (!parsed)
    {
        return decode_error<DecodedColumnMetadata>(
            interface::Result<void>::failure(parsed.status, parsed.error));
    }
    DecodedColumnMetadata result;
    for (const auto& column : (*parsed.value).at("input_columns"))
    {
        result.input_columns.emplace_back(decode_column(column));
    }
    for (const auto& column : (*parsed.value).at("output_columns"))
    {
        result.output_columns.emplace_back(decode_column(column));
    }
    return interface::Result<DecodedColumnMetadata>::success(std::move(result));
}

interface::Result<ConnectionInformation> MetadataCodec::decode_connection_information(
    const std::string_view json)
{
    auto parsed = parse_document(JsonDocument::ConnectionInformation, json);
    if (!parsed)
    {
        return decode_error<ConnectionInformation>(
            interface::Result<void>::failure(parsed.status, parsed.error));
    }
    const auto& value = *parsed.value;
    return interface::Result<ConnectionInformation>::success(ConnectionInformation{
        value.at("kind").get<std::string>(), value.at("address").get<std::string>(),
        value.at("user").get<std::string>(), value.at("password").get<std::string>()});
}

interface::Result<ImportSpecification> MetadataCodec::decode_import_specification(
    const std::string_view json)
{
    auto parsed = parse_document(JsonDocument::ImportSpecification, json);
    if (!parsed)
    {
        return decode_error<ImportSpecification>(
            interface::Result<void>::failure(parsed.status, parsed.error));
    }
    const auto& value = *parsed.value;
    ImportSpecification result;
    result.is_subselect           = value.at("is_subselect").get<bool>();
    result.connection_information = decode_optional_connection(value);
    if (value.contains("connection_name"))
    {
        result.connection_name = value.at("connection_name").get<std::string>();
    }
    if (value.contains("subselect_column_specification"))
    {
        for (const auto& column : value.at("subselect_column_specification"))
        {
            result.subselect_column_specification.emplace_back(decode_column(column));
        }
    }
    return decode_parameters(value, "parameters", std::move(result));
}

interface::Result<ExportSpecification> MetadataCodec::decode_export_specification(
    const std::string_view json)
{
    auto parsed = parse_document(JsonDocument::ExportSpecification, json);
    if (!parsed)
    {
        return decode_error<ExportSpecification>(
            interface::Result<void>::failure(parsed.status, parsed.error));
    }
    const auto& value = *parsed.value;
    ExportSpecification result;
    result.has_truncate           = value.at("has_truncate").get<bool>();
    result.has_replace            = value.at("has_replace").get<bool>();
    result.connection_information = decode_optional_connection(value);
    if (value.contains("created_by"))
    {
        result.created_by = value.at("created_by").get<std::string>();
    }
    if (value.contains("connection_name"))
    {
        result.connection_name = value.at("connection_name").get<std::string>();
    }
    if (value.contains("source_column_names"))
    {
        for (const auto& column : value.at("source_column_names"))
        {
            result.source_column_names.emplace_back(column.get<std::string>());
        }
    }
    return decode_parameters(value, "parameters", std::move(result));
}

} // namespace exasol::udf::v2::context
