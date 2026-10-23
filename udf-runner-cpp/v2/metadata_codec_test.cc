#include <exasol/udf/v2/context/metadata_codec.hpp>
#include <exasol/udf/v2/json_schema.hpp>

#include <gtest/gtest.h>

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace exasol::udf::v2::context
{
namespace
{

    using enum JsonDocument;
    constexpr auto connection_document = JsonDocument::ConnectionInformation;
    constexpr auto import_document     = JsonDocument::ImportSpecification;
    constexpr auto export_document     = JsonDocument::ExportSpecification;

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
    EXPECT_FALSE(MetadataCodec::validate(CallMetadata, "[]"));
    EXPECT_FALSE(MetadataCodec::validate(connection_document, "null"));
}

TEST(MetadataCodecValidationTest, RejectsInvalidCallMetadataValues)
{
    const std::string metadata = R"JSON({
        "database_name":"EXASOL", "database_version":"8", "session_id":"1",
        "statement_id":-1, "node_count":1, "node_id":1, "vm_id":"1",
        "maximal_memory_limit":"1", "script_schema":"SYS",
        "input_iter_type":"INVALID", "output_iter_type":"EXACTLY_ONCE",
        "single_call_mode":false
    })JSON";
    EXPECT_FALSE(MetadataCodec::decode_call_metadata(metadata));
}

TEST(MetadataCodecValidationTest, RejectsInvalidColumnOptionalValues)
{
    const std::string metadata = R"({
        "input_columns":[{"name":"A", "type":"DECIMAL", "type_name":"DECIMAL",
                           "precision":-1, "character_set":"UTF16"}],
        "output_columns":[{"name":"OUT","type":"BOOLEAN","type_name":"BOOLEAN"}]
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
    EXPECT_FALSE(MetadataCodec::decode_connection_information(R"({"kind":1})"));
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

TEST(MetadataCodecDecodeTest, DecodesAllColumnTypesAndOptionalFields)
{
    const std::string metadata = R"JSON({
        "input_columns":[
            {"name":"DOUBLE","type":"DOUBLE PRECISION","type_name":"DOUBLE PRECISION"},
            {"name":"DECIMAL","type":"DECIMAL","type_name":"DECIMAL(10,2)","precision":10,"scale":2},
            {"name":"DATE","type":"DATE","type_name":"DATE"},
            {"name":"TIMESTAMP","type":"TIMESTAMP","type_name":"TIMESTAMP","precision":6},
            {"name":"TIMESTAMP_TZ","type":"TIMESTAMP WITH LOCAL TIME ZONE","type_name":"TIMESTAMP WITH LOCAL TIME ZONE"},
            {"name":"CHAR","type":"CHAR","type_name":"CHAR(10)","size":10,"character_set":"ASCII"},
            {"name":"VARCHAR","type":"VARCHAR","type_name":"VARCHAR(10)","size":10,"character_set":"UTF8"},
            {"name":"BOOLEAN","type":"BOOLEAN","type_name":"BOOLEAN"},
            {"name":"HASH","type":"HASHTYPE","type_name":"HASHTYPE(8)","size":8,"size_unit":"BYTE"},
            {"name":"GEOMETRY","type":"GEOMETRY","type_name":"GEOMETRY","srid":4326},
            {"name":"YM","type":"INTERVAL YEAR TO MONTH","type_name":"INTERVAL YEAR(2) TO MONTH","precision":2},
            {"name":"DS","type":"INTERVAL DAY TO SECOND","type_name":"INTERVAL DAY(2) TO SECOND(3)","precision":2,"fractional_second_precision":3}
        ],
        "output_columns":[{"name":"OUT","type":"BOOLEAN","type_name":"BOOLEAN"}]
})JSON";
    const auto result          = MetadataCodec::decode_column_metadata(metadata);
    ASSERT_TRUE(result);
    ASSERT_EQ(result.value->input_columns.size(), 12U);
    EXPECT_EQ(result.value->input_columns[0].type, ExasolType::DoublePrecision);
    EXPECT_EQ(result.value->input_columns[2].type, ExasolType::Date);
    EXPECT_EQ(result.value->input_columns[4].type, ExasolType::TimestampWithLocalTimeZone);
    EXPECT_EQ(result.value->input_columns[8].type, ExasolType::HashType);
    EXPECT_EQ(result.value->input_columns[9].type, ExasolType::Geometry);
    EXPECT_EQ(result.value->input_columns[10].type, ExasolType::IntervalYearToMonth);
    EXPECT_EQ(result.value->input_columns[11].type, ExasolType::IntervalDayToSecond);
    EXPECT_EQ(result.value->input_columns[9].srid, 4326U);
}

