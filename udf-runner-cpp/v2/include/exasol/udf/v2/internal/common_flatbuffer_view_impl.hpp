#pragma once

#include <exasol/udf/v2/interface/message_view/common_flatbuffer_views.hpp>
#include <exasol/udf/v2/udf_protocol.hpp>

#include <memory>
#include <optional>
#include <string_view>
#include <utility>

namespace exasol::udf::v2::internal {

std::optional<std::string_view>
flatbuffer_string_view(const third_party::flatbuffers::String *value) noexcept;

std::unique_ptr<interface::PayloadsView>
make_payloads_view(std::shared_ptr<const protocol::DecodedFrame> frame,
                   const protocol::Payloads *value);
std::unique_ptr<interface::ErrorView>
make_error_view(std::shared_ptr<const protocol::DecodedFrame> frame,
                const protocol::Error *value);

template <typename Interface, typename Value>
class MarkerViewImpl final : public Interface {
public:
  MarkerViewImpl(std::shared_ptr<const protocol::DecodedFrame> frame,
                 const Value *)
      : frame_(std::move(frame)) {}

private:
  std::shared_ptr<const protocol::DecodedFrame> frame_;
};

template <typename Interface, typename Value>
std::unique_ptr<Interface>
make_marker_view(std::shared_ptr<const protocol::DecodedFrame> frame,
                 const Value *value) {
  return value == nullptr ? nullptr
                          : std::make_unique<MarkerViewImpl<Interface, Value>>(
                                std::move(frame), value);
}

} // namespace exasol::udf::v2::internal
