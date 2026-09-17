#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <span>
#include <thread>
#include <vector>

#include <exasol/udf/v2/mpmc_queue.hpp>
#include <exasol/udf/v2/spsc_queue.hpp>
#include <exasol/udf/v2/waitable_queue.hpp>

namespace {

constexpr std::size_t kMaxOperations = 32;
constexpr std::size_t kMaxBatchSize = 4;

using Byte = std::byte;
using Value = std::uint64_t;

[[noreturn]] void fuzz_failure() {
  // Queue corruption, loss, duplication, or reordering is a finding. Keep
  // this independent from assert(), which may be disabled in fuzz builds.
  std::abort();
}

class StartGate {
public:
  explicit StartGate(const std::size_t participant_count)
      : participant_count_(participant_count) {}

  void arrive_and_wait() {
    ready_.fetch_add(1, std::memory_order::seq_cst);
    while (!go_.load(std::memory_order::seq_cst)) {
      std::this_thread::yield();
    }
  }

  void release() {
    while (ready_.load(std::memory_order::seq_cst) != participant_count_) {
      std::this_thread::yield();
    }
    go_.store(true, std::memory_order::seq_cst);
  }

private:
  const std::size_t participant_count_;
  std::atomic<std::size_t> ready_{0};
  std::atomic<bool> go_{false};
};

Value make_value(const std::size_t producer, const std::size_t sequence) {
  return (static_cast<Value>(producer) << 32U) | static_cast<Value>(sequence);
}

void verify_results(const std::vector<std::vector<Value>> &produced,
                    const std::vector<std::vector<Value>> &consumed,
                    const bool preserve_order) {
  std::vector<Value> expected;
  std::vector<Value> actual;
  for (const auto &values : produced) {
    expected.insert(expected.end(), values.begin(), values.end());
  }
  for (const auto &values : consumed) {
    actual.insert(actual.end(), values.begin(), values.end());
  }

  if (preserve_order) {
    if (produced.size() != 1 || consumed.size() != 1 || expected != actual) {
      fuzz_failure();
    }
    return;
  }

  std::ranges::sort(expected);
  std::ranges::sort(actual);
  if (expected != actual) {
    fuzz_failure();
  }
}

template <typename Queue, bool Waitable>
void produce(Queue &queue, StartGate &start_gate,
             const std::span<const Byte> operations, const std::size_t producer,
             std::vector<Value> &produced,
             std::atomic<std::size_t> &producers_remaining,
             std::atomic<bool> &producers_done) {
  start_gate.arrive_and_wait();
  std::size_t sequence = 0;
  for (const Byte operation_byte : operations) {
    if constexpr (Waitable) {
      if ((std::to_integer<unsigned int>(operation_byte) & 1U) != 0) {
        std::array<Value, kMaxBatchSize> batch{};
        const std::size_t batch_size =
            1 + ((std::to_integer<unsigned int>(operation_byte) >> 1U) %
                 kMaxBatchSize);
        for (std::size_t item = 0; item < batch_size; ++item) {
          batch[item] = make_value(producer, sequence++);
        }
        const std::size_t enqueued =
            queue.enqueue_batch(batch.begin(), batch.begin() + batch_size);
        produced.insert(produced.end(), batch.begin(),
                        batch.begin() + enqueued);
      } else {
        const Value value = make_value(producer, sequence++);
        if (queue.enqueue(value)) {
          produced.push_back(value);
        }
      }
    } else {
      const Value value = make_value(producer, sequence++);
      if (queue.enqueue(value)) {
        produced.push_back(value);
      }
    }

    if ((std::to_integer<unsigned int>(operation_byte) & 4U) != 0) {
      std::this_thread::yield();
    }
  }
  if (producers_remaining.fetch_sub(1, std::memory_order::seq_cst) == 1) {
    producers_done.store(true, std::memory_order::seq_cst);
  }
}

template <typename Queue, bool Waitable>
void consume(Queue &queue, StartGate &start_gate, std::vector<Value> &consumed,
             const std::atomic<bool> &producers_done) {
  start_gate.arrive_and_wait();
  for (;;) {
    if constexpr (Waitable) {
      queue.drain_notifications();
    }
    Value value = 0;
    if (queue.try_dequeue(value)) {
      consumed.push_back(value);
      continue;
    }
    if (producers_done.load(std::memory_order::seq_cst)) {
      break;
    }
    std::this_thread::yield();
  }
}

template <typename Queue, bool Waitable>
void run_queue(const std::span<const Byte> operations,
               const std::size_t producer_count,
               const std::size_t consumer_count, const bool preserve_order) {
  Queue queue;
  std::vector<std::vector<Value>> produced(producer_count);
  std::vector<std::vector<Value>> consumed(consumer_count);
  for (auto &values : produced) {
    values.reserve(operations.size() * (Waitable ? kMaxBatchSize : 1));
  }
  for (auto &values : consumed) {
    values.reserve(operations.size() * producer_count *
                   (Waitable ? kMaxBatchSize : 1));
  }

  StartGate start_gate(producer_count + consumer_count);
  std::atomic<bool> producers_done{false};
  std::atomic<std::size_t> producers_remaining{producer_count};
  std::vector<std::thread> producers;
  std::vector<std::thread> consumers;
  producers.reserve(producer_count);
  consumers.reserve(consumer_count);

  for (std::size_t producer = 0; producer < producer_count; ++producer) {
    producers.emplace_back(
        produce<Queue, Waitable>, std::ref(queue), std::ref(start_gate),
        operations, producer, std::ref(produced[producer]),
        std::ref(producers_remaining), std::ref(producers_done));
  }

  for (std::size_t consumer = 0; consumer < consumer_count; ++consumer) {
    consumers.emplace_back(consume<Queue, Waitable>, std::ref(queue),
                           std::ref(start_gate), std::ref(consumed[consumer]),
                           std::cref(producers_done));
  }

  start_gate.release();
  for (auto &producer : producers) {
    producer.join();
  }
  for (auto &consumer : consumers) {
    consumer.join();
  }

  if constexpr (Waitable) {
    // Notifications are hints rather than item counts. Drain any final
    // counter value after all queue elements have been consumed.
    queue.drain_notifications();
  }
  // A consumer can observe an empty queue while another consumer is
  // completing its final dequeue. Once all consumers have joined, there is no
  // longer a race, so perform a final nonblocking drain before checking the
  // accounting. This also makes termination independent of scheduling.
  Value value = 0;
  while (queue.try_dequeue(value)) {
    consumed.front().push_back(value);
  }
  verify_results(produced, consumed, preserve_order);
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data,
                                      const std::size_t size) {
  if (size < 2) {
    return 0;
  }

  const std::size_t operation_count = std::min(kMaxOperations, size - 1);
  const auto *operations = reinterpret_cast<const Byte *>(data + 1);
  const unsigned int mode = data[0] & 3U;
  const std::span<const Byte> operation_span(operations, operation_count);

  switch (mode) {
  case 0:
    run_queue<exasol::udf::v2::SpscQueue<Value>, false>(operation_span, 1, 1,
                                                        true);
    break;
  case 1:
    run_queue<exasol::udf::v2::MpmcQueue<Value>, false>(
        operation_span, 2 + ((data[0] >> 2U) & 1U), 2 + ((data[0] >> 3U) & 1U),
        false);
    break;
  case 2:
    run_queue<exasol::udf::v2::WaitableSpscQueue<Value>, true>(operation_span,
                                                               1, 1, true);
    break;
  case 3:
    run_queue<exasol::udf::v2::WaitableMpmcQueue<Value>, true>(
        operation_span, 2 + ((data[0] >> 2U) & 1U), 2 + ((data[0] >> 3U) & 1U),
        false);
    break;
  default:
    fuzz_failure();
  }
  return 0;
}
