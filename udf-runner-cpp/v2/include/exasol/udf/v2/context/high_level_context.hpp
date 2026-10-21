#pragma once

#include <exasol/udf/v2/interface/context.hpp>

#include <cstdint>
#include <memory>
#include <string_view>

namespace exasol::udf::v2::context
{

inline constexpr std::string_view protocol_name    = "exasol.udf";
inline constexpr std::string_view protocol_version = "2.0-dev";

/** High-level protocol facade built on a worker-facing low-level context. */
class HighLevelContext final
{
public:
    explicit HighLevelContext(interface::Context& context) noexcept : context_(context)
    {
    }

    /** Advertises the high-level protocol and low-level capabilities. */
    [[nodiscard]] interface::Result<void> initialize(std::uint32_t number_of_supported_workers);

    /** Sends or replaces one connection-scoped metadata payload. */
    [[nodiscard]] interface::Result<void> send_metadata(std::string_view name,
                                                        std::string_view value,
                                                        bool binary = false);

    /** Opens a named high-level call with its opening payloads. */
    [[nodiscard]] interface::Result<std::unique_ptr<interface::Call>> open_call(
        std::string_view call_name, const interface::Payloads& payloads = {});

private:
    interface::Context& context_;
};

} // namespace exasol::udf::v2::context
