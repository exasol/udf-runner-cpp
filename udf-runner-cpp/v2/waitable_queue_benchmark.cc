#include <benchmark/benchmark.h>

#include <sys/epoll.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

#include <exasol/udf/v2/spsc_queue.hpp>
#include <exasol/udf/v2/waitable_queue.hpp>

namespace {

void benchmark_check(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "waitable queue benchmark failure: %s\n", message);
        std::abort();
    }
}

// These benchmarks compare raw Moodycamel SPSC behavior with the blocking
// circular-buffer baseline and the eventfd-backed waitable queue. Results are
// informational: build mode, CPU frequency, scheduler activity, and system
// load can materially affect them.
struct TimedItem {
    std::uint64_t sequence;
    std::chrono::steady_clock::time_point sent;
};

// Includes raw enqueue and dequeue only; this is the queue-operation baseline
// for the waitable round-trip benchmark.
void BM_RawSpscRoundTrip(benchmark::State& state) {
    exasol::udf::v2::SpscQueue<int> queue(1024);
    for (auto _ : state) {
        int value = 0;
        benchmark::DoNotOptimize(queue.enqueue(1));
        benchmark::DoNotOptimize(queue.try_dequeue(value));
        benchmark::DoNotOptimize(value);
    }
    state.SetItemsProcessed(state.iterations());
}

// Includes enqueue, eventfd notification draining, and dequeue. The eventfd
// write is part of the measured round trip.
void BM_WaitableSpscRoundTrip(benchmark::State& state) {
    exasol::udf::v2::WaitableSpscQueue<int> queue(
        exasol::udf::v2::SpscQueue<int>(1024));
    for (auto _ : state) {
        int value = 0;
        benchmark::DoNotOptimize(queue.enqueue(1));
        benchmark::DoNotOptimize(queue.drain_notifications());
        benchmark::DoNotOptimize(queue.try_dequeue(value));
        benchmark::DoNotOptimize(value);
    }
    state.SetItemsProcessed(state.iterations());
}

// Includes wait_enqueue and dequeue on a non-full blocking SPSC queue. The
// benchmark measures the uncontended fast path rather than intentional waits.
void BM_BlockingSpscRoundTrip(benchmark::State& state) {
    exasol::udf::v2::SpscCircularBuffer<int> queue(1024);
    for (auto _ : state) {
        int value = 0;
        queue.wait_enqueue(1);
        benchmark::DoNotOptimize(queue.try_dequeue(value));
        benchmark::DoNotOptimize(value);
    }
    state.SetItemsProcessed(state.iterations());
}

// Enqueue-only benchmarks pause timing while removing the item so the queue
// remains empty for the next iteration. The waitable case includes its
// eventfd write; notification draining is cleanup and is not timed.
void BM_RawSpscEnqueueLatency(benchmark::State& state) {
    exasol::udf::v2::SpscQueue<int> queue(1024);
    for (auto _ : state) {
        benchmark::DoNotOptimize(queue.enqueue(1));
        state.PauseTiming();
        int value = 0;
        benchmark::DoNotOptimize(queue.try_dequeue(value));
        benchmark::DoNotOptimize(value);
        state.ResumeTiming();
    }
    state.SetItemsProcessed(state.iterations());
}

void BM_WaitableSpscEnqueueLatency(benchmark::State& state) {
    exasol::udf::v2::WaitableSpscQueue<int> queue(
        exasol::udf::v2::SpscQueue<int>(1024));
    for (auto _ : state) {
        benchmark::DoNotOptimize(queue.enqueue(1));
        state.PauseTiming();
        int value = 0;
        benchmark::DoNotOptimize(queue.try_dequeue(value));
        benchmark::DoNotOptimize(queue.drain_notifications());
        benchmark::DoNotOptimize(value);
        state.ResumeTiming();
    }
    state.SetItemsProcessed(state.iterations());
}

void BM_BlockingSpscEnqueueLatency(benchmark::State& state) {
    exasol::udf::v2::SpscCircularBuffer<int> queue(1024);
    for (auto _ : state) {
        queue.wait_enqueue(1);
        state.PauseTiming();
        int value = 0;
        benchmark::DoNotOptimize(queue.try_dequeue(value));
        benchmark::DoNotOptimize(value);
        state.ResumeTiming();
    }
    state.SetItemsProcessed(state.iterations());
}

// Raw and waitable batch benchmarks use the same batch sizes. The waitable
// queue emits one eventfd notification after the entire batch, exposing how
// batching amortizes notification overhead.
void BM_RawSpscBatch(benchmark::State& state) {
    const auto batch_size = static_cast<std::size_t>(state.range(0));
    const std::vector<int> batch(batch_size, 1);
    exasol::udf::v2::SpscQueue<int> queue(batch_size);

    for (auto _ : state) {
        for (int value : batch) {
            benchmark::DoNotOptimize(queue.enqueue(value));
        }
        int value = 0;
        for (std::size_t i = 0; i < batch_size; ++i) {
            benchmark::DoNotOptimize(queue.try_dequeue(value));
        }
        benchmark::DoNotOptimize(value);
    }
    state.SetItemsProcessed(state.iterations() * batch_size);
}

