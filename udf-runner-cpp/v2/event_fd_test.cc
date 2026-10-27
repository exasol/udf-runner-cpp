#include <unistd.h>

#include <sys/eventfd.h>

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <system_error>
#include <utility>
#include <vector>

#include <exasol/udf/v2/event_fd.hpp>
#include <exasol/udf/v2/event_fd_factory.hpp>
#include <exasol/udf/v2/linux_event_fd.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace
{

class MockPosix : public exasol::udf::v2::posix::Posix // NOSONAR: GMock mock implements the
                                                       // complete POSIX interface
{
public:
    MOCK_METHOD(int, socket, (int, int, int), (override));
    MOCK_METHOD(int, eventfd, (unsigned int, int), (override));
    MOCK_METHOD(int, connect, (int, const sockaddr*, socklen_t), (override));
    MOCK_METHOD(int, bind, (int, const sockaddr*, socklen_t), (override));
    MOCK_METHOD(int, listen, (int, int), (override));
    MOCK_METHOD(int, accept4, (int, sockaddr*, socklen_t*, int), (override));
    MOCK_METHOD(int,
                getsockopt,
                (int, int, int, void*, socklen_t*), // NOSONAR
                (override));
    MOCK_METHOD(int, shutdown, (int, int), (override));
    MOCK_METHOD(ssize_t, readv, (int, const iovec*, int), (override));
    MOCK_METHOD(ssize_t, read, (int, void*, size_t), (override));        // NOSONAR
    MOCK_METHOD(ssize_t, write, (int, const void*, size_t), (override)); // NOSONAR
    MOCK_METHOD(ssize_t, sendmsg, (int, const msghdr*, int), (override));
    MOCK_METHOD(int, unlink, (const char*), (override));
    MOCK_METHOD(int, close, (int), (override));
};

class RecordingPosix final : public exasol::udf::v2::posix::Posix
{
public:
    int socket(int domain, int type, int protocol) override
    {
        return native.socket(domain, type, protocol);
    }

    int connect(int sockfd, const sockaddr* address, socklen_t address_length) override
    {
        return native.connect(sockfd, address, address_length);
    }

    int bind(int sockfd, const sockaddr* address, socklen_t address_length) override
    {
        return native.bind(sockfd, address, address_length);
    }

    int listen(int sockfd, int backlog) override
    {
        return native.listen(sockfd, backlog);
    }

    int accept4(int sockfd, sockaddr* address, socklen_t* address_length, int flags) override
    {
        return native.accept4(sockfd, address, address_length, flags);
    }

    int getsockopt(int sockfd,
                   int level,
                   int option_name,
                   void* option_value, // NOSONAR: required by Posix::getsockopt interface
                   socklen_t* option_length) override
    {
        return native.getsockopt(sockfd, level, option_name, option_value, option_length);
    }

    int shutdown(int sockfd, int how) override
    {
        return native.shutdown(sockfd, how);
    }

    ssize_t readv(int file_descriptor, const iovec* buffers, int buffer_count) override
    {
        return native.readv(file_descriptor, buffers, buffer_count);
    }

    ssize_t sendmsg(int sockfd, const msghdr* message, int flags) override
    {
        return native.sendmsg(sockfd, message, flags);
    }

    int unlink(const char* path) override
    {
        return native.unlink(path);
    }

    int eventfd(unsigned int initial_value, int flags) override
    {
        eventfd_initial_value = initial_value;
        eventfd_flags         = flags;
        ++eventfd_calls;
        return eventfd_result;
    }

    ssize_t read(int file_descriptor,
                 void* buffer, // NOSONAR: required by the POSIX read interface
                 size_t count) override
    {
        read_file_descriptor = file_descriptor;
        read_count           = count;
        ++read_calls;
        if (read_eintr_count > 0)
        {
            --read_eintr_count;
            errno = EINTR;
            return -1;
        }
        if (read_result == static_cast<ssize_t>(sizeof(std::uint64_t)))
        {
            std::memcpy(buffer, &read_value, sizeof(read_value));
        }
        return read_result;
    }

    ssize_t write(int file_descriptor,
                  const void* buffer, // NOSONAR: required by the POSIX write interface
                  size_t count) override
    {
        write_file_descriptor = file_descriptor;
        write_count           = count;
        ++write_calls;
        if (write_eintr_count > 0)
        {
            --write_eintr_count;
            errno = EINTR;
            return -1;
        }
        if (count == sizeof(write_value))
        {
            std::memcpy(&write_value, buffer, sizeof(write_value));
        }
        return write_result;
    }

    int close(int file_descriptor) override
    {
        closed_file_descriptors.push_back(file_descriptor);
        return close_result;
    }

    void set_read_value(const std::uint64_t value)
    {
        read_value = value;
    }

    void set_read_eintr_count(const int count)
    {
        read_eintr_count = count;
    }

    void set_write_eintr_count(const int count)
    {
        write_eintr_count = count;
    }

    [[nodiscard]] int eventfd_call_count() const
    {
        return eventfd_calls;
    }
    [[nodiscard]] unsigned int eventfd_initial() const
    {
        return eventfd_initial_value;
    }
    [[nodiscard]] int eventfd_flag_value() const
    {
        return eventfd_flags;
    }
    [[nodiscard]] int read_call_count() const
    {
        return read_calls;
    }
    [[nodiscard]] int read_descriptor() const
    {
        return read_file_descriptor;
    }
    [[nodiscard]] size_t read_size() const
    {
        return read_count;
    }
    [[nodiscard]] int write_call_count() const
    {
        return write_calls;
    }
    [[nodiscard]] int write_descriptor() const
    {
        return write_file_descriptor;
    }
    [[nodiscard]] size_t write_size() const
    {
        return write_count;
    }
    [[nodiscard]] std::uint64_t written_value() const
    {
        return write_value;
    }
    [[nodiscard]] const std::vector<int>& closed_descriptors() const
    {
        return closed_file_descriptors;
    }

private:
    exasol::udf::v2::posix::Native native;
    int eventfd_result                 = 42;
    int eventfd_calls                  = 0;
    unsigned int eventfd_initial_value = 0;
    int eventfd_flags                  = 0;
    ssize_t read_result                = sizeof(std::uint64_t);
    int read_calls                     = 0;
    int read_eintr_count               = 0;
    int read_file_descriptor           = -1;
    size_t read_count                  = 0;
    std::uint64_t read_value           = 0;
    ssize_t write_result               = sizeof(std::uint64_t);
    int write_calls                    = 0;
    int write_eintr_count              = 0;
    int write_file_descriptor          = -1;
    size_t write_count                 = 0;
    std::uint64_t write_value          = 0;
    int close_result                   = 0;
    std::vector<int> closed_file_descriptors;
};

template <typename Function>
void expect_system_error(Function&& function, std::errc expected, const char* message)
{
    try
    {
        function();
        ADD_FAILURE() << message;
    }
    catch (const std::system_error& error)
    {
        EXPECT_EQ(error.code(), std::make_error_code(expected));
    }
}

template <typename Type>
void self_move_assign(Type& value)
{
    using move_assignment        = Type& (Type::*)(Type&&) noexcept;
    const move_assignment assign = &Type::operator=;
    (value.*assign)(std::move(value));
}

void expect_closed_descriptor(int file_descriptor)
{
    errno = 0;
    EXPECT_EQ(::close(file_descriptor), -1);
    EXPECT_EQ(errno, EBADF);
}

} // namespace

