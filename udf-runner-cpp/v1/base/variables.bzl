BENCHMARK_VM_ENABLED_DEFINE=select({
        "//:benchmark": ["ENABLE_BENCHMARK_VM"],
        "//conditions:default": []
    }) 
STREAMING_VM_ENABLED_DEFINE=select({
        "//:bash": ["ENABLE_STREAMING_VM"],
        "//conditions:default": []
    }) 
PLUGIN_CLIENT_ENABLED_DEFINE=select({
        "//:plugin_client": ["UDF_PLUGIN_CLIENT"],
        "//conditions:default": []
    })
VM_ENABLED_DEFINES=BENCHMARK_VM_ENABLED_DEFINE+STREAMING_VM_ENABLED_DEFINE+PLUGIN_CLIENT_ENABLED_DEFINE
