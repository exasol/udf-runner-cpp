#include "script_options_parser/legacy/script_option_lines.h"
#include <gtest/gtest.h>
#include <string>
#include <exception>
#include "script_options_parser/exception.h"

const std::string whitespace = " \t\f\v";
const std::string lineEnd = ";";

using namespace ExecutionGraph;


class ScriptOptionLinesWhitespaceTest : public ::testing::TestWithParam<std::tuple<std::string, std::string, std::string, std::string, std::string>> {};

TEST_P(ScriptOptionLinesWhitespaceTest, WhitespaceExtractOptionLineTest) {
    size_t pos;
    const std::string prefix = std::get<0>(GetParam());
    const std::string suffix = std::get<1>(GetParam());
    const std::string option = std::get<2>(GetParam());
    const std::string value = std::get<3>(GetParam());
    const std::string payload =  std::get<4>(GetParam());
    std::string code = prefix + option + value + lineEnd + suffix + "\n" + payload;
    const std::string res = extractOptionLine(code, option, whitespace, lineEnd, pos);
    EXPECT_EQ(res, value);
    EXPECT_EQ(code, prefix + suffix + "\n" + payload);
}

std::vector<std::string> white_space_strings = {"", " ", "\t", "\f", "\v", "\n", " \t", "\t ", "\t\f", "\f\t", "\f ", " \f", "\t\v", "\v\t", "\v ", " \v", "\f\v", "\v\f", "  \t", " \t "};
std::vector<std::string> keywords = {"%import", "%scriptclass", "%env", "%some_option"};
std::vector<std::string> values = {"something", "com.mycompany.MyScriptClass", "LD_LIBRARY_PATH=/nvdriver", "some-value"};
std::vector<std::string> payloads = {"anything", "\n\ndef my_func:\n\tpass", "class AnyClass\n public static void Main() {\n};\n"};

INSTANTIATE_TEST_SUITE_P(
    ScriptOptionLines,
    ScriptOptionLinesWhitespaceTest,
    ::testing::Combine(::testing::ValuesIn(white_space_strings),
                       ::testing::ValuesIn(white_space_strings),
                       ::testing::ValuesIn(keywords),
                       ::testing::ValuesIn(values),
                       ::testing::ValuesIn(payloads)
    )
);

TEST(ScriptOptionLinesTest, ignore_anything_other_than_whitepsace) {
    size_t pos;
    std::string code =
        "abc %option myoption;\n"
        "\nmycode";
    const std::string res = extractOptionLine(code, "%option", whitespace, lineEnd, pos);
    EXPECT_TRUE(res.empty());
}

TEST(ScriptOptionLinesTest, need_line_end_character) {
    size_t pos;
    std::string code =
        "%option myoption\n"
        "\nmycode";
   EXPECT_THROW({
        const std::string res = extractOptionLine(code, "%option", whitespace, lineEnd, pos);
    }, OptionParserException );
}

TEST(ScriptOptionLinesTest, only_finds_the_first_option_same_key) {
    size_t pos;
    std::string code =
        "%option myoption; %option mysecondoption;\n"
        "\nmycode";
    const std::string res = extractOptionLine(code, "%option", whitespace, lineEnd, pos);
    const std::string expected_resulting_code =
        " %option mysecondoption;\n"
        "\nmycode";

    EXPECT_EQ(res, "myoption");
    EXPECT_EQ(code, expected_resulting_code);
}

TEST(ScriptOptionLinesTest, only_finds_the_first_option_different_key) {
    size_t pos;
    std::string code =
        "%option myoption; %otheroption mysecondoption;\n"
        "\nmycode";
    const std::string res = extractOptionLine(code, "%option", whitespace, lineEnd, pos);
    const std::string expected_resulting_code =
        " %otheroption mysecondoption;\n"
        "\nmycode";

    EXPECT_EQ(res, "myoption");
    EXPECT_EQ(code, expected_resulting_code);
}

class ScriptOptionLinesInvalidOptionTest : public ::testing::TestWithParam<std::string> {};


TEST_P(ScriptOptionLinesInvalidOptionTest, value_is_mandatory) {
    size_t pos;
    const std::string invalid_option = GetParam();
    std::string code = invalid_option + "\nsomething";
    EXPECT_THROW({
     const std::string res = extractOptionLine(code, "%option", whitespace, lineEnd, pos);
    }, OptionParserException );
}

std::vector<std::string> invalid_options = {"%option ;", "%option \n", "\n%option\n;", "%option\nvalue;"};

INSTANTIATE_TEST_SUITE_P(
    ScriptOptionLines,
    ScriptOptionLinesInvalidOptionTest,
    ::testing::ValuesIn(invalid_options)
);

TEST(ScriptOptionLinesTest, ignores_any_other_option) {
    size_t pos;
    const std::string original_code =
        "%option myoption; %option mysecondoption;\n"
        "\nmycode";
    std::string code = original_code;
    const std::string res = extractOptionLine(code, "%mythirdoption", whitespace, lineEnd, pos);
    EXPECT_TRUE(res.empty());
    EXPECT_EQ(code, original_code);
}


TEST(ScriptOptionLinesTest, test_multiple_lines_with_code) {
    /**
    Verify that the parser can read options coming after some code.
    */
    size_t pos;
    const std::string original_code =
        "%option alpha beta; class Abc{};\n\n"
        "%otheroption gamma; class DEF{};\n";
    std::string code = original_code;

    std::string res = extractOptionLine(code, "%option", whitespace, lineEnd, pos);
    EXPECT_EQ(res, "alpha beta");
    std::string expected_result_code =
        " class Abc{};\n\n"
        "%otheroption gamma; class DEF{};\n";
    EXPECT_EQ(code, expected_result_code);

    res = extractOptionLine(code, "%otheroption", whitespace, lineEnd, pos);
    EXPECT_EQ(res, "gamma");
    expected_result_code =
        " class Abc{};\n\n"
        " class DEF{};\n";

    EXPECT_EQ(code, expected_result_code);
}
