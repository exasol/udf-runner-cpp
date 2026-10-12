#pragma once

namespace exasol::udf::v2::socket
{

/** Owns a native file descriptor and closes it on destruction. */
class OwnedFileDescriptor
{
public:
    /** Creates an empty descriptor that does not own a native handle. */
    OwnedFileDescriptor() noexcept;

    /** Closes the owned descriptor, if any. */
    ~OwnedFileDescriptor();

    /** Takes ownership of an open native file descriptor.
     *
     * @throws std::invalid_argument if @p owned_fd is negative.
     */
    [[nodiscard]] static OwnedFileDescriptor adopt_native_handle(int owned_fd);

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
    explicit OwnedFileDescriptor(int owned_fd) noexcept;

    int file_descriptor = -1;
};

} // namespace exasol::udf::v2::socket
