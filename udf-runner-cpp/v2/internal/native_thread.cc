#include <exasol/udf/v2/internal/native_thread.hpp>

#include <thread>
#include <utility>

namespace exasol::udf::v2::internal::detail
{

void StdThreadFacade::start(ThreadTask task)
{
    this->thread = std::thread(std::move(task));
}

void StdThreadFacade::join()
{
    this->thread.join();
}

std::thread::id StdThreadFacade::get_id() const noexcept
{
    return this->thread.get_id();
}

} // namespace exasol::udf::v2::internal::detail
