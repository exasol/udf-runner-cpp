## Summary


## Bug Fixes

n/a

## Features / Enhancements

 - #32: Added clang tidy to v2
 - #36: Added developer guide for clang-tidy and clang-format
 - #38: Added Sonar Qube Public
 - #49: Added Bazel-based fuzzing for v2

## Refactorings

* #44: Extracted WaitableQueue eventfd notifications into a reusable component

## Internal

* #60: Added Mull mutation testing workflow and report-viewing documentation
  for v2; targets without generated mutants now produce warnings instead of
  failing the workflow, and separated Linux EventFd code from the generic
  waitable-queue mutation target
* #64: Added GoogleTest, GoogleMock, and Google Benchmark support for v2 tests
* #57: Defined and enforced public v2 C++ coding style
* #51: Added agent and contributor guidance for v1/v2 development, SLC workflows, CI testing, and PR conventions
* #56: Restructured the developer guide and synchronized agent guidance
* Updated Poetry dependencies and added developer guide and added .gitignore
