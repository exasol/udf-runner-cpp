#pragma once

#include <exasol/udf/v2/interface/content_types.hpp>
#include <exasol/udf/v2/interface/message_builder/control_message_builder.hpp>

namespace exasol::udf::v2::internal
{

/// Internal, non-owning representation consumed by the protocol encoder.
struct ControlMessageEncodingView
{
    std::uint64_t stream_id;
    const interface::ControlMessage& message;
};

class ControlMessageBuilderImpl final : public interface::ControlMessageBuilder
{
public:
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

    [[nodiscard]] ControlMessageEncodingView encoding_view() const noexcept;

private:
    std::uint64_t stream_id_;
    interface::ControlMessage message_;
};

} // namespace exasol::udf::v2::internal
