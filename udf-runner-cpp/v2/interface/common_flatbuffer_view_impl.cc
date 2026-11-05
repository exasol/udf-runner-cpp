#include <exasol/udf/v2/internal/common_flatbuffer_view_impl.hpp>

#include <utility>

namespace exasol::udf::v2::internal {
namespace {

class PayloadValueViewImpl final : public interface::PayloadValueView {
public:
  PayloadValueViewImpl(std::shared_ptr<const protocol::DecodedFrame> frame,
                       const protocol::Payload *value)
      : frame_(std::move(frame)), value_(value) {}
  interface::PayloadType type() const noexcept override {
    return value_ != nullptr && value_->payload_type() ==
                                    protocol::PayloadValue_StringPayload
               ? interface::PayloadType::String
               : interface::PayloadType::Binary;
  }
  std::optional<std::string_view> string_value() const noexcept override {
    const auto *value =
        value_ == nullptr ? nullptr : value_->payload_as_StringPayload();
    return flatbuffer_string_view(value == nullptr ? nullptr : value->value());
  }
  std::optional<std::span<const std::uint8_t>>
  binary_value() const noexcept override {
    const auto *value =
        value_ == nullptr ? nullptr : value_->payload_as_BinaryPayload();
    if (value == nullptr || value->value() == nullptr) {
      return std::nullopt;
    }
    return std::span<const std::uint8_t>(value->value()->Data(),
                                         value->value()->size());
  }

private:
  std::shared_ptr<const protocol::DecodedFrame> frame_;
  const protocol::Payload *value_;
};

class PayloadViewImpl final : public interface::PayloadView {
public:
  PayloadViewImpl(std::shared_ptr<const protocol::DecodedFrame> frame,
                  const protocol::Payload *value)
      : frame_(std::move(frame)), value_(value) {}
  std::optional<std::string_view> name() const noexcept override {
    return flatbuffer_string_view(value_ == nullptr ? nullptr : value_->name());
  }
  std::unique_ptr<interface::PayloadValueView> value() const override {
    if (value_ == nullptr || value_->payload() == nullptr) {
      return nullptr;
    }
    return std::make_unique<PayloadValueViewImpl>(frame_, value_);
  }

private:
  std::shared_ptr<const protocol::DecodedFrame> frame_;
  const protocol::Payload *value_;
};

class PayloadsViewImpl final : public interface::PayloadsView {
public:
  PayloadsViewImpl(std::shared_ptr<const protocol::DecodedFrame> frame,
                   const protocol::Payloads *value)
      : frame_(std::move(frame)), value_(value) {}
  std::size_t size() const noexcept override {
    return value_ == nullptr || value_->payloads() == nullptr
               ? 0
               : value_->payloads()->size();
  }
  std::unique_ptr<interface::PayloadView> at(std::size_t index) const override {
    if (value_ == nullptr || value_->payloads() == nullptr || index >= size()) {
      return nullptr;
    }
    return std::make_unique<PayloadViewImpl>(frame_,
                                             value_->payloads()->Get(index));
  }

private:
  std::shared_ptr<const protocol::DecodedFrame> frame_;
  const protocol::Payloads *value_;
};

class ErrorViewImpl final : public interface::ErrorView {
public:
  ErrorViewImpl(std::shared_ptr<const protocol::DecodedFrame> frame,
                const protocol::Error *value)
      : frame_(std::move(frame)), value_(value) {}
  std::optional<std::string_view> code() const noexcept override {
    return flatbuffer_string_view(value_ == nullptr ? nullptr : value_->code());
  }
  std::optional<std::string_view> message() const noexcept override {
    return flatbuffer_string_view(value_ == nullptr ? nullptr
                                                    : value_->message());
  }

private:
  std::shared_ptr<const protocol::DecodedFrame> frame_;
  const protocol::Error *value_;
};

} // namespace

std::optional<std::string_view>
flatbuffer_string_view(const third_party::flatbuffers::String *value) noexcept {
  return value == nullptr ? std::nullopt
                          : std::optional<std::string_view>(
                                std::string_view(value->data(), value->size()));
}

std::unique_ptr<interface::PayloadsView>
make_payloads_view(std::shared_ptr<const protocol::DecodedFrame> frame,
                   const protocol::Payloads *value) {
  return value == nullptr
             ? nullptr
             : std::make_unique<PayloadsViewImpl>(std::move(frame), value);
}

std::unique_ptr<interface::ErrorView>
make_error_view(std::shared_ptr<const protocol::DecodedFrame> frame,
                const protocol::Error *value) {
  return value == nullptr
             ? nullptr
             : std::make_unique<ErrorViewImpl>(std::move(frame), value);
}

} // namespace exasol::udf::v2::internal
