BENCHMARK_VM_ENABLED_DEFINE=select({
        "@exaudfclient_base//:benchmark": ["ENABLE_BENCHMARK_VM"],
        "//conditions:default": []
    }) 
STREAMING_VM_ENABLED_DEFINE=select({
        "@exaudfclient_base//:bash": ["ENABLE_STREAMING_VM"],
        "//conditions:default": []
    }) 
TEST_VM_ENABLED_DEFINE=select({
        "@exaudfclient_base//:test_vm": ["ENABLE_TEST_VM"],
        "//conditions:default": []
    })
VM_ENABLED_DEFINES=BENCHMARK_VM_ENABLED_DEFINE+STREAMING_VM_ENABLED_DEFINE+TEST_VM_ENABLED_DEFINE
