load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

CORE_COMPUTE_SRCS = [
    "cpp/src/arrow/compute/api_aggregate.cc",
    "cpp/src/arrow/compute/api_scalar.cc",
    "cpp/src/arrow/compute/api_vector.cc",
    "cpp/src/arrow/compute/cast.cc",
    "cpp/src/arrow/compute/exec.cc",
    "cpp/src/arrow/compute/expression.cc",
    "cpp/src/arrow/compute/function.cc",
    "cpp/src/arrow/compute/function_internal.cc",
    "cpp/src/arrow/compute/kernel.cc",
    "cpp/src/arrow/compute/kernels/chunked_internal.cc",
    "cpp/src/arrow/compute/kernels/codegen_internal.cc",
    "cpp/src/arrow/compute/kernels/scalar_cast_boolean.cc",
    "cpp/src/arrow/compute/kernels/scalar_cast_dictionary.cc",
    "cpp/src/arrow/compute/kernels/scalar_cast_extension.cc",
    "cpp/src/arrow/compute/kernels/scalar_cast_internal.cc",
    "cpp/src/arrow/compute/kernels/scalar_cast_nested.cc",
    "cpp/src/arrow/compute/kernels/scalar_cast_numeric.cc",
    "cpp/src/arrow/compute/kernels/scalar_cast_string.cc",
    "cpp/src/arrow/compute/kernels/scalar_cast_temporal.cc",
    "cpp/src/arrow/compute/kernels/temporal_internal.cc",
    "cpp/src/arrow/compute/kernels/vector_hash.cc",
    "cpp/src/arrow/compute/kernels/vector_selection.cc",
    "cpp/src/arrow/compute/kernels/vector_selection_filter_internal.cc",
    "cpp/src/arrow/compute/kernels/vector_selection_internal.cc",
    "cpp/src/arrow/compute/kernels/vector_selection_take_internal.cc",
    "cpp/src/arrow/compute/kernels/vector_swizzle.cc",
    "cpp/src/arrow/compute/ordering.cc",
    "cpp/src/arrow/compute/registry.cc",
]

cc_library(
    name = "arrow_core",
    srcs = (
        glob(["cpp/src/arrow/*.cc"], exclude = [
            "cpp/src/arrow/*_test.cc",
            "cpp/src/arrow/*_benchmark.cc",
            "cpp/src/arrow/memory_pool_jemalloc.cc",
            "cpp/src/arrow/memory_pool_mimalloc.cc",
        ]) +
        glob(["cpp/src/arrow/c/*.cc"], exclude = [
            "cpp/src/arrow/c/*_test.cc",
            "cpp/src/arrow/c/*_benchmark.cc",
        ]) +
        glob(["cpp/src/arrow/array/*.cc"], exclude = [
            "cpp/src/arrow/array/*_benchmark.cc",
            "cpp/src/arrow/array/*_test.cc",
        ]) +
        CORE_COMPUTE_SRCS +
        [
            "cpp/src/arrow/extension/bool8.cc",
            "cpp/src/arrow/extension/json.cc",
            "cpp/src/arrow/extension/parquet_variant.cc",
            "cpp/src/arrow/extension/uuid.cc",
        ] +
        glob(["cpp/src/arrow/io/*.cc"], exclude = [
            "cpp/src/arrow/io/*_benchmark.cc",
            "cpp/src/arrow/io/*_test.cc",
            "cpp/src/arrow/io/test_common.cc",
        ]) +
        glob(["cpp/src/arrow/tensor/*.cc"], exclude = [
            "cpp/src/arrow/tensor/*_benchmark.cc",
            "cpp/src/arrow/tensor/*_test.cc",
        ]) +
        glob(["cpp/src/arrow/util/*.cc"], exclude = [
            "cpp/src/arrow/util/*_benchmark.cc",
            "cpp/src/arrow/util/*_test.cc",
            "cpp/src/arrow/util/bpacking_simd_128_alt.cc",
            "cpp/src/arrow/util/bpacking_simd_256.cc",
            "cpp/src/arrow/util/bpacking_simd_avx512.cc",
            "cpp/src/arrow/util/byte_stream_split_internal_avx2.cc",
            "cpp/src/arrow/util/compression_brotli.cc",
            "cpp/src/arrow/util/compression_bz2.cc",
            "cpp/src/arrow/util/compression_lz4.cc",
            "cpp/src/arrow/util/compression_snappy.cc",
            "cpp/src/arrow/util/compression_zlib.cc",
            "cpp/src/arrow/util/compression_zstd.cc",
            "cpp/src/arrow/util/test_common.cc",
            "cpp/src/arrow/util/tracing_internal.cc",
        ]) +
        glob(
            ["cpp/src/arrow/vendored/**/*.c", "cpp/src/arrow/vendored/**/*.cc"],
            exclude = ["cpp/src/arrow/vendored/xxhash/**"],
        ) + [
            "cpp/src/arrow/vendored/base64.cpp",
            "cpp/src/arrow/vendored/datetime.cpp",
        ]
    ),
    hdrs = glob([
        "cpp/src/arrow/**/*.h",
        "cpp/src/arrow/**/*.hpp",
    ]) + [
        "cpp/src/arrow/vendored/datetime/tz.cpp",
        "cpp/thirdparty/hadoop/include/hdfs.h",
    ],
    copts = [
        "-std=c++20",
        "-pthread",
    ],
    includes = [
        "cpp/src",
        "cpp/thirdparty/hadoop/include",
    ],
    linkopts = [
        "-ldl",
        "-lpthread",
        "-lrt",
    ],
    linkstatic = True,
    deps = ["@v2_xsimd//:xsimd"],
)
