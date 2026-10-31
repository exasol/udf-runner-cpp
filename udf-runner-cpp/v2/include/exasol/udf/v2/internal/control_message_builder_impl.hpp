#pragma once

#include <exasol/udf/v2/interface/content_types.hpp>
#include <exasol/udf/v2/interface/error_info.hpp>
#include <exasol/udf/v2/interface/message_builder/control_message_builder.hpp>

#include <bit>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace exasol::udf::v2::internal
{

/// Internal mutable representation consumed by the protocol encoder.
class ControlMessage
{
public:
    explicit ControlMessage(std::uint64_t stream_id);

    void server_capabilities(std::uint32_t major,
                             std::uint32_t minor,
                             std::endian endianness,
                             std::uint32_t number_of_supported_workers);
    void keep_alive();
    void add_payload(std::string_view name, std::string_view value, bool binary);
    void error(std::string_view code, std::string_view text);
    void close_connection();

    [[nodiscard]] std::uint64_t stream_id() const noexcept;
    [[nodiscard]] const std::optional<interface::ServerCapabilities>& server_capabilities()
        const noexcept;
    [[nodiscard]] bool is_keep_alive() const noexcept;
    [[nodiscard]] const std::vector<interface::Payload>& payloads() const noexcept;
    [[nodiscard]] const std::optional<interface::ErrorInfo>& error() const noexcept;
    [[nodiscard]] bool is_close_connection() const noexcept;

private:
    std::uint64_t stream_id_;
    std::optional<interface::ServerCapabilities> server_capabilities_;
    bool keep_alive_{false};
    std::vector<interface::Payload> payloads_;
    std::optional<interface::ErrorInfo> error_;
    bool close_connection_{false};
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
