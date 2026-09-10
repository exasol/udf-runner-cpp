#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#include <exasol/udf/v2/json_schema.hpp>

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
        [](const isolated_nlohmann::json_uri& uri, isolated_nlohmann::json& schema) {
            std::cerr << "schema loader request: url=" << uri.url()
                      << ", location=" << uri.location()
                      << ", path=" << uri.path()
                      << ", fragment=" << uri.fragment() << '\n';

            schema = read_json("json_schema/connection_information.schema.json");

            std::cerr << "schema loader response: source=json_schema/connection_information.schema.json"
                      << ", type=" << schema.type_name() << ", keys=[";
            bool first = true;
            for (const auto& item : schema.items()) {
                if (!first) {
                    std::cerr << ',';
                }
                std::cerr << item.key();
                first = false;
            }
            std::cerr << "]\n";
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
