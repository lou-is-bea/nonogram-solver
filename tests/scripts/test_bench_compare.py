"""Tests of scripts/bench_compare.py (docs/TESTING.md §4.8, gate G10).

Each test writes a baseline and a current measurement in the nonogram-bench/1 schema to a
temporary directory, runs the script as a separate process, and checks its exit code and output.
Only the standard library is used (no new dependency, AGENTS.md rule 7).
"""

import copy
import json
import os
import subprocess
import sys
import tempfile
import unittest

SCRIPT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "scripts",
                      "bench_compare.py")

MS = 1_000_000  # nanoseconds


def result(puzzle, median_ns, peak_bytes=300, nodes=1000):
    return {
        "puzzle": puzzle,
        "result": "SOLVED",
        "nodes": nodes,
        "line_solves": 0,
        "solutions_found": 1,
        "workspace_bytes": peak_bytes,
        "peak_bytes": peak_bytes,
        "median_ns": median_ns,
        "min_ns": median_ns,
        "max_ns": median_ns,
    }


def measurement(*results):
    return {
        "schema": "nonogram-bench/1",
        "solver": "brute",
        "clock": "monotonic",
        "seed": 0,
        "warmup_runs": 2,
        "measured_runs": 11,
        "results": list(results),
    }


# A slow puzzle (above the 1 ms floor) and a fast one (below it).
BASELINE = measurement(result("slow.non", 40 * MS, peak_bytes=1000),
                       result("fast.non", 20_000, peak_bytes=200))


class BenchCompareTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)

    def write(self, name, content):
        path = os.path.join(self.directory.name, name)
        with open(path, "w", encoding="utf-8") as file:
            if isinstance(content, str):
                file.write(content)
            else:
                json.dump(content, file)
        return path

    def run_compare(self, current, baseline=BASELINE, *options):
        baseline_path = baseline if isinstance(baseline, str) and baseline.startswith("/") \
            else self.write("baseline.json", baseline)
        current_path = self.write("current.json", current)
        completed = subprocess.run(
            [sys.executable, "-I", SCRIPT, baseline_path, current_path, *options],
            capture_output=True, text=True, check=False)
        return completed

    @staticmethod
    def changed(**changes):
        """BASELINE with results modified: changes = {puzzle: {field: value}}."""
        current = copy.deepcopy(BASELINE)
        for entry in current["results"]:
            for field, value in changes.get(entry["puzzle"].replace(".non", ""), {}).items():
                entry[field] = value
        return current

    # --- No regression -----------------------------------------------------------------------

    def test_identical_measurements_pass(self):
        completed = self.run_compare(BASELINE)

        self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)
        self.assertIn("no blocking regression", completed.stdout)

    def test_exactly_ten_percent_is_not_a_regression(self):
        completed = self.run_compare(self.changed(slow={"median_ns": 44 * MS, "peak_bytes": 1100}))

        self.assertEqual(completed.returncode, 0, completed.stdout)

    def test_improvements_are_reported(self):
        completed = self.run_compare(self.changed(slow={"median_ns": 20 * MS, "peak_bytes": 500}))

        self.assertEqual(completed.returncode, 0, completed.stdout)
        self.assertIn("slow.non: time improved", completed.stdout)
        self.assertIn("slow.non: memory improved", completed.stdout)

    def test_small_time_improvements_are_noise(self):
        completed = self.run_compare(self.changed(slow={"median_ns": 37 * MS}))

        self.assertEqual(completed.returncode, 0, completed.stdout)
        self.assertNotIn("slow.non", completed.stdout)

    # --- Memory ------------------------------------------------------------------------------

    def test_memory_regression_above_the_threshold_blocks(self):
        completed = self.run_compare(self.changed(fast={"peak_bytes": 223}))

        self.assertEqual(completed.returncode, 1, completed.stdout)
        self.assertIn("fast.non: memory regression +11.5 % (200 -> 223 bytes) [BLOCKING]",
                      completed.stdout)

    def test_memory_regression_blocks_even_when_time_only_reports(self):
        completed = self.run_compare(self.changed(slow={"peak_bytes": 2000}), BASELINE,
                                     "--time-mode", "report")

        self.assertEqual(completed.returncode, 1, completed.stdout)

    def test_memory_from_zero_is_a_regression(self):
        baseline = self.changed(fast={"peak_bytes": 0})
        completed = self.run_compare(BASELINE, baseline)

        self.assertEqual(completed.returncode, 1, completed.stdout)
        self.assertIn("fast.non: memory regression (0 -> 200 bytes) [BLOCKING]", completed.stdout)

    # --- Time --------------------------------------------------------------------------------

    def test_time_regression_above_the_floor_blocks(self):
        completed = self.run_compare(self.changed(slow={"median_ns": 45 * MS}))

        self.assertEqual(completed.returncode, 1, completed.stdout)
        self.assertIn("slow.non: time regression +12.5 % (40000000 -> 45000000 ns) [BLOCKING]",
                      completed.stdout)

    def test_time_regression_below_the_floor_is_only_reported(self):
        completed = self.run_compare(self.changed(fast={"median_ns": 40_000}))

        self.assertEqual(completed.returncode, 0, completed.stdout)
        self.assertIn("fast.non: time regression +100.0 % (20000 -> 40000 ns) "
                      "[reported: baseline below the 1000000 ns floor]", completed.stdout)

    def test_time_regression_is_only_reported_in_report_mode(self):
        completed = self.run_compare(self.changed(slow={"median_ns": 80 * MS}), BASELINE,
                                     "--time-mode", "report")

        self.assertEqual(completed.returncode, 0, completed.stdout)
        self.assertIn("slow.non: time regression +100.0 % (40000000 -> 80000000 ns) "
                      "[reported: --time-mode report]", completed.stdout)

    def test_thresholds_and_floor_can_be_set(self):
        current = self.changed(fast={"median_ns": 21_000, "peak_bytes": 204})

        completed = self.run_compare(current, BASELINE, "--time-threshold", "4",
                                     "--memory-threshold", "1", "--time-floor-ns", "10000")

        self.assertEqual(completed.returncode, 1, completed.stdout)
        self.assertIn("fast.non: time regression +5.0 %", completed.stdout)
        self.assertIn("fast.non: memory regression +2.0 %", completed.stdout)

    # --- Counters and puzzles ----------------------------------------------------------------

    def test_changed_counters_are_reported_without_blocking(self):
        completed = self.run_compare(self.changed(slow={"nodes": 999, "result": "INCOMPLETE"}))

        self.assertEqual(completed.returncode, 0, completed.stdout)
        self.assertIn("slow.non: nodes changed (1000 -> 999)", completed.stdout)
        self.assertIn("slow.non: result changed (SOLVED -> INCOMPLETE)", completed.stdout)

    def test_new_puzzle_is_reported(self):
        current = copy.deepcopy(BASELINE)
        current["results"].append(result("new.non", 10))

        completed = self.run_compare(current)

        self.assertEqual(completed.returncode, 0, completed.stdout)
        self.assertIn("new.non: not in the baseline", completed.stdout)

    # --- Errors (exit 2) ---------------------------------------------------------------------

    def assert_error(self, completed, message):
        self.assertEqual(completed.returncode, 2, completed.stdout + completed.stderr)
        self.assertIn(message, completed.stderr)

    def test_missing_baseline_is_an_error(self):
        completed = self.run_compare(BASELINE, os.path.join(self.directory.name, "none.json"))

        self.assert_error(completed, "cannot read")

    def test_malformed_json_is_an_error(self):
        completed = self.run_compare(BASELINE, "{ not json")

        self.assert_error(completed, "not valid JSON")

    def test_wrong_schema_is_an_error(self):
        baseline = copy.deepcopy(BASELINE)
        baseline["schema"] = "nonogram-solve/1"

        self.assert_error(self.run_compare(BASELINE, baseline),
                          "schema 'nonogram-solve/1', expected 'nonogram-bench/1'")

    def test_missing_field_is_an_error(self):
        baseline = copy.deepcopy(BASELINE)
        del baseline["results"][0]["peak_bytes"]

        self.assert_error(self.run_compare(BASELINE, baseline), "missing or invalid 'peak_bytes'")

    def test_negative_or_non_integer_field_is_an_error(self):
        for value in (-1, 1.5, "12", True):
            with self.subTest(value=value):
                current = self.changed(slow={"median_ns": value})

                self.assert_error(self.run_compare(current), "missing or invalid 'median_ns'")

    def test_duplicate_puzzle_is_an_error(self):
        current = copy.deepcopy(BASELINE)
        current["results"].append(copy.deepcopy(current["results"][0]))

        self.assert_error(self.run_compare(current), "slow.non appears twice")

    def test_puzzle_missing_from_the_current_measurement_is_an_error(self):
        current = copy.deepcopy(BASELINE)
        del current["results"][1]

        self.assert_error(self.run_compare(current), "fast.non: in the baseline but not measured")

    def test_different_solvers_are_an_error(self):
        current = copy.deepcopy(BASELINE)
        current["solver"] = "line"

        self.assert_error(self.run_compare(current), "solver 'line', baseline solver 'brute'")

    def test_invalid_options_are_errors(self):
        for options in (["--time-threshold", "-1"], ["--time-mode", "maybe"],
                        ["--time-floor-ns", "x"]):
            with self.subTest(options=options):
                completed = self.run_compare(BASELINE, BASELINE, *options)

                self.assertEqual(completed.returncode, 2, completed.stdout + completed.stderr)


if __name__ == "__main__":
    unittest.main()
