#include <exasol/udf/v2/posix/posix.hpp>

#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>

#include <gtest/gtest.h>

namespace
{

using exasol::udf::v2::posix::Native;

class PosixPathTest : public testing::Test
{
protected:
    void SetUp() override
    {
        auto directory_template = std::to_array(P_tmpdir "/udf-runner-cpp-posix-XXXXXX"); // NOSONAR
        char* directory         = ::mkdtemp(directory_template.data());                   // NOSONAR
        ASSERT_NE(directory, nullptr);
        temporary_directory = directory;
    }

    void TearDown() override
    {
        std::error_code error;
        std::filesystem::remove_all(temporary_directory, error);
        EXPECT_FALSE(error) << error.message();
    }

    [[nodiscard]] std::filesystem::path socket_path() const
    {
        return temporary_directory / "udf_runner_posix_test.sock";
    }

private:
    std::filesystem::path temporary_directory;
};

sockaddr_un make_address(const std::filesystem::path& path, socklen_t& length)
{
    sockaddr_un address{};
    address.sun_family            = AF_UNIX;
    const std::string path_string = path.string();
    if (path_string.size() >= sizeof(address.sun_path))
    {
        throw std::invalid_argument("Unix socket path is too long");
    }
    std::memcpy(address.sun_path, path_string.c_str(), path_string.size() + 1);
    length = static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + path_string.size() + 1);
    return address;
}

} // namespace

TEST_F(PosixPathTest, CreatesConnectsListensAndAcceptsUnixSocket)
{
    Native posix;
    const std::filesystem::path path = socket_path();
    socklen_t address_length         = 0;
    const sockaddr_un address        = make_address(path, address_length);

    const int listener = posix.socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    ASSERT_NE(listener, -1);
    ASSERT_EQ(posix.bind(listener, reinterpret_cast<const sockaddr*>(&address), address_length), 0);
    ASSERT_EQ(posix.listen(listener, 1), 0);

    const int client = posix.socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    ASSERT_NE(client, -1);
    ASSERT_EQ(posix.connect(client, reinterpret_cast<const sockaddr*>(&address), address_length),
              0);

    const int accepted = posix.accept4(listener, nullptr, nullptr, SOCK_CLOEXEC);
    ASSERT_NE(accepted, -1);
    EXPECT_EQ(posix.close(accepted), 0);
    EXPECT_EQ(posix.close(client), 0);
    EXPECT_EQ(posix.close(listener), 0);
    EXPECT_EQ(posix.unlink(path.c_str()), 0);
}

TEST(PosixTest, PerformsSocketOptionsShutdownAndVectoredIo)
{
    Native posix;
    std::array<int, 2> sockets{};
    ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets.data()), 0);

    int socket_type              = 0;
    socklen_t socket_type_length = sizeof(socket_type);
    ASSERT_EQ(posix.getsockopt(sockets[0], SOL_SOCKET, SO_TYPE, &socket_type, &socket_type_length),
              0);
    EXPECT_EQ(socket_type, SOCK_STREAM);

    std::array<std::byte, 4> written{
        {std::byte{'t'}, std::byte{'e'}, std::byte{'s'}, std::byte{'t'}}};
    iovec write_buffer{written.data(), written.size()};
    msghdr message{};
    message.msg_iov    = &write_buffer;
    message.msg_iovlen = 1;
    ASSERT_EQ(posix.sendmsg(sockets[0], &message, MSG_NOSIGNAL), 4);

    std::array<std::byte, 4> received{};
    iovec read_buffer{received.data(), received.size()};
    ASSERT_EQ(posix.readv(sockets[1], &read_buffer, 1), 4);
    EXPECT_EQ(received, written);

    EXPECT_EQ(posix.shutdown(sockets[0], SHUT_RDWR), 0);
    EXPECT_EQ(posix.close(sockets[0]), 0);
    EXPECT_EQ(posix.close(sockets[1]), 0);
}

TEST(PosixTest, CreatesControlsAndWaitsForEpoll)
{
    Native posix;
    std::array<int, 2> sockets{};
    ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets.data()), 0);

    const int epoll_fd = posix.epoll_create1(EPOLL_CLOEXEC);
    ASSERT_NE(epoll_fd, -1);

    epoll_event registration{};
    registration.events   = EPOLLIN;
    registration.data.u64 = 42;
    ASSERT_EQ(posix.epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sockets[1], &registration), 0);

    constexpr char signal = 'x';
    ASSERT_EQ(posix.write(sockets[0], &signal, sizeof(signal)), sizeof(signal));

    epoll_event ready{};
    ASSERT_EQ(posix.epoll_wait(epoll_fd, &ready, 1, 1000), 1);
    EXPECT_EQ(ready.data.u64, 42);
    EXPECT_NE(ready.events & EPOLLIN, 0U);

    EXPECT_EQ(posix.epoll_ctl(epoll_fd, EPOLL_CTL_DEL, sockets[1], nullptr), 0);
    EXPECT_EQ(posix.close(epoll_fd), 0);
    EXPECT_EQ(posix.close(sockets[0]), 0);
    EXPECT_EQ(posix.close(sockets[1]), 0);
}

TEST(PosixTest, PreservesNativeFailureResults)
{
    Native posix;
    errno = 0;
    EXPECT_EQ(posix.close(-1), -1);
    EXPECT_EQ(errno, EBADF);

    errno = 0;
    EXPECT_EQ(posix.unlink("/path/that/does/not/exist"), -1);
    EXPECT_EQ(errno, ENOENT);
}
