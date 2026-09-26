import os
import unittest
from pathlib import Path
from unittest.mock import patch

from noxfile import (
    _MullMutationResult,
    _parse_mull_mutation_result,
    _warn_about_zero_mutants,
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


if __name__ == "__main__":
    unittest.main()
