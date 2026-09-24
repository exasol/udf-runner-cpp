# v2 Dependency and Symbol-Leak Policy

Whenever a third-party dependency is added or modified, evaluate whether its
symbols, headers, macros, or runtime dependencies can leak into the public v2
library or shared-object interface. The evaluation must cover the relevant
shared and static artifacts and must result in tests or documented evidence.

If symbols can leak, use appropriate isolation measures, such as:

- keeping the dependency behind a private Bazel target or implementation
  wrapper;
- rewriting namespaces, include paths, and include guards for vendored headers
  when the dependency can collide with a consumer installation;
- compiling with hidden visibility and explicitly exporting only the required
  API;
- using linker version scripts and `--exclude-libs` to suppress archive symbols;
- isolating the dependency in a dedicated shared library or linker namespace;
- avoiding third-party dependencies through public Bazel targets; and
- adding or updating `nm`-based tests for dynamic symbols and static archives.

The existing private FlatBuffers runtime, rewritten generated includes, hidden
visibility, version scripts, and symbol-leak tests demonstrate these patterns.
Do not treat a successful compile as sufficient evidence that a new dependency
is isolated.
