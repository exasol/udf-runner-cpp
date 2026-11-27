"""Helpers for assigning each production translation unit a single target."""

load("@rules_cc//cc:cc_library.bzl", "cc_library")


def cc_source_group(name, srcs, **kwargs):
    """Create one library per source and preserve the existing aggregate label."""
    attrs = dict(kwargs)
    headers = attrs.pop("hdrs", [])
    dependencies = attrs.pop("deps", [])
    source_targets = []
    header_target = name + "_headers"

    if headers:
        cc_library(
            name = header_target,
            hdrs = headers,
            deps = dependencies,
            **attrs
        )

    for source in srcs:
        source_name = source.replace("/", "_").replace(".", "_")
        source_target = name + "__" + source_name
        source_targets.append(":" + source_target)
        source_deps = list(dependencies)
        if headers:
            source_deps.append(":" + header_target)
        cc_library(
            name = source_target,
            srcs = [source],
            deps = source_deps,
            **attrs
        )

    aggregate_attrs = {}
    for key in ["visibility", "target_compatible_with"]:
        if key in attrs:
            aggregate_attrs[key] = attrs[key]
    aggregate_deps = source_targets
    if headers:
        aggregate_deps.append(":" + header_target)
    cc_library(name = name, deps = aggregate_deps, **aggregate_attrs)
