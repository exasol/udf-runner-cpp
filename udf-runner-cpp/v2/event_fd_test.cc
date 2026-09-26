#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <system_error>
#include <utility>

#include <exasol/udf/v2/event_fd.hpp>
#include <exasol/udf/v2/event_fd_factory.hpp>
#include <exasol/udf/v2/linux_event_fd.hpp>
#include <gtest/gtest.h>

namespace
{

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

TEST(EventFdTest, AccumulatesNotifications)
{
    exasol::udf::v2::LinuxEventFd event_fd;
    ASSERT_NE(event_fd.native_handle(), -1);

    event_fd.write_notification();
    event_fd.write_notification();
    EXPECT_EQ(event_fd.read_notification(), 2);
}

TEST(EventFdTest, FactoryCreatesLinuxEventFd)
{
    auto event_fd = exasol::udf::v2::make_linux_event_fd();
    ASSERT_NE(event_fd, nullptr);
    ASSERT_NE(event_fd->native_handle(), -1);
    event_fd->write_notification();
    EXPECT_EQ(event_fd->read_notification(), 1);
}

TEST(EventFdTest, RejectsReadWhenEmpty)
{
    exasol::udf::v2::LinuxEventFd event_fd;
    expect_system_error([&event_fd] { event_fd.read_notification(); },
                        std::errc::resource_unavailable_try_again,
                        "empty eventfd read should report EAGAIN");
}

TEST(EventFdTest, SupportsMoveConstruction)
{
    exasol::udf::v2::LinuxEventFd event_fd;
    const int moved_handle = event_fd.native_handle();
    exasol::udf::v2::LinuxEventFd move_constructed(std::move(event_fd));
    EXPECT_EQ(move_constructed.native_handle(), moved_handle);
}

TEST(EventFdTest, SupportsMoveAssignmentAndSelfMove)
{
    exasol::udf::v2::LinuxEventFd source;
    const int moved_handle = source.native_handle();
    exasol::udf::v2::LinuxEventFd move_assigned;
    move_assigned = std::move(source);
    EXPECT_EQ(move_assigned.native_handle(), moved_handle);
    self_move_assign(move_assigned);
    EXPECT_EQ(move_assigned.native_handle(), moved_handle);
}

TEST(EventFdTest, MoveAssignmentClosesReplacedDescriptor)
{
    exasol::udf::v2::LinuxEventFd source;
    exasol::udf::v2::LinuxEventFd destination;
    const int replaced_handle = destination.native_handle();

    destination = std::move(source);

    expect_closed_descriptor(replaced_handle);
}

TEST(EventFdTest, DestructorClosesDescriptor)
{
    int destroyed_handle = -1;
    {
        exasol::udf::v2::LinuxEventFd event_fd;
        destroyed_handle = event_fd.native_handle();
    }

    expect_closed_descriptor(destroyed_handle);
}

TEST(EventFdTest, RejectsReadOnClosedDescriptor)
{
    exasol::udf::v2::LinuxEventFd closed_event_fd;
    ASSERT_EQ(::close(closed_event_fd.native_handle()), 0);
    expect_system_error([&closed_event_fd] { closed_event_fd.read_notification(); },
                        std::errc::bad_file_descriptor, "closed eventfd read should be rejected");
}

TEST(EventFdTest, RejectsWriteOnClosedDescriptor)
{
    exasol::udf::v2::LinuxEventFd closed_event_fd;
    ASSERT_EQ(::close(closed_event_fd.native_handle()), 0);
    expect_system_error([&closed_event_fd] { closed_event_fd.write_notification(); },
                        std::errc::bad_file_descriptor, "closed eventfd write should be rejected");
}
