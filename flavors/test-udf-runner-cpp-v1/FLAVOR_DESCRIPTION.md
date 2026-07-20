This flavor builds and smoke-tests the retained `udf-runner-cpp/v1` launcher surface.

It is intentionally minimal:

- build the reduced C++ runner bundle
- publish `language_definitions.json` for benchmark and streaming modes
- run one lightweight smoke test from `test_container`
