#pragma once

// Keep all upstream moodycamel symbols below the project-owned namespace. Do
// not include these upstream headers directly in project or consumer code.
#define moodycamel exasol::udf::v2::third_party::moodycamel
#include <blockingconcurrentqueue.h>
#include <concurrentqueue.h>
#undef moodycamel

namespace exasol::udf::v2 {

template <typename T>
using MpmcQueue = third_party::moodycamel::ConcurrentQueue<T>;

template <typename T>
using BlockingMpmcQueue =
    third_party::moodycamel::BlockingConcurrentQueue<T>;

}  // namespace exasol::udf::v2
