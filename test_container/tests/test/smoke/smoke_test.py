#!/usr/bin/env python3

from pathlib import Path

from exasol_python_test_framework import udf


class SmokeTest(udf.TestCase):
    def test_db_connection(self):
        rows = self.query("SELECT 1 FROM dual")
        self.assertRowsEqual([(1,)], rows)

    def test_release_payload_present(self):
        bucket_root = Path("/buckets/bfsdefault/myudfs")
        language_definitions = list(bucket_root.glob("**/language_definitions.json"))
        udf_clients = list(bucket_root.glob("**/exaudf/exaudfclient"))
        self.assertGreaterEqual(len(language_definitions), 1)
        self.assertGreaterEqual(len(udf_clients), 1)


if __name__ == "__main__":
    udf.main()
