## Summary


## Bug Fixes

n/a

## Features / Enhancements

 - #127: Added v2 Arrow de/serialization operations
 - #86: Added a RAII Linux epoll abstraction with injectable POSIX operations
 - #90: Added an injectable POSIX API wrapper for Unix-domain socket operations
 - #71: Documented the v2 runner architecture, worker-facing context interface, and background I/O design
 - #70: Added the v2 UDF protocol design and validation artifacts
 - #61: Added stacktrace-aware exceptions and assertions to v2
 - #54: Added v2 socket-library interface design
 - #75: Added the v2 Unix-domain stream socket library
 - #32: Added clang tidy to v2
 - #36: Added developer guide for clang-tidy and clang-format
 - #38: Added Sonar Qube Public
 - #49: Added Bazel-based fuzzing for v2

## Refactorings

* #125: Added a Lima-derived Podman build container for reproducible v2 Bazel builds
* #121: Replaced direct FlatBuffer types in v2 message views with mockable, serialization-independent typed view wrappers
* #111: Simplified v2 message-view presence APIs, split flow-control reset from send budget, removed builder encoding views, and retained control capabilities and call/control payloads in builder messages
* #44: Extracted WaitableQueue eventfd notifications into a reusable component
* #95: Routed POSIX APIs of socket-abstraction and Linux eventfd through POSIX facade

## Internal

* #111: Added parallel v2 regular-test Nox and CI runs for ASan, UBSan, and Valgrind
* #111: Added independent Nox and CI jobs for v2 clang-tidy, clang-format, coverage, and Sonar analysis
* #111: Split v1 and v2 Bazel checks into dedicated CI workflows
* #111: Fixed v1 CI to select Bazel 8.8.2 and resolved new Sonar ownership and escape-sequence findings
* #111: Fixed Lima Mull provisioning by installing the pinned LLVM 22 package
  directly from its verified release artifact
* #70: Removed the standalone JSON schema validation script and its dedicated CI workflow
* #60: Added Mull mutation testing workflow and report-viewing documentation
  for v2; targets without generated mutants now produce warnings instead of
  failing the workflow, and separated Linux EventFd code and factory-based
  Linux queue construction from the generic waitable-queue mutation target;
  updated artifact uploads to the Node.js 24-compatible action version
* #64: Added GoogleTest, GoogleMock, and Google Benchmark support for v2 tests
* #57: Defined and enforced public v2 C++ coding style
* #51: Added agent and contributor guidance for v1/v2 development, SLC workflows, CI testing, and PR conventions
* #56: Restructured the developer guide and synchronized agent guidance
* Updated Poetry dependencies and added developer guide and added .gitignore
* Updated mull checks workflows to use Ubuntu 26.04 and unified clang tool usage
