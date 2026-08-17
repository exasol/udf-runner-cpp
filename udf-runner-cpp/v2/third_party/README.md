# V2 Vendored Dependencies

The v2 protocol vendors these dependencies because both are compiled into an
isolated C++ namespace:

| Dependency | Version | Upstream revision | License |
| --- | --- | --- | --- |
| [nlohmann/json](https://github.com/nlohmann/json) | `v3.12.0` | `55f93686c01528224f448c19128836e7df245f72` | MIT |
| [json-schema-validator](https://github.com/pboettch/json-schema-validator) | `2.4.0` | `c780404a84dd9ba978ba26bc58d17cb43fa7bc80` | MIT |

Their public C++ symbols are rewritten from `nlohmann` to
`exasol::udf::v2::third_party::nlohmann`. Header include paths remain
`nlohmann/...` so the upstream source layout is preserved. The vendor copies
contain their respective upstream license files.
