#pragma once

#include <exasol/udf/v2/interface/error_info.hpp>
#include <exasol/udf/v2/interface/status.hpp>

#include <optional>
#include <utility>

namespace exasol::udf::v2::interface
{

/// Status, optional value, and optional error returned by an operation.
template <typename T>
struct Result
{
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    OperationStatus status{OperationStatus::TransportError};
    std::optional<T> value;         // NOLINT(misc-non-private-member-variables-in-classes)
    std::optional<ErrorInfo> error; // NOLINT(misc-non-private-member-variables-in-classes)

    [[nodiscard]] static Result success(T result)
    {
        return {.status = OperationStatus::Ok, .value = std::move(result)};
    }
    [[nodiscard]] static Result failure(OperationStatus result_status,
                                        const std::optional<ErrorInfo>& detail = {})
    {
        return {result_status, {}, detail};
    }
    [[nodiscard]] explicit operator bool() const noexcept
    {
        return status == OperationStatus::Ok;
    }
};

template <>
struct Result<void>
{
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    OperationStatus status{OperationStatus::TransportError};
    std::optional<ErrorInfo> error; // NOLINT(misc-non-private-member-variables-in-classes)

    [[nodiscard]] static Result success()
    {
        return {.status = OperationStatus::Ok};
    }
    [[nodiscard]] static Result failure(OperationStatus result_status,
                                        const std::optional<ErrorInfo>& detail = {})
    {
        return {result_status, detail};
    }
    [[nodiscard]] explicit operator bool() const noexcept
    {
        return status == OperationStatus::Ok;
    }
};

} // namespace exasol::udf::v2::interface