TEST(EventFdTest, UsesInjectedPosixForCreationAndIo)
{
    RecordingPosix posix;
    posix.set_read_value(7);

    exasol::udf::v2::LinuxEventFd event_fd(posix);
    EXPECT_EQ(posix.eventfd_call_count(), 1);
    EXPECT_EQ(posix.eventfd_initial(), 0);
    EXPECT_EQ(posix.eventfd_flag_value(), EFD_NONBLOCK | EFD_CLOEXEC);

    event_fd.write_notification();
    EXPECT_EQ(posix.write_call_count(), 1);
    EXPECT_EQ(posix.write_descriptor(), 42);
    EXPECT_EQ(posix.write_size(), sizeof(std::uint64_t));
    EXPECT_EQ(posix.written_value(), 1);

    EXPECT_EQ(event_fd.read_notification(), 7);
    EXPECT_EQ(posix.read_call_count(), 1);
    EXPECT_EQ(posix.read_descriptor(), 42);
    EXPECT_EQ(posix.read_size(), sizeof(std::uint64_t));
}

TEST(EventFdTest, RetriesNotificationIoAfterInterruptedCalls)
{
    RecordingPosix posix;
    posix.set_read_value(7);
    posix.set_read_eintr_count(1);
    posix.set_write_eintr_count(1);

    exasol::udf::v2::LinuxEventFd event_fd(posix);
    event_fd.write_notification();
    EXPECT_EQ(event_fd.read_notification(), 7);
    EXPECT_EQ(posix.write_call_count(), 2);
    EXPECT_EQ(posix.read_call_count(), 2);
}

TEST(EventFdTest, FactoryUsesInjectedPosix)
{
    RecordingPosix posix;
    auto event_fd = exasol::udf::v2::make_linux_event_fd(posix);
    ASSERT_NE(event_fd, nullptr);
    EXPECT_EQ(event_fd->native_handle(), 42);
    EXPECT_EQ(posix.eventfd_call_count(), 1);
}

TEST(EventFdTest, FactoryUsesMockPosix)
{
    testing::StrictMock<MockPosix> posix;
    EXPECT_CALL(posix, eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)).WillOnce(testing::Return(42));
    EXPECT_CALL(posix, close(42)).WillOnce(testing::Return(0));

    auto event_fd = exasol::udf::v2::make_linux_event_fd(posix);
    ASSERT_NE(event_fd, nullptr);
    EXPECT_EQ(event_fd->native_handle(), 42);
}

