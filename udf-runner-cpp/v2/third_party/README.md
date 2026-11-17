# V2 Third-Party Dependencies

The V2 Bazel module fetches the following dependencies with `http_archive`.
Versions and SHA-256 checksums are pinned in `../MODULE.bazel`.

| Dependency | Version | Upstream revision | License | Purpose |
| --- | --- | --- | --- | --- |
| [nlohmann/json](https://github.com/nlohmann/json) | `v3.12.0` | `55f93686c01528224f448c19128836e7df245f72` | MIT | JSON representation |
| [json-schema-validator](https://github.com/pboettch/json-schema-validator) | `2.4.0` | `c780404a84dd9ba978ba26bc58d17cb43fa7bc80` | MIT | JSON Schema draft-7 validation |
| [Apache Arrow](https://arrow.apache.org/) | `25.0.0` | `apache-arrow-25.0.0` | Apache-2.0 | Core in-memory data model only |
| [xsimd](https://github.com/xtensor-stack/xsimd) | `14.2.0` | `14.2.0` | BSD-3-Clause | Arrow core CPU feature detection |

`json_schema.hpp` exposes the JSON dependencies through
`exasol::udf::v2::third_party::nlohmann`. Include the wrapper before any other
`nlohmann` JSON header in a translation unit.

Arrow is built as a static C++20 library with IPC, Parquet, compression,
storage, and cloud modules excluded.
