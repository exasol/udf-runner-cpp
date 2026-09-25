#include <array>
#include <cstdlib>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <unistd.h>

#include <exasol/udf/v2/assert.hpp>
#include <exasol/udf/v2/exception.hpp>
#include <gtest/gtest.h>

namespace
{

void trigger_assertion()
{
    EXASOL_UDF_ASSERT(false);
}

TEST(ExceptionTest, CapturesMessageLocationAndStacktrace)
{
    const exasol::udf::v2::Exception error("example message");
    EXPECT_STREQ(error.what(), "example message");
    EXPECT_FALSE(error.stacktrace().empty());
    EXPECT_EQ(error.location().file_name(), std::string_view(__FILE__));
}

TEST(ExceptionTest, AssertionAbortsAndPrintsStacktrace)
{
    std::array<int, 2> output_pipe{};
    const int pipe_result = ::pipe(output_pipe.data());
    ASSERT_EQ(pipe_result, 0);

    const pid_t child = ::fork();
    ASSERT_GE(child, 0);
    if (child == 0)
    {
        ::close(output_pipe[0]);
        const int dup2_result = ::dup2(output_pipe[1], STDERR_FILENO);
        if (dup2_result < 0)
        {
            std::_Exit(EXIT_FAILURE);
        }
        ::close(output_pipe[1]);
        trigger_assertion();
        std::_Exit(EXIT_FAILURE);
    }

    ::close(output_pipe[1]);
    std::string output;
    std::array<char, 4096> buffer{};
    ssize_t bytes_read = 0;
    while ((bytes_read = ::read(output_pipe[0], buffer.data(), buffer.size())) > 0)
    {
        output.append(buffer.data(), static_cast<std::size_t>(bytes_read));
    }
    ::close(output_pipe[0]);
    ASSERT_GE(bytes_read, 0);

    int status              = 0;
    const pid_t wait_result = ::waitpid(child, &status, 0);
    ASSERT_EQ(wait_result, child);
    ASSERT_TRUE(WIFSIGNALED(status));
    EXPECT_EQ(WTERMSIG(status), SIGABRT);
    EXPECT_FALSE(output.empty());
    EXPECT_NE(output.find("Assertion failed: false"), std::string::npos);
    EXPECT_NE(output.find("trigger_assertion"), std::string::npos);
}

} // namespace
