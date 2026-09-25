# v2 C++ Coding Style

This guide defines the conventions for C++ code under
[`udf-runner-cpp/v2`](../../../udf-runner-cpp/v2). The checked-in
[`clang-format` configuration](../../../udf-runner-cpp/v2/tools/clang-format/.clang-format)
and [`clang-tidy` configuration](../../../udf-runner-cpp/v2/tools/clang-tidy/.clang-tidy)
are authoritative for automatically checked rules.

## Files and includes

- Use UTF-8 source files.
- Use `.cc` for implementation files and `.h` or `.hpp` for headers, matching
  the convention already used by the v2 module.
- Keep `#include` directives at the top of the file. Do not include headers
  inside functions unless there is a documented, compelling reason.
- Include every header required by a file directly; do not rely on transitive
  includes.
- Put non-template function definitions in implementation files unless there
  is a measured performance reason to keep them inline.
- Keep public headers independent of private implementation details and avoid
  conditional compilation in headers unless it is required by the public API.

## Names and namespaces

- Use ASCII identifiers and `lower_case` for functions, variables, parameters,
  and data members.
- Use `CamelCase` for classes and enum types. Use `CamelCase` for scoped enum
  values as well.
- Name factory functions with a `create` prefix and getters/setters with
  `get_`/`set_` prefixes, for example `get_value()` and `set_value()`.
- Put file-local functions and types in an unnamed namespace.
- Put project code in an appropriate `exasol::udf::v2` namespace rather than
  importing a namespace with `using namespace`.
- Keep namespace aliases local and descriptive when they improve readability.

The naming policy is enforced by clang-tidy’s
`readability-identifier-naming` check. Class members have no naming prefix or
suffix. When a member access would otherwise be ambiguous, qualify it with
`this->`, for example `this->value`.

Names required by an external ABI or framework are exceptions. For example,
the libFuzzer entry point `LLVMFuzzerTestOneInput` keeps its required spelling.

## Functions and classes

- Prefer free functions for behavior that does not depend on object state.
- Avoid operator overloading unless the type has a clear value-like meaning
  and the overload is required for natural use of the public API.
- Mark a class `final` when it is not designed for inheritance.
- Mark overriding methods with `override`.
- Keep class declarations ordered, where practical, as public, protected, then
  private; within each section, place types before methods and data members.
- Avoid ambiguity between constructor parameters and members. Prefer the same
  descriptive name and qualify member access with `this->`.
- Separate function definitions with a blank line.

## Types, control flow, and errors

- Prefer fixed-width integer types such as `std::int32_t` and `std::uint64_t`
  when the width is part of the interface or serialized representation.
- Use `enum class` for new enumerations.
- For `std::optional`, use `has_value()` when testing presence and `value()`
  when explicitly retrieving the contained value. Name the variable after its
  value, not after the fact that it is optional.
- Follow the repository formatter for braces and indentation. Keep all code
  belonging to a `case`, including its terminating `break`, `return`, or
  fallthrough marker, inside the case body when braces are needed.
- Prefer safe, expressive casts. If a lower-level cast is required for a
  measured hot path or ABI boundary, document why it is safe.
- Report failures caused by external input or environment through the public
  error mechanism, normally an exception. Use assertions for programmer
  contract violations and impossible internal states.

## Documentation and cleanup

- Document design decisions close to the code they constrain.
- Put API documentation in public headers and implementation details near the
  implementation.
- Use Doxygen commands with `@`. Prefer `@returns`, `@throws`, and `@see`.
  Omit `@brief` when the first sentence already provides the brief.
- Remove commented-out code and avoid `#if 0` or `#if 1` except when a clear,
  documented temporary or compatibility purpose requires it.
- Keep comments factual and explain why non-obvious code exists, not what an
  immediately readable statement does.

## Tests and review

- Add or update tests when changing behavior, public interfaces, parsing,
  serialization, concurrency, or dependency boundaries.
- Prefer small, focused tests that make failures easy to diagnose.
- Write functional unit tests with GoogleTest `TEST` or `TEST_F` cases. Use
  `ASSERT_*` for prerequisites and `EXPECT_*` for independent checks; use
  `@googletest//:gtest_main` instead of a hand-written `main()`.
- GoogleMock is available through the GoogleTest dependency. Use it only to
  verify meaningful interactions with collaborators, callbacks, or failure
  boundaries. Do not add production abstractions solely to create a mock.
- Keep custom entry points for ELF inspection, dynamic-loading, include-order,
  and other specialized tests where they make the test's purpose clearer.
- Use Google Benchmark for performance tests. Exclude setup and cleanup from
  measured regions when appropriate, use `benchmark::DoNotOptimize` for values
  that must remain observable, and do not make benchmarks depend on fixed
  timing thresholds or a particular machine.
- Run the v2 build and tests, then the `clang-format` and `clang-tidy` checks
  described in the [code quality guide](v2_code_quality.md).
- Do not suppress a static-analysis warning without documenting the reason at
  the suppression site.
