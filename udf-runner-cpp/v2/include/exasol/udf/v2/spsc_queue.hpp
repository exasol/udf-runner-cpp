#pragma once

// Moodycamel is header-only. Rewrite its namespace while parsing the upstream
// headers so the resulting symbols cannot collide with an unrelated copy that
// may be loaded into the same process.
#define moodycamel exasol::udf::v2::third_party::moodycamel
#include <readerwritercircularbuffer.h>
#include <readerwriterqueue.h>
#undef moodycamel

namespace exasol::udf::v2 {

template <typename T>
using SpscQueue = third_party::moodycamel::ReaderWriterQueue<T>;

template <typename T>
using SpscCircularBuffer =
    third_party::moodycamel::BlockingReaderWriterCircularBuffer<T>;

}  // namespace exasol::udf::v2
