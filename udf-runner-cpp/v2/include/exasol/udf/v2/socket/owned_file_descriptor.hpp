#pragma once

namespace exasol::udf::v2::socket
{

class OwnedFileDescriptor
{
public:
    OwnedFileDescriptor() noexcept;
    ~OwnedFileDescriptor();

    [[nodiscard]] static OwnedFileDescriptor adopt_native_handle(int owned_fd);

    OwnedFileDescriptor(const OwnedFileDescriptor&)            = delete;
    OwnedFileDescriptor& operator=(const OwnedFileDescriptor&) = delete;
    OwnedFileDescriptor(OwnedFileDescriptor&& other) noexcept;
    OwnedFileDescriptor& operator=(OwnedFileDescriptor&& other) noexcept;

    [[nodiscard]] int native_handle() const noexcept;
    [[nodiscard]] bool is_open() const noexcept;
    void close() noexcept;
    [[nodiscard]] int release_native_handle() noexcept;

private:
    explicit OwnedFileDescriptor(int owned_fd) noexcept;

    int file_descriptor = -1;
};

} // namespace exasol::udf::v2::socket
