#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <unistd.h>

#include <exasol/udf/v2/assert.hpp>
#include <exasol/udf/v2/exception.hpp>

namespace
{

void trigger_assertion()
{
    EXASOL_UDF_ASSERT(false);
}

void test_exception()
{
    const exasol::udf::v2::Exception error("example message");
    assert(std::strcmp(error.what(), "example message") == 0);
    assert(!error.stacktrace().empty());
    assert(error.location().file_name() == std::string_view(__FILE__));
}

void test_assertion()
{
    int output_pipe[2];
    assert(::pipe(output_pipe) == 0);

    const pid_t child = ::fork();
    assert(child >= 0);
    if (child == 0)
    {
        ::close(output_pipe[0]);
        assert(::dup2(output_pipe[1], STDERR_FILENO) >= 0);
        ::close(output_pipe[1]);
        trigger_assertion();
        std::_Exit(EXIT_FAILURE);
    }

    ::close(output_pipe[1]);
    std::string output;
    char buffer[4096];
    ssize_t bytes_read = 0;
    while ((bytes_read = ::read(output_pipe[0], buffer, sizeof(buffer))) > 0)
    {
        output.append(buffer, static_cast<std::size_t>(bytes_read));
    }
    ::close(output_pipe[0]);

    int status = 0;
    assert(::waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status));
    assert(WTERMSIG(status) == SIGABRT);
    assert(!output.empty());
    assert(output.find("Assertion failed: false") != std::string::npos);
    assert(output.find("trigger_assertion") != std::string::npos);
}

} // namespace

int main()
{
    test_exception();
    test_assertion();
}
