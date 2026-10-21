#pragma once

#include <exasol/udf/v2/interface/context.hpp>
#include <exasol/udf/v2/socket/owned_file_descriptor.hpp>

#include <cstdint>
#include <memory>

namespace exasol::udf::v2::context
{

/**
 * Abstracts the background I/O owner used by a worker-facing context.
 *
 * The worker-facing context remains responsible for the public lifetime and
 * operation contract. This interface is the construction seam for replacing
 * the production thread with a deterministic test backend.
 */
class BackgroundIoThreadInterface
{
public:
    virtual ~BackgroundIoThreadInterface() = default;

    BackgroundIoThreadInterface(const BackgroundIoThreadInterface&)            = delete;
    BackgroundIoThreadInterface& operator=(const BackgroundIoThreadInterface&) = delete;
    BackgroundIoThreadInterface(BackgroundIoThreadInterface&&)                 = delete;
    BackgroundIoThreadInterface& operator=(BackgroundIoThreadInterface&&)      = delete;

    virtual void start()                 = 0;
    virtual void request_stop() noexcept = 0;
    virtual void join() noexcept         = 0;

    [[nodiscard]] virtual interface::MessageStatus receive_status() const                       = 0;
    [[nodiscard]] virtual bool is_new_inbound_call_available() const                            = 0;
    [[nodiscard]] virtual interface::MessageStatus wait_for_message(interface::Timeout timeout) = 0;
    [[nodiscard]] virtual interface::Result<std::unique_ptr<interface::Call>> accept_call()     = 0;
    [[nodiscard]] virtual interface::Result<std::unique_ptr<interface::Call>> open_call(
        interface::CallMessageBuilder opening_message)                                  = 0;
    [[nodiscard]] virtual interface::Result<void> send_call(std::uint64_t stream_id,
                                                            interface::CallMessageBuilder message,
                                                            interface::Timeout timeout) = 0;
    [[nodiscard]] virtual interface::Result<void> send_control(
        interface::ControlMessageBuilder message)                    = 0;
    [[nodiscard]] virtual interface::ControlStream& control_stream() = 0;

protected:
    BackgroundIoThreadInterface() = default;
};

/** Creates one background I/O owner for an accepted connection. */
class BackgroundIoThreadFactory
{
public:
    virtual ~BackgroundIoThreadFactory() = default;

    BackgroundIoThreadFactory()                                            = default;
    BackgroundIoThreadFactory(const BackgroundIoThreadFactory&)            = delete;
    BackgroundIoThreadFactory& operator=(const BackgroundIoThreadFactory&) = delete;
    BackgroundIoThreadFactory(BackgroundIoThreadFactory&&)                 = delete;
    BackgroundIoThreadFactory& operator=(BackgroundIoThreadFactory&&)      = delete;

    [[nodiscard]] virtual std::unique_ptr<BackgroundIoThreadInterface> create(
        socket::OwnedFileDescriptor connection) = 0;
};

[[nodiscard]] std::unique_ptr<BackgroundIoThreadInterface> make_background_io_thread(
    socket::OwnedFileDescriptor connection);

} // namespace exasol::udf::v2::context
