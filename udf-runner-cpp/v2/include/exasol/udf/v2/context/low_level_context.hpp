#pragma once

#include <exasol/udf/v2/interface/context.hpp>
#include <exasol/udf/v2/context/background_io_thread.hpp>

#include <memory>

namespace exasol::udf::v2::context
{

/** Concrete worker-facing context for one low-level protocol connection. */
class LowLevelContext final : public interface::Context
{
public:
    explicit LowLevelContext(std::unique_ptr<BackgroundIoThreadInterface> background_thread);
    ~LowLevelContext() override;

    LowLevelContext(const LowLevelContext&)            = delete;
    LowLevelContext& operator=(const LowLevelContext&) = delete;
    LowLevelContext(LowLevelContext&&)                 = delete;
    LowLevelContext& operator=(LowLevelContext&&)      = delete;

    [[nodiscard]] interface::MessageStatus receive_status() const override;
    [[nodiscard]] bool is_new_inbound_call_available() const override;
    [[nodiscard]] interface::MessageStatus wait_for_message(interface::Timeout timeout) override;
    [[nodiscard]] interface::Result<std::unique_ptr<interface::Call>> accept_call() override;
    [[nodiscard]] interface::Result<std::unique_ptr<interface::Call>> open_call(
        interface::CallMessageBuilder opening_message) override;
    [[nodiscard]] interface::ControlStream& control_stream() override;

private:
    std::unique_ptr<BackgroundIoThreadInterface> background_thread_;
};

} // namespace exasol::udf::v2::context
