#pragma once

#include <memory>

#include <exasol/udf/v2/posix/posix.hpp>

namespace exasol::udf::v2::detail
{

class NativeEpollState final
{
public:
    explicit NativeEpollState(std::shared_ptr<posix::Posix> posix_facade);
    ~NativeEpollState();

    NativeEpollState(const NativeEpollState&)            = delete;
    NativeEpollState& operator=(const NativeEpollState&) = delete;
    NativeEpollState(NativeEpollState&&)                 = delete;
    NativeEpollState& operator=(NativeEpollState&&)      = delete;

    [[nodiscard]] int native_handle() const noexcept;
    [[nodiscard]] bool is_open() const noexcept;
    [[nodiscard]] std::shared_ptr<posix::Posix> posix_facade() const noexcept;
    void close() noexcept;

private:
    int file_descriptor = -1;
    std::shared_ptr<posix::Posix> posix;
};

} // namespace exasol::udf::v2::detail
