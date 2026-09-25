# v2 Code Quality Guide

## Static analysis with clang-tidy

`clang-tidy` runs static analysis checks on C++ source files. It is integrated
into the Bazel build as a configuration flag:

```bash
cd udf-runner-cpp/v2
bazel build --verbose_failures --config clang-tidy //...
```

Run clang-tidy on changed `.cpp` files before submitting code for review to
catch common issues early.

The Bazel configuration uses `clang-tidy-22` by default. To use another
installed executable, set `CLANG_TIDY` when invoking Bazel:

```bash
CLANG_TIDY=clang-tidy bazel build --verbose_failures --config clang-tidy //...
```

`CLANG_TIDY` may also contain an absolute path to the executable.

The wrapper removes `-fno-canonical-system-headers` from the compiler
arguments by default. To retain that argument, clear `CLANG_TIDY_REMOVED_ARG`:

```bash
CLANG_TIDY=clang-tidy CLANG_TIDY_REMOVED_ARG= \
  bazel build --verbose_failures --config clang-tidy //...
```

### Apply clang-tidy fixes

You can run `clang-apply-replacements` with:

```bash
bazel run @rules_clang_tidy//:apply-fixes \
  --@rules_clang_tidy//:clang-apply-replacements=//tools/clang-tidy:apply-replacements-wrapper \
  -- $(bazel info output_path)
```

Review the resulting diff carefully because automated fixes may not address
all findings correctly.

## Code formatting with clang-format

Source files are checked with `clang-format` through Bazel aspects. Check
formatting with:

```bash
cd udf-runner-cpp/v2
bazel build --verbose_failures --config clang-format //...
```

Apply formatting fixes with:

```bash
bazel build --config clang-format-fix //...
```

Run the formatting fix before committing to keep source files consistent.

## Excluding targets

Targets that contain third-party or otherwise incompatible code can be excluded
from both checks with the `noclangtidy` tag. The corresponding Bazel
configurations exclude targets carrying this tag:

```bazelrc
build:clang-tidy --build_tag_filters=-noclangtidy
build:clang-format --build_tag_filters=-noclangtidy
```

Apply the tag to a target as follows:

```starlark
alias(
    name = "third_party_package",
    actual = "@v2_third_party//:package",
    tags = ["noclangtidy"],
)
```

## Mutation testing with Mull

Mutation testing for the functional v2 C++ tests uses [Mull](https://mull-project.com/)
with the pinned Mull 0.34.1 release and matching LLVM 20 toolchain. Install the
LLVM 20 compiler and `mull-20`, then verify that `mull-runner-20` and
`/usr/lib/mull-ir-frontend-20` are available.

Run the mutation session from the repository root:

```bash
poetry run -- nox --sessions=mull
```

If the Bazel executable is named `bazelisk`, run:
`BAZEL=bazelisk poetry run -- nox --sessions=mull`.

The session discovers Bazel `cc_test` targets and runs each eligible target with
Mull instrumentation. It writes reports to `.build_output/mull/` and enforces
an 80% mutation-score threshold for every target. The LLVM major version can be
changed with `MULL_LLVM_VERSION`; custom tool paths can be supplied with
`MULL_CXX`, `MULL_RUNNER`, and `MULL_IR_FRONTEND`. The C compiler used by Bazel
can be overridden with `MULL_CC`.

Mutation testing is not reliable for C++ template implementations or tests
that only exercise third-party dependencies. Keep those tests in normal Bazel
test coverage and exclude them from Mull with the `no-mull` tag. Production
implementation units with Mull-compatible non-template code should have a
dedicated test target that remains in the mutation matrix.
