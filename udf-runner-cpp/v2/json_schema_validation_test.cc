#include <fstream>
#include <stdexcept>
#include <string>

#include <exasol/udf/v2/json_schema.hpp>
// [utest~udf-v2-json-schema-validation~1 -> dsn~udf-v2-frame-schema-implementation~1]
#include <gtest/gtest.h>

namespace isolated_nlohmann = exasol::udf::v2::third_party::nlohmann;

namespace
{

class JsonSchemaTestError final : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

isolated_nlohmann::json read_json(const std::string& path)
{
    std::ifstream input(path);
    if (!input.good())
    {
        throw JsonSchemaTestError("cannot open JSON schema: " + path);
    }
    return isolated_nlohmann::json::parse(input);
}

} // namespace

TEST(JsonSchemaValidationTest, AcceptsValidImportSpecification)
{
    const auto import_schema = read_json("json_schema/import_specification.schema.json");
    isolated_nlohmann::json_schema::json_validator validator(
        [](const isolated_nlohmann::json_uri&, isolated_nlohmann::json& schema) {
            schema = read_json("json_schema/connection_information.schema.json");
        });
    validator.set_root_schema(import_schema);

    const isolated_nlohmann::json valid = {
        {"is_subselect", true},
        {"connection_information",
         {
             {"kind", "JDBC"},
             {"address", "jdbc:example://host/database"},
             {"user", "user"},
             {"password", "secret"},
         }},
    };

    EXPECT_NO_THROW(validator.validate(valid));
}

TEST(JsonSchemaValidationTest, RejectsInvalidImportSpecification)
{
    const auto import_schema = read_json("json_schema/import_specification.schema.json");
    isolated_nlohmann::json_schema::json_validator validator(
        [](const isolated_nlohmann::json_uri&, isolated_nlohmann::json& schema) {
            schema = read_json("json_schema/connection_information.schema.json");
        });
    validator.set_root_schema(import_schema);

    EXPECT_THROW(validator.validate(isolated_nlohmann::json::object()), std::exception);
}

TEST(JsonSchemaValidationTest, AcceptsColumnFieldsInCallMetadata)
{
    const auto call_metadata_schema = read_json("json_schema/call_metadata.schema.json");
    isolated_nlohmann::json_schema::json_validator validator;
    validator.set_root_schema(call_metadata_schema);

    const isolated_nlohmann::json call_metadata = {
        {"database_name", "EXASOL"},
        {"database_version", "8.0"},
        {"session_id", "42"},
        {"statement_id", 1},
        {"node_count", 1},
        {"node_id", 0},
        {"vm_id", "7"},
        {"maximal_memory_limit", "1073741824"},
        {"script_schema", "SYS"},
        {"input_iter_type", "EXACTLY_ONCE"},
        {"output_iter_type", "EXACTLY_ONCE"},
        {"single_call_mode", false},
        {"input_columns", isolated_nlohmann::json::array()},
        {"output_columns", isolated_nlohmann::json::array()},
    };

    EXPECT_NO_THROW(validator.validate(call_metadata));
}
