#include <cassert>
#include <cstdlib>
#include <fstream>
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
            const auto path = uri.path();
            const auto filename = path.substr(path.find_last_of('/') + 1);
            if (filename == "connection_information.schema.json" ||
                filename == "column.schema.json") {
                schema = read_json("json_schema/" + filename);
                return;
            }
            throw std::runtime_error("unsupported schema reference: " + uri.url());
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

    const isolated_nlohmann::json valid_with_columns = {
        {"is_subselect", true},
        {"subselect_column_specification", {{{
            {"name", "ID"},
            {"type", "DECIMAL"},
            {"type_name", "DECIMAL(18,0)"},
            {"precision", 18},
            {"scale", 0},
        }}}},
    };
    validator.validate(valid_with_columns);

    bool rejected = false;
    try {
        validator.validate(isolated_nlohmann::json::object());
    } catch (const std::exception&) {
        rejected = true;
    }
    assert(rejected);
}
