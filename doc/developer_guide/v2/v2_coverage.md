# v2 Coverage Guide

## Generate a local coverage report

Install Bazel, LLVM coverage tooling, and `lcov`/`genhtml` on the local
machine. From the repository root, run the coverage tests from the v2 module:

```bash
cd udf-runner-cpp/v2
bazel test --verbose_failures \
  --collect_code_coverage \
  --combined_report=lcov \
  --build_tag_filters=-fuzz-test \
  --test_tag_filters=-fuzz-test \
  //...
```

The combined LCOV report is written to:

```text
bazel-out/_coverage/_coverage_report.dat
```

Print a text summary of the report with:

```bash
lcov --summary bazel-out/_coverage/_coverage_report.dat
```

For a file-by-file text listing, use:

```bash
lcov --list bazel-out/_coverage/_coverage_report.dat
```

Generate an HTML report from the LCOV data with:

```bash
genhtml bazel-out/_coverage/_coverage_report.dat \
  --branch-coverage \
  --fail-under-branches 80 \
  --output-directory coverage-html
```

Open `coverage-html/index.html` in a browser to inspect line and file
coverage, including branch coverage. The command fails if total branch
coverage is below 80%. The `coverage-html` directory is local build output and
should not be committed.

## Generate the CI-compatible report

To generate the report format consumed by the SonarQube configuration, add the
same coverage report generator used in CI:

```bash
cd udf-runner-cpp/v2
bazel test --verbose_failures \
  --collect_code_coverage \
  --combined_report=lcov \
  --coverage_report_generator=@bazel_sonarqube//:sonarqube_coverage_generator \
  --build_tag_filters=-fuzz-test \
  --test_tag_filters=-fuzz-test \
  //...
```

The generated report is consumed from
`bazel-out/_coverage/_coverage_report.dat`, matching
`sonar.coverageReportPaths` in `sonar-project.properties`.

Fuzz targets are excluded from these commands because they have separate
sanitizer and regression workflows documented in the [v2 fuzzing
guide](v2_fuzzing.md).
