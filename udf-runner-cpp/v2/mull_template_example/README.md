# Mull template fixtures

This directory contains two small Bazel tests for investigating C++ template
support in Mull.

`mull_template_test` is the positive control. It contains a simple function
template and should produce and kill mutants.

`mull_template_queue_like_test` is the negative control. It uses a header-only
class template with inline methods, dependent storage calls, a forwarding
member template, and implicit instantiation in the test translation unit. It
has the same relevant shape as `WaitableQueue`, but no eventfd, threading,
GMock, or third-party queue dependency. It is expected to reproduce Mull's
missing or incomplete mutation discovery for the queue-shaped template.

Run the normal tests from the v2 module:

```bash
bazel test //:mull_template_test //:mull_template_queue_like_test
```

Run the positive control through the Mull session:

```bash
poetry run nox -s mull -- --target mull_template_test
```

The queue-shaped control is tagged `no-mull` in the normal matrix, but can be
run explicitly for diagnosis:

```bash
poetry run nox -s mull -- --target mull_template_queue_like_test
```

The diagnostic should be interpreted together with the linker comparison: the
Mull-only Bazel build uses `ld.bfd` so that `.mull_mutants` metadata survives
linking. If the positive control works with BFD while this class-template
control still reports no usable mutants, the remaining issue is template
instantiation/linkage support rather than only the linker.
