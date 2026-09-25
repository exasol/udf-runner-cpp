#ifndef EXASOL_UDF_V2_JSON_SCHEMA_FUZZING_HPP_
#define EXASOL_UDF_V2_JSON_SCHEMA_FUZZING_HPP_

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>

#include <exasol/udf/v2/json_schema.hpp>

namespace exasol::udf::v2::fuzzing
{

namespace isolated_nlohmann = exasol::udf::v2::third_party::nlohmann;
using Json                  = isolated_nlohmann::json;
using JsonValidator         = isolated_nlohmann::json_schema::json_validator;

class SchemaReadError final : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

inline Json read_schema(const char* path)
{
    std::ifstream input(path);
    if (!input.good())
    {
        throw SchemaReadError(std::string("Unable to read JSON schema: ") + path);
    }
    return Json::parse(input);
}

class SchemaValidator
{
public:
    explicit SchemaValidator(const char* root_schema_path)
    {
        this->validator.set_root_schema(read_schema(root_schema_path));
    }

    void validate(const uint8_t* data, std::size_t size) const noexcept
    {
        if (size == 0)
        {
            return;
        }

        try
        {
            const auto input = Json::parse(std::string(reinterpret_cast<const char*>(data), size));
            this->validator.validate(input);
        }
        catch (const std::exception& exception)
        {
            // Parse and schema-validation failures are expected input
            // outcomes. Sanitizer findings and other process failures still
            // terminate the fuzz target.
            static_cast<void>(exception);
        }
    }

private:
    JsonValidator validator{[](const isolated_nlohmann::json_uri&, Json& schema) {
        // The import and export schemas are the only schemas with an external
        // reference, and both reference this schema.
        schema = read_schema("json_schema/connection_information.schema.json");
    }};
};

inline void fuzz_json_schema(const uint8_t* data, std::size_t size, const char* schema_path)
{
    static const auto validator = std::make_unique<SchemaValidator>(schema_path);
    validator->validate(data, size);
}

} // namespace exasol::udf::v2::fuzzing

#endif // EXASOL_UDF_V2_JSON_SCHEMA_FUZZING_HPP_
