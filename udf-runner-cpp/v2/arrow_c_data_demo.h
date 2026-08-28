#pragma once

#include <arrow/c/abi.h>

#include <stdint.h>

#if defined(_WIN32)
#define UDF_RUNNER_CPP_V2_EXPORT __declspec(dllexport)
#else
#define UDF_RUNNER_CPP_V2_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

UDF_RUNNER_CPP_V2_EXPORT int udf_runner_cpp_v2_demo_export_record_batch(
    struct ArrowArray* out_array, struct ArrowSchema* out_schema);

UDF_RUNNER_CPP_V2_EXPORT int udf_runner_cpp_v2_demo_consume_record_batch(
    struct ArrowArray* array, struct ArrowSchema* schema, int64_t* out_row_count,
    int64_t* out_id_sum);

UDF_RUNNER_CPP_V2_EXPORT const char* udf_runner_cpp_v2_demo_last_error(void);

#ifdef __cplusplus
}  // extern "C"
#endif

