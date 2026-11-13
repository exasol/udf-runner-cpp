#pragma once

#include <exasol/udf/v2/internal/call_message_builder_impl.hpp>
#include <exasol/udf/v2/internal/control_message_builder_impl.hpp>

#include <cstdint>
#include <memory>
#include <utility>

namespace exasol::udf::v2::internal
{

/// Creates builders bound to a stream without exposing stream IDs to workers.
class MessageBuilderFactory
{
public:
    [[nodiscard]] static std::unique_ptr<interface::CallMessageBuilder> create_call_builder(
        std::uint64_t stream_id, std::weak_ptr<const void> owner = {})
    {
        return std::make_unique<CallMessageBuilderImpl>(stream_id, std::move(owner));
    }

    [[nodiscard]] static std::unique_ptr<interface::OpeningCallMessageBuilder>
    create_opening_call_builder(std::uint64_t stream_id, std::weak_ptr<const void> owner = {})
    {
        return std::make_unique<OpeningCallMessageBuilderImpl>(stream_id, std::move(owner));
    }

    [[nodiscard]] static std::unique_ptr<interface::ControlMessageBuilder> create_control_builder(
        std::weak_ptr<const void> owner = {})
    {
        return std::make_unique<ControlMessageBuilderImpl>(0, std::move(owner));
    }
};

} // namespace exasol::udf::v2::internal