void BM_WaitableSpscBatch(benchmark::State& state) {
    const auto batch_size = static_cast<std::size_t>(state.range(0));
    const std::vector<int> batch(batch_size, 1);
    exasol::udf::v2::WaitableSpscQueue<int> queue{
        exasol::udf::v2::SpscQueue<int>(batch_size)};

    for (auto _ : state) {
        benchmark::DoNotOptimize(queue.enqueue_batch(batch.begin(), batch.end()));
        benchmark::DoNotOptimize(queue.drain_notifications());
        int value = 0;
        for (std::size_t i = 0; i < batch_size; ++i) {
            benchmark::DoNotOptimize(queue.try_dequeue(value));
        }
        benchmark::DoNotOptimize(value);
    }
    state.SetItemsProcessed(state.iterations() * batch_size);
}

// Measures producer timestamp through enqueue, eventfd readiness, epoll_wait,
// and dequeue. One item is outstanding at a time, so the result measures
// wakeup latency rather than latency caused by queue backlog. The producer
// handshake is outside the manually recorded interval.
void BM_WaitableSpscEpollLatency(benchmark::State& state) {
    exasol::udf::v2::WaitableSpscQueue<TimedItem> queue{
        exasol::udf::v2::SpscQueue<TimedItem>(8)};
    const int epoll_fd = ::epoll_create1(EPOLL_CLOEXEC);
    benchmark_check(epoll_fd != -1, "epoll_create1 failed");

    epoll_event queue_event{};
    queue_event.events = EPOLLIN;
    queue_event.data.fd = queue.native_handle();
    benchmark_check(::epoll_ctl(epoll_fd, EPOLL_CTL_ADD, queue.native_handle(),
                                &queue_event) == 0,
                    "epoll_ctl failed");

    std::atomic<std::uint64_t> requested{0};
    std::atomic<std::uint64_t> completed{0};
    std::atomic<bool> stop{false};
    std::thread producer([&] {
        std::uint64_t sequence = 0;
        while (!stop.load(std::memory_order_acquire)) {
            while (requested.load(std::memory_order_acquire) <= sequence &&
                   !stop.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            if (stop.load(std::memory_order_acquire)) {
                break;
            }

            TimedItem item{sequence++, std::chrono::steady_clock::now()};
            benchmark_check(queue.enqueue(std::move(item)), "queue enqueue failed");

            while (completed.load(std::memory_order_acquire) < sequence &&
                   !stop.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
        }
    });

    std::uint64_t expected_sequence = 0;
    for (auto _ : state) {
        requested.fetch_add(1, std::memory_order_release);

        epoll_event event{};
        int event_count = 0;
        do {
            event_count = ::epoll_wait(epoll_fd, &event, 1, -1);
        } while (event_count == -1 && errno == EINTR);
        benchmark_check(event_count == 1, "epoll_wait failed");
        benchmark_check(event.data.fd == queue.native_handle(),
                        "unexpected epoll event");

        benchmark::DoNotOptimize(queue.drain_notifications());
        TimedItem item{};
        benchmark_check(queue.try_dequeue(item), "queue dequeue failed");
        benchmark_check(item.sequence == expected_sequence,
                        "unexpected item sequence");
        ++expected_sequence;
        const auto elapsed = std::chrono::steady_clock::now() - item.sent;
        benchmark::DoNotOptimize(item);
        state.SetIterationTime(
            std::chrono::duration<double>(elapsed).count());
        completed.store(expected_sequence, std::memory_order_release);
    }

    stop.store(true, std::memory_order_release);
    requested.fetch_add(1, std::memory_order_release);
    producer.join();
    ::close(epoll_fd);
}

}  // namespace

BENCHMARK(BM_RawSpscRoundTrip);
BENCHMARK(BM_WaitableSpscRoundTrip);
BENCHMARK(BM_BlockingSpscRoundTrip);
BENCHMARK(BM_RawSpscEnqueueLatency);
BENCHMARK(BM_WaitableSpscEnqueueLatency);
BENCHMARK(BM_BlockingSpscEnqueueLatency);
BENCHMARK(BM_RawSpscBatch)
    ->Args({1})
    ->Args({8})
    ->Args({64})
    ->Args({256});
BENCHMARK(BM_WaitableSpscBatch)
    ->Args({1})
    ->Args({8})
    ->Args({64})
    ->Args({256});
BENCHMARK(BM_WaitableSpscEpollLatency)->UseManualTime();
