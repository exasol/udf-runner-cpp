#!/usr/bin/env python3

from exasol_python_test_framework import udf


class SmokeTest(udf.TestCase):
    def test_db_connection(self):
        rows = self.query("SELECT 1 FROM dual")
        self.assertRowsEqual([(1,)], rows)


if __name__ == "__main__":
    udf.main()
