#include <exasol/udf/v2/context/context_factory.hpp>
#include <exasol/udf/v2/context/context_manager.hpp>

#include <exasol/udf/v2/context/low_level_context.hpp>

#include <stdexcept>
#include <utility>

namespace exasol::udf::v2::context
{
namespace
{

    class DefaultFactory final : public BackgroundIoThreadFactory
    {
    public:
        [[nodiscard]] std::unique_ptr<BackgroundIoThreadInterface> create(
            socket::OwnedFileDescriptor connection) override
        {
            return make_background_io_thread(std::move(connection));
        }
    };

} // namespace

ContextManager::ContextManager() : ContextManager(std::make_unique<DefaultFactory>())
{
}

ContextManager::ContextManager(std::unique_ptr<BackgroundIoThreadFactory> factory)
    : factory_(std::move(factory))
{
    if (factory_ == nullptr)
    {
        throw std::invalid_argument("background I/O thread factory must not be null");
    }
}

std::unique_ptr<interface::Context> ContextManager::create_context(
    socket::OwnedFileDescriptor connection)
{
    return std::make_unique<LowLevelContext>(factory_->create(std::move(connection)));
}

} // namespace exasol::udf::v2::context

namespace exasol::udf::v2
{

std::unique_ptr<interface::Context> create_context(socket::OwnedFileDescriptor connection)
{
    return context::ContextManager().create_context(std::move(connection));
}

std::unique_ptr<interface::Context> create_context(
    socket::OwnedFileDescriptor connection,
    std::unique_ptr<context::BackgroundIoThreadFactory> background_thread_factory)
{
    return context::ContextManager(std::move(background_thread_factory))
        .create_context(std::move(connection));
}

} // namespace exasol::udf::v2
