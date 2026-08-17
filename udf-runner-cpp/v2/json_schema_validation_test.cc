#include <cassert>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <string>

#include <nlohmann/json-schema.hpp>

namespace isolated_nlohmann = exasol::udf::v2::third_party::nlohmann;

namespace {

isolated_nlohmann::json read_json(const std::string& path) {
    std::ifstream input(path);
    assert(input.good());
    return isolated_nlohmann::json::parse(input);
}

}  // namespace

int main() {
    const auto import_schema = read_json("json_schema/import_specification.schema.json");
    isolated_nlohmann::json_schema::json_validator validator(
        [](const isolated_nlohmann::json_uri&, isolated_nlohmann::json& schema) {
            schema = read_json("json_schema/connection_information.schema.json");
        });
    validator.set_root_schema(import_schema);

    const isolated_nlohmann::json valid = {
        {"is_subselect", true},
        {"connection_information", {
            {"kind", "JDBC"},
            {"address", "jdbc:example://host/database"},
            {"user", "user"},
            {"password", "secret"},
        }},
    };
    validator.validate(valid);

    bool rejected = false;
    try {
        validator.validate(isolated_nlohmann::json::object());
    } catch (const std::exception&) {
        rejected = true;
    }
    assert(rejected);
}