TEST(MetadataCodecDecodeTest, DecodesAllOptionalImportFields)
{
    const auto result = MetadataCodec::decode_import_specification(R"({
        "is_subselect":false,
        "connection_information":{"kind":"JDBC","address":"db","user":"u","password":"p"},
        "connection_name":"REMOTE",
        "subselect_column_specification":[{"name":"ID","type":"DECIMAL","type_name":"DECIMAL","precision":10}],
        "parameters":[{"key":"encoding","value":"UTF-8"}]
    })");
    ASSERT_TRUE(result);
    ASSERT_TRUE(result.value->connection_information.has_value());
    EXPECT_EQ(result.value->connection_information->address, "db");
    EXPECT_EQ(result.value->connection_name, "REMOTE");
    ASSERT_EQ(result.value->parameters.size(), 1U);
    EXPECT_EQ(result.value->parameters.front().first, "encoding");
}

TEST(MetadataCodecDecodeTest, DecodesAllOptionalExportFields)
{
    const auto result = MetadataCodec::decode_export_specification(R"({
        "has_truncate":false,"has_replace":true,"created_by":"UDF",
        "source_column_names":["ID"],
        "connection_information":{"kind":"JDBC","address":"db","user":"u","password":"p"},
        "connection_name":"REMOTE",
        "parameters":[{"key":"format","value":"CSV"}]
    })");
    ASSERT_TRUE(result);
    EXPECT_EQ(result.value->created_by, "UDF");
    ASSERT_TRUE(result.value->connection_information.has_value());
    EXPECT_EQ(result.value->connection_name, "REMOTE");
    ASSERT_EQ(result.value->source_column_names.size(), 1U);
    ASSERT_EQ(result.value->parameters.size(), 1U);
    EXPECT_EQ(result.value->parameters.front().second, "CSV");
}

TEST(MetadataCodecValidationTest, RejectsInvalidDocumentsForEveryMetadataKind)
{
    const std::vector<std::pair<JsonDocument, std::string>> invalid_documents{
        {CallMetadata, R"({"database_name":1})"},
        {CallMetadata, R"({"session_id":"-1"})"},
        {CallMetadata, R"({"statement_id":4294967296})"},
        {CallMetadata, R"({"current_user":false})"},
        {CallMetadata, R"({"input_iter_type":"INVALID"})"},
        {ColumnMetadata, R"({"input_columns":[]})"},
        {ColumnMetadata, R"({"input_columns":[1],"output_columns":[]})"},
        {ColumnMetadata, R"({"input_columns":[{"name":1}],"output_columns":[]})"},
        {ColumnMetadata,
         R"({"input_columns":[{"name":"A","type":"UNKNOWN","type_name":"X"}],"output_columns":[]})"},
        {ColumnMetadata,
         R"({"input_columns":[{"name":"A","type":"DECIMAL","type_name":1}],"output_columns":[]})"},
        {ColumnMetadata,
         R"({"input_columns":[{"name":"A","type":"DECIMAL","type_name":"X","size_unit":1}],"output_columns":[]})"},
        {connection_document, R"({"kind":"JDBC"})"},
        {connection_document, R"({"kind":"JDBC","address":1,"user":"u","password":"p"})"},
        {import_document, R"({"is_subselect":1})"},
        {import_document, R"({"is_subselect":true,"connection_name":1})"},
        {import_document, R"({"is_subselect":true,"subselect_column_specification":1})"},
        {import_document, R"({"is_subselect":true,"parameters":1})"},
        {export_document, R"({"has_truncate":true})"},
        {export_document, R"({"has_truncate":true,"has_replace":false,"created_by":1})"},
        {export_document, R"({"has_truncate":true,"has_replace":false,"source_column_names":1})"},
        {export_document, R"({"has_truncate":true,"has_replace":false,"parameters":1})"}};

    for (const auto& [document, json] : invalid_documents)
    {
        EXPECT_FALSE(MetadataCodec::validate(document, json)) << json;
    }
}

TEST(MetadataCodecValidationTest, AcceptsMultipleIteratorsAndMaximumUnsignedValues)
{
    const std::string metadata = R"({
        "database_name":"EXASOL", "database_version":"8", "session_id":"1",
        "statement_id":4294967295, "node_count":4294967295, "node_id":4294967295,
        "vm_id":"1", "maximal_memory_limit":"1", "script_schema":"SYS",
        "input_iter_type":"MULTIPLE", "output_iter_type":"MULTIPLE",
        "single_call_mode":true
    })";
    ASSERT_TRUE(MetadataCodec::validate(CallMetadata, metadata));
    const auto result = MetadataCodec::decode_call_metadata(metadata);
    ASSERT_TRUE(result);
    EXPECT_EQ(result.value->input_iter_type, IteratorType::Multiple);
    EXPECT_EQ(result.value->output_iter_type, IteratorType::Multiple);
}

} // namespace exasol::udf::v2::context
