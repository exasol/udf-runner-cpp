#pragma once

#include <exasol/udf/v2/internal/call_message_builder_impl.hpp>
#include <exasol/udf/v2/internal/control_message_builder_impl.hpp>

#include <cstdint>
#include <memory>

namespace exasol::udf::v2::internal
{

/// Creates builders bound to a stream without exposing stream IDs to workers.
class MessageBuilderFactory
{
public:
    [[nodiscard]] static std::unique_ptr<interface::CallMessageBuilder> create_call_builder(
        std::uint64_t stream_id)
    {
        return std::make_unique<CallMessageBuilderImpl>(stream_id);
    }

    [[nodiscard]] static std::unique_ptr<interface::OpeningCallMessageBuilder>
    create_opening_call_builder(std::uint64_t stream_id)
    {
        return std::make_unique<OpeningCallMessageBuilderImpl>(stream_id);
    }

    [[nodiscard]] static std::unique_ptr<interface::ControlMessageBuilder> create_control_builder()
    {
        return std::make_unique<ControlMessageBuilderImpl>(0);
    }
};

} // namespace exasol::udf::v2::internal
