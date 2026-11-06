#pragma once

#include <cstdint>
#include <memory>

namespace exasol::udf::v2::interface
{

class VersionView
{
public:
    virtual ~VersionView()                                     = default;
    VersionView(const VersionView&)                            = delete;
    VersionView& operator=(const VersionView&)                 = delete;
    VersionView(VersionView&&)                                 = delete;
    VersionView& operator=(VersionView&&)                      = delete;
    [[nodiscard]] virtual std::uint32_t major() const noexcept = 0;
    [[nodiscard]] virtual std::uint32_t minor() const noexcept = 0;

protected:
    VersionView() = default;
};

enum class Endianness
{
    Little,
    Big
};

class ServerCapabilitiesView
{
public:
    virtual ~ServerCapabilitiesView()                                                = default;
    ServerCapabilitiesView(const ServerCapabilitiesView&)                            = delete;
    ServerCapabilitiesView& operator=(const ServerCapabilitiesView&)                 = delete;
    ServerCapabilitiesView(ServerCapabilitiesView&&)                                 = delete;
    ServerCapabilitiesView& operator=(ServerCapabilitiesView&&)                      = delete;
    [[nodiscard]] virtual std::unique_ptr<VersionView> supported_version() const     = 0;
    [[nodiscard]] virtual Endianness endianness() const noexcept                     = 0;
    [[nodiscard]] virtual std::uint32_t number_of_supported_workers() const noexcept = 0;

protected:
    ServerCapabilitiesView() = default;
};

class KeepAliveView
{
public:
    virtual ~KeepAliveView()                       = default;
    KeepAliveView(const KeepAliveView&)            = delete;
    KeepAliveView& operator=(const KeepAliveView&) = delete;
    KeepAliveView(KeepAliveView&&)                 = delete;
    KeepAliveView& operator=(KeepAliveView&&)      = delete;

protected:
    KeepAliveView() = default;
};

class CloseConnectionView
{
public:
    virtual ~CloseConnectionView()                             = default;
    CloseConnectionView(const CloseConnectionView&)            = delete;
    CloseConnectionView& operator=(const CloseConnectionView&) = delete;
    CloseConnectionView(CloseConnectionView&&)                 = delete;
    CloseConnectionView& operator=(CloseConnectionView&&)      = delete;

protected:
    CloseConnectionView() = default;
};

} // namespace exasol::udf::v2::interface
