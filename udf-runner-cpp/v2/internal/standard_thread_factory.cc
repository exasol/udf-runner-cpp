#include <exasol/udf/v2/internal/standard_thread_factory.hpp>

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>

namespace exasol::udf::v2::internal
{

namespace
{

    class StandardThread final : public Thread
    {
    public:
        explicit StandardThread(ThreadTask task) : task(std::move(task))
        {
        }

        void start() override
        {
            std::lock_guard lock(this->mutex);
            if (this->started)
            {
                throw std::logic_error("thread has already been started");
            }
            this->started = true;
            try
            {
                this->thread = std::thread([this] { this->run_task(); });
            }
            catch (...)
            {
                this->started = false;
                throw;
            }
        }

        void join() override
        {
            static_cast<void>(this->join_impl(std::nullopt));
        }

        [[nodiscard]] bool join_for(const std::chrono::steady_clock::duration timeout) override
        {
            if (timeout < std::chrono::steady_clock::duration::zero())
            {
                throw std::invalid_argument("thread join timeout must not be negative");
            }
            const auto now             = std::chrono::steady_clock::now();
            const auto maximum_timeout = std::chrono::steady_clock::time_point::max() - now;
            if (timeout >= maximum_timeout)
            {
                return this->join_until(std::chrono::steady_clock::time_point::max());
            }
            return this->join_until(now + timeout);
        }

        [[nodiscard]] bool join_until(const std::chrono::steady_clock::time_point deadline) override
        {
            return this->join_impl(deadline);
        }

    private:
        [[nodiscard]] bool join_impl(
            const std::optional<std::chrono::steady_clock::time_point> deadline)
        {
            std::unique_lock lock(this->mutex);
            if (!this->started)
            {
                throw std::logic_error("thread has not been started");
            }
            if (this->joined)
            {
                return true;
            }
            if (!this->joining && this->thread.get_id() == std::this_thread::get_id())
            {
                throw std::logic_error("a thread cannot join itself");
            }
            if (!this->wait_for(lock, deadline, [this] { return this->completed; }))
            {
                return false;
            }
            while (!this->joined)
            {
                if (this->joining)
                {
                    if (!this->wait_for(lock, deadline,
                                        [this] { return this->joined || !this->joining; }))
                    {
                        return false;
                    }
                    continue;
                }

                this->joining = true;
                lock.unlock();
                try
                {
                    this->thread.join();
                }
                catch (...)
                {
                    lock.lock();
                    this->joining = false;
                    lock.unlock();
                    this->state_changed.notify_all();
                    throw;
                }

                lock.lock();
                this->joining = false;
                this->joined  = true;
                lock.unlock();
                this->state_changed.notify_all();
                return true;
            }
            return true;
        }

        template <typename Predicate>
        [[nodiscard]] bool wait_for(
            std::unique_lock<std::mutex>& lock,
            const std::optional<std::chrono::steady_clock::time_point> deadline,
            Predicate predicate)
        {
            if (!deadline.has_value())
            {
                this->state_changed.wait(lock, std::move(predicate));
                return true;
            }
            return this->state_changed.wait_until(lock, deadline.value(), std::move(predicate));
        }

        void run_task()
        {
            this->task();
            {
                std::lock_guard lock(this->mutex);
                this->completed = true;
            }
            this->state_changed.notify_all();
        }

        ThreadTask task;
        std::thread thread;
        std::condition_variable state_changed;
        std::mutex mutex;
        bool started   = false;
        bool completed = false;
        bool joining   = false;
        bool joined    = false;
    };

} // namespace

std::unique_ptr<Thread> StandardThreadFactory::create(ThreadTask task)
{
    if (!task)
    {
        throw std::invalid_argument("thread task must not be empty");
    }
    return std::make_unique<StandardThread>(std::move(task));
}

} // namespace exasol::udf::v2::internal
