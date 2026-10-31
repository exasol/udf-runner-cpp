#pragma once

#include <exasol/udf/v2/interface/content_types.hpp>
#include <exasol/udf/v2/interface/error_info.hpp>
#include <exasol/udf/v2/interface/message_builder/control_message_builder.hpp>

#include <optional>

namespace exasol::udf::v2::internal
{

/// Internal mutable representation consumed by the protocol encoder.
struct ControlMessage
{
    explicit ControlMessage(std::uint64_t stream_id) : stream_id(stream_id)
    {
    }

    std::uint64_t stream_id;
    bool keep_alive{false}; // NOLINT(misc-non-private-member-variables-in-classes)
    std::optional<interface::ErrorInfo>
        error; // NOLINT(misc-non-private-member-variables-in-classes)
    bool close_connection{false}; // NOLINT(misc-non-private-member-variables-in-classes)
};

class ControlMessageBuilderImpl final : public interface::ControlMessageBuilder
{
public:
    using interface::ControlMessageBuilder::add_payload;

    explicit ControlMessageBuilderImpl(std::uint64_t stream_id);

    interface::ControlMessageBuilder& server_capabilities(
        std::uint32_t major,
        std::uint32_t minor,
        std::endian endianness,
        std::uint32_t number_of_supported_workers) override;
    interface::ControlMessageBuilder& keep_alive() override;
    interface::ControlMessageBuilder& add_payload(std::string_view name,
                                                  std::string_view value,
                                                  bool binary) override;
    interface::ControlMessageBuilder& error(std::string_view code, std::string_view text) override;
    interface::ControlMessageBuilder& close_connection() override;

    [[nodiscard]] const ControlMessage& message() const noexcept;

private:
    ControlMessage message_;
};

} // namespace exasol::udf::v2::internal
