#include <exasol/udf/v2/context/metadata_codec.hpp>
#include <exasol/udf/v2/json_schema.hpp>

#include <gtest/gtest.h>

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace exasol::udf::v2::context
{
namespace
{

    class MetadataExampleReadError final : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };

    std::string read_example(const std::string& name)
    {
        std::ifstream input("testdata/high_level_examples/" + name);
        if (!input)
        {
            throw MetadataExampleReadError("cannot open metadata example: " + name);
        }
        std::ostringstream contents;
        contents << input.rdbuf();
        return contents.str();
    }

} // namespace

TEST(MetadataCodecExampleTest, DecodesCallMetadataExample)
{
    const auto result = MetadataCodec::decode_call_metadata(read_example("call_metadata.json"));
    ASSERT_TRUE(result);
    EXPECT_EQ(result.value->database_name, "EXASOL");
    EXPECT_EQ(result.value->session_id, "42");
}

TEST(MetadataCodecExampleTest, DecodesColumnMetadataExample)
{
    const auto result = MetadataCodec::decode_column_metadata(read_example("column_metadata.json"));
    ASSERT_TRUE(result);
    ASSERT_EQ(result.value->input_columns.size(), 1U);
    EXPECT_EQ(result.value->input_columns.front().type, ExasolType::Decimal);
    EXPECT_EQ(result.value->input_columns.front().precision, 12U);
}

TEST(MetadataCodecExampleTest, DecodesConnectionExample)
{
    const auto result =
        MetadataCodec::decode_connection_information(read_example("connection_information.json"));
    ASSERT_TRUE(result);
    EXPECT_EQ(result.value->kind, "JDBC");
}

TEST(MetadataCodecExampleTest, DecodesImportExample)
{
    const auto result =
        MetadataCodec::decode_import_specification(read_example("import_specification.json"));
    ASSERT_TRUE(result);
    EXPECT_TRUE(result.value->is_subselect);
    ASSERT_EQ(result.value->subselect_column_specification.size(), 1U);
    ASSERT_EQ(result.value->parameters.size(), 1U);
}

TEST(MetadataCodecExampleTest, DecodesExportExample)
{
    const auto result =
        MetadataCodec::decode_export_specification(read_example("export_specification.json"));
    ASSERT_TRUE(result);
    EXPECT_TRUE(result.value->has_truncate);
    EXPECT_FALSE(result.value->has_replace);
    EXPECT_EQ(result.value->source_column_names.size(), 2U);
}

TEST(MetadataCodecValidationTest, RejectsMalformedJson)
{
    const auto result = MetadataCodec::decode_call_metadata("{");
    EXPECT_FALSE(result);
    EXPECT_EQ(result.status, interface::OperationStatus::ProtocolError);
}

TEST(MetadataCodecValidationTest, RejectsWrongTopLevelType)
{
    EXPECT_FALSE(MetadataCodec::validate(JsonDocument::CallMetadata, "[]"));
    EXPECT_FALSE(MetadataCodec::validate(JsonDocument::ConnectionInformation, "null"));
}

TEST(MetadataCodecValidationTest, RejectsInvalidCallMetadataValues)
{
    const std::string metadata = R"({
        "database_name":"EXASOL", "database_version":"8", "session_id":"1",
        "statement_id":-1, "node_count":1, "node_id":1, "vm_id":"1",
        "maximal_memory_limit":"1", "script_schema":"SYS",
        "input_iter_type":"INVALID", "output_iter_type":"EXACTLY_ONCE",
        "single_call_mode":false
    })";
    EXPECT_FALSE(MetadataCodec::decode_call_metadata(metadata));
}

TEST(MetadataCodecValidationTest, RejectsInvalidColumnOptionalValues)
{
    const std::string metadata = R"({
        "input_columns":[{"name":"A", "type":"DECIMAL", "type_name":"DECIMAL",
                           "precision":-1, "character_set":"UTF16"}],
        "output_columns":[]
    })";
    EXPECT_FALSE(MetadataCodec::decode_column_metadata(metadata));
}

TEST(MetadataCodecValidationTest, RejectsInvalidNestedImportValues)
{
    EXPECT_FALSE(MetadataCodec::decode_import_specification(
        R"({"is_subselect":true,"connection_information":{"kind":"JDBC"}})"));
    EXPECT_FALSE(MetadataCodec::decode_import_specification(
        R"({"is_subselect":true,"parameters":[{"key":"x","value":1}]})"));
}

TEST(MetadataCodecValidationTest, RejectsInvalidNestedExportValues)
{
    EXPECT_FALSE(MetadataCodec::decode_export_specification(
        R"({"has_truncate":false,"has_replace":false,"source_column_names":[1]})"));
    EXPECT_FALSE(MetadataCodec::decode_export_specification(
        R"({"has_truncate":false,"has_replace":false,"connection_name":true})"));
}

TEST(MetadataCodecValidationTest, DecodesOptionalCallFields)
{
    auto metadata = read_example("call_metadata.json");
    metadata.insert(metadata.rfind('}'),
                    R"(,"current_user":"SYS","current_schema":"APP","scope_user":"U")");
    const auto result = MetadataCodec::decode_call_metadata(metadata);
    ASSERT_TRUE(result);
    EXPECT_EQ(result.value->current_user, "SYS");
    EXPECT_EQ(result.value->current_schema, "APP");
    EXPECT_EQ(result.value->scope_user, "U");
}

} // namespace exasol::udf::v2::context
