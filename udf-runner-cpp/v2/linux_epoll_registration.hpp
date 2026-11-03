#pragma once

#include <cstdint>
#include <memory>

#include <exasol/udf/v2/linux_epoll.hpp>

#include "linux_epoll_state.hpp"

namespace exasol::udf::v2::detail
{

class NativeEpollRegistration final : public EpollRegistration
{
public:
    NativeEpollRegistration(std::shared_ptr<NativeEpollState> state,
                            int file_descriptor,
                            EpollInterest interest,
                            std::uint64_t token);
    ~NativeEpollRegistration() override;

    NativeEpollRegistration(const NativeEpollRegistration&)            = delete;
    NativeEpollRegistration& operator=(const NativeEpollRegistration&) = delete;
    NativeEpollRegistration(NativeEpollRegistration&&)                 = delete;
    NativeEpollRegistration& operator=(NativeEpollRegistration&&)      = delete;

    void modify(EpollInterest interest, std::uint64_t token) override;

private:
    std::weak_ptr<NativeEpollState> state;
    int file_descriptor = -1;
};

} // namespace exasol::udf::v2::detail
