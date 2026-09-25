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
            _parse_mull_mutation_result("[info] Killed mutants (4/4):"),
            _MullMutationResult(4, 4),
        )

    def test_parses_surviving_mutants(self):
        self.assertEqual(
            _parse_mull_mutation_result("[info] Survived mutants (2/5):"),
            _MullMutationResult(3, 5),
        )

    def test_parses_zero_mutants(self):
        self.assertEqual(
            _parse_mull_mutation_result("[info] No mutants found"),
            _MullMutationResult(0, 0),
        )

    def test_rejects_unrecognized_output(self):
        self.assertIsNone(_parse_mull_mutation_result("Mull exited successfully"))

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
