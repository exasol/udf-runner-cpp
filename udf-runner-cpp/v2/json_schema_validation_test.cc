#include <fstream>
#include <stdexcept>
#include <string>

#include <exasol/udf/v2/json_schema.hpp>
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
