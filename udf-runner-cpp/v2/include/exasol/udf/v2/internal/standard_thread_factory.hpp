#pragma once

#include <exasol/udf/v2/internal/thread_factory.hpp>

#include <memory>

namespace exasol::udf::v2::internal
{

/// Creates threads backed by the C++ standard-library thread runtime.
class StandardThreadFactory final : public ThreadFactory
{
public:
    [[nodiscard]] std::unique_ptr<Thread> create(ThreadTask task) override;
};

} // namespace exasol::udf::v2::internal
