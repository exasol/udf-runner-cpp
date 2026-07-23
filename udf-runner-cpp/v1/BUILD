
load("//:variables.bzl", "VM_ENABLED_DEFINES")



VM_ENABLED_DEPS=select({
        "@exaudfclient_base//:benchmark": ["//benchmark_container:benchmark_container"],
        "//conditions:default": []
    }) + select({
        "@exaudfclient_base//:bash": ["//streaming_container:streamingcontainer"],
        "//conditions:default": []
    }) + select({
        "@exaudfclient_base//:test_vm": ["//test_container:test_container"],
        "//conditions:default": []
    })

cc_binary(
    name = "cpp_runner_bin",
    srcs = [
        "exa_udfclient.cc",
        "exa_udf_client.cc",
        "exa_udf_client.h",
        "exa_vm_factory.cc",
        "exa_vm_factory.h",
        "@exaudfclient_base//:load_dynamic",
    ],
    linkopts = ["-ldl"],
    deps = [
        "@exaudfclient_base//exa_udfclient_binary_common:exa_udfclient_binary_common_lib",
    ] + VM_ENABLED_DEPS,
    defines = VM_ENABLED_DEFINES,
    data = ["@exaudfclient_base//:libexaudflib_complete.so"],
)

## The purpose of the static binaries is to verify if the linker namespace test work correctly.
## It is crucial that all linker symbols of exaudflib and all it's dependency are loaded into a new linker namespace,
## so that the user can load it's own versions of those dependencies without any conflict.
## With this binary we simulate an error in our build system, that is a direct depedency to protobuf/zmq,
## which then must be detected with the linker namespace tests:
## test/linker_namespace_sanity/linker_namespace_sanity.py checks the wrong configuration
## The normal build should not expose protobuf/zmq in the primary linker namespace.
##
## We need to explicitly declare the dependency of protobuf/zmq here, as the exaudflib is a static lib (//exaudflib:exaudflib)
## and hence does not contain dependency information. We cannot declare the shared lib (:exaudflib_complete.so)
## as dependency as it is a binary for bazel.

cc_binary(
    name = "cpp_runner_static",
    srcs = [
        "exa_udfclient.cc",
        "exa_udf_client.cc",
        "exa_udf_client.h",
        "exa_vm_factory.cc",
        "exa_vm_factory.h",
        "@exaudfclient_base//:load_dynamic",
    ],
    linkopts = ["-ldl"],
    deps = [
        "@exaudfclient_base//exa_udfclient_binary_common:exa_udfclient_binary_common_lib",
        "@zmq//:zmq",
        "@protobuf//:protobuf",
    ] + VM_ENABLED_DEPS,
    defines = VM_ENABLED_DEFINES,
    data = ["@exaudfclient_base//:libexaudflib_complete.so"],
)

# Workarround for the hardcoded paths in udf-runner-cpp for libexaudflib_complete.so
# - libexaudflib_complete.so gets dynamically loaded, therefore the udf-runner-cpp needs to know their path
# - Most flexible way to provides these paths would environment variables
# - The exasol database can't provide these paths, because they depend on the container
# - A workarround to provide these paths would be wrapper bash script which set these environment variables
# - For performance reason, we can not wrap the binary into a shell script, as such this is only for local debugging and testing
#   and we hardcode the paths for the production container

sh_library(
    name = "wrapper_generator_bin",
    srcs=select({
        "@exaudfclient_base//:valgrind_wrapper": ["@exaudfclient_base//:create_binary_wrapper_valgrind.sh"],
        "@exaudfclient_base//:valgrind_massif_wrapper": ["@exaudfclient_base//:create_binary_wrapper_valgrind_massif.sh"],
        "@exaudfclient_base//:stdout_to_bucketfs": ["@exaudfclient_base//:create_binary_wrapper_stdout_to_bucketfs.sh"],
        "//conditions:default": ["//base:create_binary_wrapper.sh"]
    })
)

SLOW_WRAPPER_BINARY="""$(location //:wrapper_generator_bin) "$(location cpp_runner_bin)" "$@" "$(location @exaudfclient_base//:exaudfclient.template.sh)" """
FAST_BINARY="""cp "$(location cpp_runner_bin)" "$@" """
CREATE_BINARY_SCRIPT=select({
        "@exaudfclient_base//:fast_binary": FAST_BINARY,
        "@exaudfclient_base//:slow_wrapper": SLOW_WRAPPER_BINARY,
        "//conditions:default": FAST_BINARY
    })
genrule(
    name = "udf_runner_cpp_v1_gen",
    cmd = CREATE_BINARY_SCRIPT,
    outs = ["udf_runner_cpp_v1"],
    srcs = [":cpp_runner_bin", "@exaudfclient_base//:libexaudflib_complete.so", "@exaudfclient_base//:exaudfclient.template.sh", "//:wrapper_generator_bin"],
    output_to_bindir = True
)

SLOW_WRAPPER_STATIC_BINARY="""$(location //:wrapper_generator_bin) "$(location cpp_runner_static)" "$@" "$(location @exaudfclient_base//:exaudfclient.template.sh)" """
FAST_BINARY_STATIC="""cp "$(location cpp_runner_static)" "$@" """
CREATE_STATIC_BINARY_SCRIPT=select({
        "@exaudfclient_base//:fast_binary": FAST_BINARY_STATIC,
        "@exaudfclient_base//:slow_wrapper": SLOW_WRAPPER_STATIC_BINARY,
        "//conditions:default": FAST_BINARY_STATIC
    })

genrule(
    name = "udf_runner_cpp_v1_static_gen",
    cmd = CREATE_STATIC_BINARY_SCRIPT,
    outs = ["udf_runner_cpp_v1_static"],
    srcs = [":cpp_runner_static", "@exaudfclient_base//:libexaudflib_complete.so", "@exaudfclient_base//:exaudfclient.template.sh", "//:wrapper_generator_bin"],
    output_to_bindir = True
)
