#pragma once

#include <exasol/udf/v2/posix/posix.hpp>

namespace exasol::udf::v2::socket
{

/** Owns a native file descriptor and closes it on destruction. */
class OwnedFileDescriptor
{
public:
    /** Creates an empty descriptor that does not own a native handle. */
    explicit OwnedFileDescriptor(posix::Posix& posix) noexcept;

    /** Closes the owned descriptor, if any. */
    ~OwnedFileDescriptor() noexcept;

    /** Takes ownership of an open native file descriptor using @p posix. */
    [[nodiscard]] static OwnedFileDescriptor adopt_native_handle(int owned_fd, posix::Posix& posix);

    OwnedFileDescriptor(const OwnedFileDescriptor&)            = delete;
    OwnedFileDescriptor& operator=(const OwnedFileDescriptor&) = delete;
    OwnedFileDescriptor(OwnedFileDescriptor&& other) noexcept;
    OwnedFileDescriptor& operator=(OwnedFileDescriptor&& other) noexcept;

    /** Returns the owned native descriptor, or -1 when empty. */
    [[nodiscard]] int native_handle() const noexcept;

    /** Returns whether this object owns a native descriptor. */
    [[nodiscard]] bool is_open() const noexcept;

    /** Closes the owned descriptor and becomes empty. */
    void close() noexcept;

    /** Releases ownership and returns the native descriptor without closing it. */
    [[nodiscard]] int release_native_handle() noexcept;

private:
    explicit OwnedFileDescriptor(int owned_fd, posix::Posix& posix) noexcept;

    [[nodiscard]] posix::Posix& posix_facade() const noexcept;

    friend class UnixSocket;
    friend class UnixSocketListener;

    int m_file_descriptor         = -1;
    posix::Posix* m_posix_pointer = nullptr;
};

} // namespace exasol::udf::v2::socket
