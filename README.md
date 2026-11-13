# udf-runner-cpp

This repository contains the extracted `exaudfclient` C++ code and related build tooling from `exasol/script-languages`.

The retained runner lives under `udf-runner-cpp/v1`. The newer v2 Bazel module
lives under `udf-runner-cpp/v2`.

The v2 Arrow helpers create run arrays from `values` and `counts` (run lengths).
`create_range_run([1, 3], [3, 4])` represents `[1, 2, 3, 3, 4, 5, 6]`, and
`create_run_end([10, 20], [2, 2])` represents `[10, 10, 20, 20]`. Callers choose
run boundaries, including group boundaries when the protocol requires them.
