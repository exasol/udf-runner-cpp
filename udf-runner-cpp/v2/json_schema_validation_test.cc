#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

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
    const std::vector<std::string> column_types = {
        "DOUBLE PRECISION", "DECIMAL", "DATE", "TIMESTAMP",
        "TIMESTAMP WITH LOCAL TIME ZONE", "CHAR", "VARCHAR", "BOOLEAN",
        "HASHTYPE", "GEOMETRY", "INTERVAL YEAR TO MONTH",
        "INTERVAL DAY TO SECOND",
    };
    const auto make_column = [](const std::string& type) {
        return isolated_nlohmann::json{
            {"name", "COLUMN_" + type},
            {"type", type},
            {"type_name", type},
        };
    };

    const auto column_metadata_schema = read_json("json_schema/column_metadata.schema.json");
    isolated_nlohmann::json_schema::json_validator column_metadata_validator;
    column_metadata_validator.set_root_schema(column_metadata_schema);
    for (const auto& type : column_types) {
        column_metadata_validator.validate({
            {"input_columns", {make_column(type)}},
            {"output_columns", isolated_nlohmann::json::array()},
        });
    }

    const auto import_schema = read_json("json_schema/import_specification.schema.json");
    const auto load_schema = [](const isolated_nlohmann::json_uri& uri,
                                isolated_nlohmann::json& schema) {
            std::cerr << "schema loader request: url=" << uri.url()
                      << ", location=" << uri.location()
                      << ", path=" << uri.path()
                      << ", fragment=" << uri.fragment() << '\n';

            const auto path = uri.path();
            const auto filename = path.substr(path.find_last_of('/') + 1);
            if (filename != "connection_information.schema.json") {
                throw std::runtime_error("unsupported schema reference: " + uri.url());
            }
            const auto source = "json_schema/" + filename;
            schema = read_json(source);

            std::cerr << "schema loader response: source=" << source
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
    };

    isolated_nlohmann::json_schema::json_validator validator(load_schema);
    validator.set_root_schema(import_schema);

    for (const auto& type : column_types) {
        validator.validate({
            {"is_subselect", true},
            {"subselect_column_specification", {make_column(type)}},
        });
    }

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
