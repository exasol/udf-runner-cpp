#include <exasol/udf/v2/context/high_level_context.hpp>

#include <bit>
#include <utility>

namespace exasol::udf::v2::context
{

interface::Result<void> HighLevelContext::initialize(const std::uint32_t workers)
{
    if (workers == 0)
    {
        return interface::Result<void>::failure(
            interface::OperationStatus::ProtocolError,
            interface::ErrorInfo{"invalid_worker_count",
                                 "number of supported workers must be positive"});
    }

    interface::ControlMessageBuilder message;
    message.server_capabilities(2, 0, std::endian::native, workers)
        .add_payload("high_level_name", protocol_name)
        .add_payload("high_level_version", protocol_version);
    return context_.control_stream().send(std::move(message));
}

interface::Result<void> HighLevelContext::send_metadata(const std::string_view name,
                                                        const std::string_view value,
                                                        const bool binary)
{
    interface::ControlMessageBuilder message;
    message.add_payload(name, value, binary);
    return context_.control_stream().send(std::move(message));
}

interface::Result<std::unique_ptr<interface::Call>> HighLevelContext::open_call(
    const std::string_view call_name, const interface::Payloads& payloads)
{
    interface::CallMessageBuilder message;
    message.open_call(call_name);
    for (const auto& payload : payloads)
    {
        message.add_payload(payload.name, payload.value, payload.binary);
    }
    return context_.open_call(std::move(message));
}

} // namespace exasol::udf::v2::context
