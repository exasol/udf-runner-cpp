#pragma once

#include <cstdint>
#include <memory>

namespace exasol::udf::v2::interface {

class VersionView {
public:
  virtual ~VersionView() = default;
  [[nodiscard]] virtual std::uint32_t major() const noexcept = 0;
  [[nodiscard]] virtual std::uint32_t minor() const noexcept = 0;

protected:
  VersionView() = default;
};

enum class Endianness { Little, Big };

class ServerCapabilitiesView {
public:
  virtual ~ServerCapabilitiesView() = default;
  [[nodiscard]] virtual std::unique_ptr<VersionView>
  supported_version() const = 0;
  [[nodiscard]] virtual Endianness endianness() const noexcept = 0;
  [[nodiscard]] virtual std::uint32_t
  number_of_supported_workers() const noexcept = 0;

protected:
  ServerCapabilitiesView() = default;
};

class KeepAliveView {
public:
  virtual ~KeepAliveView() = default;

protected:
  KeepAliveView() = default;
};

class CloseConnectionView {
public:
  virtual ~CloseConnectionView() = default;

protected:
  CloseConnectionView() = default;
};

} // namespace exasol::udf::v2::interface
