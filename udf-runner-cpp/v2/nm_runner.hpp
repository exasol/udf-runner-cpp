#pragma once

#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <stdexcept>
#include <string>

namespace exasol::udf::v2::test {

inline std::string run_nm(const char* option, const std::string& path) {
    std::array<int, 2> pipe_fds{};
    if (::pipe(pipe_fds.data()) != 0) {
        throw std::runtime_error("cannot create pipe for nm");
    }

    const pid_t child = ::fork();
    if (child == -1) {
        ::close(pipe_fds[0]);
        ::close(pipe_fds[1]);
        throw std::runtime_error("cannot fork nm");
    }
    if (child == 0) {
        if (::dup2(pipe_fds[1], STDOUT_FILENO) == -1) {
            _exit(126);
        }
        ::close(pipe_fds[0]);
        ::close(pipe_fds[1]);
        ::execlp("nm", "nm", option, "--defined-only", "--", path.c_str(),
                 static_cast<char*>(nullptr));
        _exit(127);
    }

    ::close(pipe_fds[1]);
    std::string output;
    std::array<char, 4096> buffer{};
    for (;;) {
        const ssize_t count = ::read(pipe_fds[0], buffer.data(), buffer.size());
        if (count > 0) {
            output.append(buffer.data(), static_cast<std::size_t>(count));
        } else if (count == 0) {
            break;
        } else if (errno != EINTR) {
            ::close(pipe_fds[0]);
            ::waitpid(child, nullptr, 0);
            throw std::runtime_error("cannot read nm output");
        }
    }
    ::close(pipe_fds[0]);

    int status = 0;
    if (::waitpid(child, &status, 0) == -1 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        throw std::runtime_error("nm failed while inspecting " + path);
    }
    return output;
}

}  // namespace exasol::udf::v2::test
