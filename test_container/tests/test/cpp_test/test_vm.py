#!/usr/bin/env python3

from exasol_python_test_framework import udf


class TestVmTest(udf.TestCase):
    def setUp(self):
        self.query("DROP SCHEMA CPP_TEST CASCADE", ignore_errors=True)
        self.query("CREATE SCHEMA CPP_TEST")
        self.query("OPEN SCHEMA CPP_TEST")

    def tearDown(self):
        self.query("DROP SCHEMA CPP_TEST CASCADE", ignore_errors=True)

    def test_emit_metadata(self):
        self.query(
            "CREATE CPP_TEST SET SCRIPT emit_metadata(ignored VARCHAR(1)) "
            "EMITS (script_user VARCHAR(128), script_code VARCHAR(128)) "
            "AS emit_metadata"
        )

        rows = self.query("SELECT emit_metadata('x') FROM DUAL")
        self.assertRowsEqual([("SYS", "emit_metadata")], rows)

    def test_forward_input(self):
        self.query(
            "CREATE CPP_TEST SET SCRIPT forward_input(value VARCHAR(128)) "
            "EMITS (value VARCHAR(128)) AS forward_input"
        )
        self.query("CREATE TABLE input_values (value VARCHAR(128))")
        self.query("INSERT INTO input_values VALUES ('first'), (NULL), ('last')")

        rows = self.query("""
            SELECT value
            FROM (SELECT forward_input(value) AS value FROM input_values)
            ORDER BY value NULLS FIRST
        """)
        self.assertRowsEqual([(None,), ("first",), ("last",)], rows)

    def test_unsupported_strategy_fails(self):
        self.query(
            "CREATE CPP_TEST SET SCRIPT unknown_strategy() "
            "EMITS (value VARCHAR(128)) AS unknown_strategy"
        )

        with self.assertRaisesRegex(Exception, "unsupported test strategy: unknown_strategy"):
            self.query("SELECT * FROM unknown_strategy()")


if __name__ == "__main__":
    udf.main()
