package(default_visibility = ["//visibility:public"])

load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "test_container",
    srcs = ["test_container.cc"],
    hdrs = ["test_container.h"],
    deps = [
        "@exaudfclient_base//exaudflib:exaudflib-deps",
        "@exaudfclient_base//exaudflib:header",
    ],
)
