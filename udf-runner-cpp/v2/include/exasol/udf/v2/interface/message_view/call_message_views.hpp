#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace exasol::udf::v2::interface
{

class OpenCallView
{
public:
    virtual ~OpenCallView()                                                          = default;
    OpenCallView(const OpenCallView&)                                                = delete;
    OpenCallView& operator=(const OpenCallView&)                                     = delete;
    OpenCallView(OpenCallView&&)                                                     = delete;
    OpenCallView& operator=(OpenCallView&&)                                          = delete;
    [[nodiscard]] virtual std::optional<std::string_view> call_name() const noexcept = 0;

protected:
    OpenCallView() = default;
};

class ResetView
{
public:
    virtual ~ResetView()                                        = default;
    ResetView(const ResetView&)                                 = delete;
    ResetView& operator=(const ResetView&)                      = delete;
    ResetView(ResetView&&)                                      = delete;
    ResetView& operator=(ResetView&&)                           = delete;
    [[nodiscard]] virtual std::uint64_t row_id() const noexcept = 0;

protected:
    ResetView() = default;
};

class CloseCallView
{
public:
    virtual ~CloseCallView()                       = default;
    CloseCallView(const CloseCallView&)            = delete;
    CloseCallView& operator=(const CloseCallView&) = delete;
    CloseCallView(CloseCallView&&)                 = delete;
    CloseCallView& operator=(CloseCallView&&)      = delete;

protected:
    CloseCallView() = default;
};

} // namespace exasol::udf::v2::interface
