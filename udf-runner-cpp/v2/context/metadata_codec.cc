#include <exasol/udf/v2/context/metadata_codec.hpp>

#include <exasol/udf/v2/json_schema.hpp>

#include <algorithm>
#include <array>
#include <limits>
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
        auto validation = MetadataCodec::validate(document, text);
        if (!validation)
        {
            return decode_error<Json>(validation);
        }
        try
        {
            return interface::Result<Json>::success(Json::parse(text));
        }
        catch (const std::exception& error)
        {
            return interface::Result<Json>::failure(
                interface::OperationStatus::ProtocolError,
                interface::ErrorInfo{"invalid_json", error.what()});
        }
    }

    [[nodiscard]] ExasolType decode_type(const std::string_view type)
    {
        static constexpr std::array<std::pair<std::string_view, ExasolType>, 12> types{
            {{"DOUBLE PRECISION", ExasolType::DoublePrecision},
             {"DECIMAL", ExasolType::Decimal},
             {"DATE", ExasolType::Date},
             {"TIMESTAMP", ExasolType::Timestamp},
             {"TIMESTAMP WITH LOCAL TIME ZONE", ExasolType::TimestampWithLocalTimeZone},
             {"CHAR", ExasolType::Char},
             {"VARCHAR", ExasolType::Varchar},
             {"BOOLEAN", ExasolType::Boolean},
             {"HASHTYPE", ExasolType::HashType},
             {"GEOMETRY", ExasolType::Geometry},
             {"INTERVAL YEAR TO MONTH", ExasolType::IntervalYearToMonth},
             {"INTERVAL DAY TO SECOND", ExasolType::IntervalDayToSecond}}};
        return std::find_if(types.begin(), types.end(),
                            [type](const auto& item) { return item.first == type; })
            ->second;
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

} // namespace

interface::Result<void> MetadataCodec::validate(const JsonDocument document,
                                                const std::string_view json)
{
    Json value;
    try
    {
        value = Json::parse(json);
    }
    catch (const std::exception& error)
    {
        return invalid_metadata(error.what());
    }
    if (!value.is_object())
    {
        return invalid_metadata("metadata document must be an object");
    }
    const auto has_string = [&value](const std::string_view name) {
        return value.contains(name) && value.at(name).is_string();
    };
    const auto has_boolean = [&value](const std::string_view name) {
        return value.contains(name) && value.at(name).is_boolean();
    };
    const auto has_array = [&value](const std::string_view name) {
        return value.contains(name) && value.at(name).is_array();
    };
    const auto has_u32 = [&value](const std::string_view name) {
        if (!value.contains(name) || !value.at(name).is_number_integer())
        {
            return false;
        }
        try
        {
            return value.at(name).get<std::uint64_t>() <= std::numeric_limits<std::uint32_t>::max();
        }
        catch (const std::exception&)
        {
            return false;
        }
    };
    const auto has_decimal_string = [&value, &has_string](const std::string_view name) {
        if (!has_string(name))
        {
            return false;
        }
        const auto text = value.at(name).get<std::string_view>();
        return !text.empty() && std::all_of(text.begin(), text.end(), [](const char character) {
            return character >= '0' && character <= '9';
        });
    };
    const auto valid_column = [](const Json& column) {
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
        return column.is_object() && column.contains("name") && column.at("name").is_string() &&
               column.contains("type") && column.at("type").is_string() &&
               std::find(types.begin(), types.end(), column.at("type").get<std::string_view>()) !=
                   types.end() &&
               column.contains("type_name") && column.at("type_name").is_string();
    };
    const auto valid_columns = [&value, &has_array, &valid_column](const std::string_view name) {
        return has_array(name) &&
               std::all_of(value.at(name).begin(), value.at(name).end(), valid_column);
    };
    bool valid = false;
    switch (document)
    {
        case JsonDocument::CallMetadata:
            valid = has_string("database_name") && has_string("database_version") &&
                    has_decimal_string("session_id") && has_u32("statement_id") &&
                    has_u32("node_count") && has_u32("node_id") && has_decimal_string("vm_id") &&
                    has_decimal_string("maximal_memory_limit") && has_string("script_schema") &&
                    has_string("input_iter_type") && has_string("output_iter_type") &&
                    (value.at("input_iter_type") == "EXACTLY_ONCE" ||
                     value.at("input_iter_type") == "MULTIPLE") &&
                    (value.at("output_iter_type") == "EXACTLY_ONCE" ||
                     value.at("output_iter_type") == "MULTIPLE") &&
                    has_boolean("single_call_mode");
            break;
        case JsonDocument::ColumnMetadata:
            valid = valid_columns("input_columns") && valid_columns("output_columns");
            break;
        case JsonDocument::ConnectionInformation:
            valid = has_string("kind") && has_string("address") && has_string("user") &&
                    has_string("password");
            break;
        case JsonDocument::ImportSpecification:
            valid = has_boolean("is_subselect");
            break;
        case JsonDocument::ExportSpecification:
            valid = has_boolean("has_truncate") && has_boolean("has_replace");
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
