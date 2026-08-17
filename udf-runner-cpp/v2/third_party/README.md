# V2 Third-Party Dependencies

The V2 Bazel module fetches the following MIT-licensed dependencies with
`http_archive`. Versions and SHA-256 checksums are pinned in `../MODULE.bazel`.

| Dependency | Version | Upstream revision | License |
| --- | --- | --- | --- |
| [nlohmann/json](https://github.com/nlohmann/json) | `v3.12.0` | `55f93686c01528224f448c19128836e7df245f72` | MIT |
| [json-schema-validator](https://github.com/pboettch/json-schema-validator) | `2.4.0` | `c780404a84dd9ba978ba26bc58d17cb43fa7bc80` | MIT |

`json_schema.hpp` exposes both libraries through
`exasol::udf::v2::third_party::nlohmann`. It applies the namespace rewrite only
while including the upstream headers, and the validator uses the same rewrite
privately while it is compiled. Include the wrapper before any other
`nlohmann` JSON header in a translation unit.
