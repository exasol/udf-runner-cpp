# v2 Coverage Guide

## Coverage goal

For the SonarQube quality gate, **new-code coverage must be at least 80%**. This is SonarQube's combined coverage metric (new_coverage), not a requirement for 80% line coverage and 80% branch coverage separately.

SonarQube combines covered executable lines and conditions evaluated true, divided by the total executable lines and conditions, restricted to new or changed code. Line coverage and branch (condition) coverage are also reported separately, but neither percentage alone is the quality-gate value. Because the combined metric is weighted by the numbers of coverable lines and conditions, it is not the arithmetic mean of the line and branch percentages. For example, 98.74% new line coverage and 57.65% new branch coverage can yield 67.8% new coverage, which fails an 80% gate.

On a pull request, SonarQube treats changed code as new code. For branch analysis, the project's configured new-code definition determines the scope. Check the **New Coverage** condition in the SonarQube quality-gate result; it must be **>= 80%** for the gate to pass.

The local genhtml report below is useful for inspecting line and branch coverage. A branch-only percentage from that report is not a substitute for SonarQube's combined new-code coverage gate.

## Generate a local coverage report

Install Bazel, LLVM coverage tooling, and lcov/genhtml on the local
machine. From the repository root, run the coverage tests from the v2 module:

~~~bash
cd udf-runner-cpp/v2
bazel test --verbose_failures \
  --collect_code_coverage \
  --combined_report=lcov \
  --build_tag_filters=-fuzz-test \
  --test_tag_filters=-fuzz-test \
  //...
~~~

The combined LCOV report is written to:

~~~text
bazel-out/_coverage/_coverage_report.dat
~~~

Print a text summary of the report with:

~~~bash
lcov --summary bazel-out/_coverage/_coverage_report.dat
~~~

For a file-by-file text listing, use:

~~~bash
lcov --list bazel-out/_coverage/_coverage_report.dat
~~~

Generate an HTML report from the LCOV data with:

~~~bash
genhtml bazel-out/_coverage/_coverage_report.dat \
  --branch-coverage \
  --output-directory coverage-html
~~~

Open coverage-html/index.html in a browser to inspect line and branch
coverage. The coverage-html directory is local build output and should not
be committed.

## Generate the CI-compatible report

To generate the report format consumed by the SonarQube configuration, add the
same coverage report generator used in CI:

~~~bash
cd udf-runner-cpp/v2
bazel test --verbose_failures \
  --collect_code_coverage \
  --combined_report=lcov \
  --coverage_report_generator=@bazel_sonarqube//:sonarqube_coverage_generator \
  --build_tag_filters=-fuzz-test \
  --test_tag_filters=-fuzz-test \
  //...
~~~

The generated report is consumed from
bazel-out/_coverage/_coverage_report.dat, matching
sonar.coverageReportPaths in sonar-project.properties. SonarQube applies
its configured coverage exclusions and evaluates the 80% threshold against
combined coverage on new code.

Fuzz targets are excluded from these commands because they have separate
sanitizer and regression workflows documented in the [v2 fuzzing
guide](v2_fuzzing.md).