TEST(EventFdTest, InjectedPosixClosesOnDestructionAndMoveAssignment)
{
    RecordingPosix posix;
    {
        exasol::udf::v2::LinuxEventFd source(posix);
        exasol::udf::v2::LinuxEventFd destination(posix);
        destination = std::move(source);
        ASSERT_EQ(posix.closed_descriptors(), std::vector<int>{42});
    }
    EXPECT_EQ(posix.closed_descriptors(), (std::vector<int>{42, 42}));
}

TEST(EventFdTest, AccumulatesNotifications)
{
    exasol::udf::v2::posix::Native posix;
    exasol::udf::v2::LinuxEventFd event_fd(posix);
    ASSERT_NE(event_fd.native_handle(), -1);

    event_fd.write_notification();
    event_fd.write_notification();
    EXPECT_EQ(event_fd.read_notification(), 2);
}

TEST(EventFdTest, FactoryCreatesLinuxEventFd)
{
    exasol::udf::v2::posix::Native posix;
    auto event_fd = exasol::udf::v2::make_linux_event_fd(posix);
    ASSERT_NE(event_fd, nullptr);
    ASSERT_NE(event_fd->native_handle(), -1);
    event_fd->write_notification();
    EXPECT_EQ(event_fd->read_notification(), 1);
}

TEST(EventFdTest, DefaultFactoryCreatesLinuxEventFd)
{
    auto event_fd = exasol::udf::v2::make_linux_event_fd();
    ASSERT_NE(event_fd, nullptr);
    ASSERT_NE(event_fd->native_handle(), -1);
    event_fd->write_notification();
    EXPECT_EQ(event_fd->read_notification(), 1);
}

TEST(EventFdTest, DefaultConstructorCreatesLinuxEventFd)
{
    exasol::udf::v2::LinuxEventFd event_fd;
    ASSERT_NE(event_fd.native_handle(), -1);
    event_fd.write_notification();
    EXPECT_EQ(event_fd.read_notification(), 1);
}

TEST(EventFdTest, RejectsReadWhenEmpty)
{
    exasol::udf::v2::posix::Native posix;
    exasol::udf::v2::LinuxEventFd event_fd(posix);
    expect_system_error([&event_fd] { event_fd.read_notification(); },
                        std::errc::resource_unavailable_try_again,
                        "empty eventfd read should report EAGAIN");
}

TEST(EventFdTest, SupportsMoveConstruction)
{
    exasol::udf::v2::posix::Native posix;
    exasol::udf::v2::LinuxEventFd event_fd(posix);
    const int moved_handle = event_fd.native_handle();
    exasol::udf::v2::LinuxEventFd move_constructed(std::move(event_fd));
    EXPECT_EQ(move_constructed.native_handle(), moved_handle);
}

TEST(EventFdTest, SupportsMoveAssignmentAndSelfMove)
{
    exasol::udf::v2::posix::Native posix;
    exasol::udf::v2::LinuxEventFd source(posix);
    const int moved_handle = source.native_handle();
    exasol::udf::v2::LinuxEventFd move_assigned(posix);
    move_assigned = std::move(source);
    EXPECT_EQ(move_assigned.native_handle(), moved_handle);
    self_move_assign(move_assigned);
    EXPECT_EQ(move_assigned.native_handle(), moved_handle);
}

TEST(EventFdTest, MoveAssignmentClosesReplacedDescriptor)
{
    exasol::udf::v2::posix::Native posix;
    exasol::udf::v2::LinuxEventFd source(posix);
    exasol::udf::v2::LinuxEventFd destination(posix);
    const int replaced_handle = destination.native_handle();

    destination = std::move(source);

    expect_closed_descriptor(replaced_handle);
}

TEST(EventFdTest, DestructorClosesDescriptor)
{
    int destroyed_handle = -1;
    exasol::udf::v2::posix::Native posix;
    {
        exasol::udf::v2::LinuxEventFd event_fd(posix);
        destroyed_handle = event_fd.native_handle();
    }

    expect_closed_descriptor(destroyed_handle);
}

TEST(EventFdTest, RejectsReadOnClosedDescriptor)
{
    exasol::udf::v2::posix::Native posix;
    exasol::udf::v2::LinuxEventFd closed_event_fd(posix);
    ASSERT_EQ(::close(closed_event_fd.native_handle()), 0);
    expect_system_error([&closed_event_fd] { closed_event_fd.read_notification(); },
                        std::errc::bad_file_descriptor, "closed eventfd read should be rejected");
}

TEST(EventFdTest, RejectsWriteOnClosedDescriptor)
{
    exasol::udf::v2::posix::Native posix;
    exasol::udf::v2::LinuxEventFd closed_event_fd(posix);
    ASSERT_EQ(::close(closed_event_fd.native_handle()), 0);
    expect_system_error([&closed_event_fd] { closed_event_fd.write_notification(); },
                        std::errc::bad_file_descriptor, "closed eventfd write should be rejected");
}
