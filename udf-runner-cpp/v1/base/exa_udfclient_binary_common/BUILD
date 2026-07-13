package(default_visibility = ["//visibility:public"])

genrule(
    name = "exaudflib_output_path_header",
    srcs = ["//:libexaudflib_complete.so"],
    outs = ["exaudf_lib_output_path.h"],
    cmd = '''
      echo "#define CUSTOM_LIBEXAUDFLIB_PATH \\"/exaudf/external/$(rlocationpath //:libexaudflib_complete.so)\\"" > $@
    ''',
)

cc_library(
    name = "exa_udfclient_binary_common_lib",
    srcs = [
        "exa_lib_loader.cc",
        "exa_set_env.cc",
        "exa_udf_base.cc",
        ":exaudflib_output_path_header",
    ],
    hdrs = [
        "exa_lib_loader.h",
        "exa_set_env.h",
        "exa_udf_base.h",
    ],
    deps = [
        "//:load_dynamic",
        "//exaudflib:header",
        "//exaudflib:exaudflib-deps",
        "//utils:utils",
    ],
    data = ["//:libexaudflib_complete.so"],
)
