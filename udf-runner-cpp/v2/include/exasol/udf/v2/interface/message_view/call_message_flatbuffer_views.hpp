#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace exasol::udf::v2::interface {

class OpenCallView {
public:
  virtual ~OpenCallView() = default;
  [[nodiscard]] virtual std::optional<std::string_view>
  call_name() const noexcept = 0;

protected:
  OpenCallView() = default;
};

class ResetView {
public:
  virtual ~ResetView() = default;
  [[nodiscard]] virtual std::uint64_t row_id() const noexcept = 0;

protected:
  ResetView() = default;
};

class CloseCallView {
public:
  virtual ~CloseCallView() = default;

protected:
  CloseCallView() = default;
};

} // namespace exasol::udf::v2::interface
