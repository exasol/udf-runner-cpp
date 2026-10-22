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

    std::string read_example(const std::string& name)
    {
        std::ifstream input("testdata/high_level_examples/" + name);
        if (!input)
        {
            throw std::runtime_error("cannot open metadata example: " + name);
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

} // namespace exasol::udf::v2::context
