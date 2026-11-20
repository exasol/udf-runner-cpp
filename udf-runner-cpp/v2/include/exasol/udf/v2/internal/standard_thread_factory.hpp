#pragma once

#include <exasol/udf/v2/internal/native_thread.hpp>
#include <exasol/udf/v2/internal/thread_factory.hpp>

#include <memory>

namespace exasol::udf::v2::internal
{

/// Creates threads backed by the C++ standard-library thread runtime.
class StandardThreadFactory final : public ThreadFactory
{
public:
    StandardThreadFactory();

    /// Rejects an empty creator with std::invalid_argument.
    explicit StandardThreadFactory(detail::NativeThreadCreator create_native_thread);

    /// Rejects a null native thread returned by the creator with std::logic_error.
    [[nodiscard]] std::unique_ptr<Thread> create(ThreadTask task) override;

private:
    detail::NativeThreadCreator create_native_thread;
};

} // namespace exasol::udf::v2::internal
