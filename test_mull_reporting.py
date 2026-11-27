import os
import unittest
from pathlib import Path
from unittest.mock import patch

from noxfile import (
    _MullMutationResult,
    _mull_target_binary_path,
    _mull_target_labels,
    _mull_target_report_name,
    _normalize_mull_target,
    _parse_mull_mutation_result,
    _warn_about_zero_mutants,
)
from tools.generate_mull_config import (
    _render_include_paths,
    _select_mull_sources,
    _source_paths,
)


class MullMutationReportingTest(unittest.TestCase):
    def test_parses_killed_mutants(self):
        self.assertEqual(
            _parse_mull_mutation_result(
                "[info] Killed mutants (4/4):",
                {"files": {"example.cc": {"mutants": [{"status": "Timeout"}] * 4}}},
            ),
            _MullMutationResult(4, 4),
        )

    def test_parses_non_killed_mutants(self):
        self.assertEqual(
            _parse_mull_mutation_result(
                "[info] Survived mutants (2/5):",
                {
                    "files": {
                        "example.cc": {
                            "mutants": [
                                {"status": "Killed"},
                                {"status": "Killed"},
                                {"status": "Killed"},
                                {"status": "Survived"},
                                {"status": "Timeout"},
                            ]
                        }
                    }
                }
            ),
            _MullMutationResult(3, 5),
        )

    def test_parses_zero_mutants(self):
        self.assertEqual(
            _parse_mull_mutation_result(
                "[info] No mutants found", {"files": {"example.cc": {"mutants": []}}}
            ),
            _MullMutationResult(0, 0),
        )

    def test_parses_surviving_mutants_summary(self):
        self.assertEqual(
            _parse_mull_mutation_result(
                "[info] Surviving mutants: 8",
                {"files": {"example.cc": {"mutants": [{"status": "Survived"}] * 8}}},
            ),
            _MullMutationResult(0, 8),
        )

    def test_rejects_malformed_report(self):
        self.assertIsNone(
            _parse_mull_mutation_result("Mull exited successfully", {"files": {"example.cc": {}}})
        )

    def test_warns_locally_without_github_annotation(self):
        with (
            patch.dict(os.environ, {"GITHUB_ACTIONS": "false"}),
            patch("builtins.print") as print_mock,
        ):
            _warn_about_zero_mutants("example_test", Path("report.txt"))

        print_mock.assert_called_once()
        self.assertTrue(print_mock.call_args.args[0].startswith("WARNING:"))

    def test_emits_github_annotation(self):
        with (
            patch.dict(os.environ, {"GITHUB_ACTIONS": "true"}),
            patch("builtins.print") as print_mock,
        ):
            _warn_about_zero_mutants("example_test", Path("report.txt"))

        self.assertEqual(print_mock.call_count, 2)
        self.assertTrue(print_mock.call_args.args[0].startswith("::warning"))


class MullTargetDiscoveryTest(unittest.TestCase):
    def test_discovers_root_and_internal_targets_with_full_labels(self):
        targets = _mull_target_labels(
            "\n".join(
                (
                    "//:event_fd_test",
                    "//test/internal:call_impl_test",
                    "//test/internal:context_handle_integration_test",
                    "//test/internal:context_impl_test",
                    "//test/internal:context_state_test",
                    "//test/internal:control_stream_impl_test",
                    "//test/internal:stream_registry_test",
                    "//test/interface:interface_test",
                )
            )
        )

        self.assertEqual(
            targets,
            (
                "//:event_fd_test",
                "//test/internal:call_impl_test",
                "//test/internal:context_handle_integration_test",
                "//test/internal:context_impl_test",
                "//test/internal:context_state_test",
                "//test/internal:control_stream_impl_test",
                "//test/internal:stream_registry_test",
            ),
        )

    def test_normalizes_full_package_and_legacy_short_targets(self):
        self.assertEqual(
            _normalize_mull_target("//test/internal:stream_registry_test"),
            "//test/internal:stream_registry_test",
        )
        self.assertEqual(
            _normalize_mull_target("test/internal:stream_registry_test"),
            "//test/internal:stream_registry_test",
        )
        self.assertEqual(_normalize_mull_target("event_fd_test"), "//:event_fd_test")

    def test_builds_package_aware_paths_and_report_names(self):
        target = "//test/internal:context_state_test"

        self.assertEqual(
            _mull_target_binary_path(target),
            Path("bazel-bin/test/internal/context_state_test"),
        )
        self.assertEqual(
            _mull_target_report_name(target), "test_internal_context_state_test"
        )

    def test_builds_binary_path_for_root_package_target(self):
        self.assertEqual(
            _mull_target_binary_path("//:mutation_smoke_test"),
            Path("bazel-bin/mutation_smoke_test"),
        )


class MullSourceSelectionTest(unittest.TestCase):
    def test_selects_only_direct_production_translation_units(self):
        sources = _source_paths(
            [
                "//:internal/context_state.cc",
                "//:interface/call_message_builder.cc",
                "//test/internal:context_state_test.cc",
                "//test/internal:stream_management_test_utils.hpp",
                "@googletest//:gtest.cc",
            ]
        )

        self.assertEqual(
            sources,
            (
                "interface/call_message_builder.cc",
                "internal/context_state.cc",
            ),
        )

    def test_measures_the_implementation_matching_a_focused_test_target(self):
        self.assertEqual(
            _select_mull_sources(
                ("internal/context_state.cc", "internal/stream_registry.cc"),
                "//test/internal:context_state_test",
            ),
            ("internal/context_state.cc",),
        )

    def test_empty_source_set_uses_a_valid_never_match_expression(self):
        self.assertEqual(_render_include_paths(()), 'includePaths:\n  - "^$"\n')

    def test_source_path_is_escaped_as_an_exact_match(self):
        self.assertEqual(
            _render_include_paths(("internal/context_state.cc",)),
            'includePaths:\n  - "(^|.*/)internal/context_state\\\\.cc$"\n',
        )


if __name__ == "__main__":
    unittest.main()
