#include <exasol/udf/v2/posix/posix.hpp>

#include <sys/socket.h>
#include <sys/un.h>

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <string>

#include <gtest/gtest.h>

namespace
{

using exasol::udf::v2::posix::Posix;

std::filesystem::path socket_path()
{
    return std::filesystem::temp_directory_path() / "udf_runner_posix_test.sock";
}

sockaddr_un make_address(const std::filesystem::path& path)
{
    sockaddr_un address{};
    address.sun_family            = AF_UNIX;
    const std::string path_string = path.string();
    std::memcpy(address.sun_path, path_string.c_str(), path_string.size() + 1);
    return address;
}

} // namespace

TEST(PosixTest, CreatesConnectsListensAndAcceptsUnixSocket)
{
    Posix posix;
    const std::filesystem::path path = socket_path();
    static_cast<void>(posix.unlink(path.c_str()));
    const sockaddr_un address = make_address(path);
    const auto address_length =
        static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + std::strlen(address.sun_path) + 1);

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
    Posix posix;
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

TEST(PosixTest, PreservesNativeFailureResults)
{
    Posix posix;
    errno = 0;
    EXPECT_EQ(posix.close(-1), -1);
    EXPECT_EQ(errno, EBADF);

    errno = 0;
    EXPECT_EQ(posix.unlink("/path/that/does/not/exist"), -1);
    EXPECT_EQ(errno, ENOENT);
}
