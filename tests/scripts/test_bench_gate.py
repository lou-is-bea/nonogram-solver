"""Tests of gate G10 (docs/TESTING.md §3): scripts/bench_gate.sh and the checks of check.sh.

scripts/bench_gate.sh measures the corpus and compares it with a baseline, up to BENCH_ATTEMPTS
times. Here a stub stands for the nonogram executable: it prints a chosen measurement for each
call and records the calls, so the attempt loop and the exit codes are tested without a build.
The checks that scripts/check.sh makes before any build (NG_BENCH_MACHINE unset, invalid or
without a baseline) are tested by running "check.sh --only G10" itself.
"""

import copy
import json
import os
import stat
import subprocess
import tempfile
import unittest

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
GATE = os.path.join(ROOT, "scripts", "bench_gate.sh")
CHECK = os.path.join(ROOT, "scripts", "check.sh")

MS = 1_000_000  # nanoseconds

BASELINE = {
    "schema": "nonogram-bench/1",
    "solver": "brute",
    "clock": "monotonic",
    "seed": 0,
    "warmup_runs": 2,
    "measured_runs": 11,
    "results": [{
        "puzzle": "slow.non", "result": "SOLVED", "nodes": 10, "line_solves": 0,
        "solutions_found": 1, "workspace_bytes": 300, "peak_bytes": 300,
        "median_ns": 40 * MS, "min_ns": 40 * MS, "max_ns": 40 * MS,
    }],
}


def slower(factor):
    measurement = copy.deepcopy(BASELINE)
    measurement["results"][0]["median_ns"] = int(40 * MS * factor)
    return measurement


# The stub answers call n (1-based) with the file measurement_<n>.json, or with the last one
# when there are fewer files than calls; "fail" in place of a file makes it exit with code 3.
STUB = """#!/bin/sh
dir=$(dirname "$0")
echo "$*" >> "$dir/calls"
n=$(wc -l < "$dir/calls")
while [ "$n" -gt 0 ] && [ ! -e "$dir/measurement_$n.json" ]; do n=$((n - 1)); done
if grep -q '^fail$' "$dir/measurement_$n.json"; then exit 3; fi
cat "$dir/measurement_$n.json"
"""


class BenchGateTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.stub = self.path("nonogram")
        with open(self.stub, "w", encoding="utf-8") as file:
            file.write(STUB)
        os.chmod(self.stub, os.stat(self.stub).st_mode | stat.S_IXUSR)
        self.baseline = self.write("baseline.json", BASELINE)

    def path(self, name):
        return os.path.join(self.directory.name, name)

    def write(self, name, content):
        with open(self.path(name), "w", encoding="utf-8") as file:
            file.write(content if isinstance(content, str) else json.dumps(content))
        return self.path(name)

    def measurements(self, *answers):
        for n, answer in enumerate(answers, start=1):
            self.write(f"measurement_{n}.json", answer)

    def calls(self):
        if not os.path.exists(self.path("calls")):
            return []
        with open(self.path("calls"), encoding="utf-8") as file:
            return file.read().splitlines()

    def run_gate(self, attempts=None):
        env = dict(os.environ)
        env.pop("BENCH_ATTEMPTS", None)
        if attempts is not None:
            env["BENCH_ATTEMPTS"] = attempts
        return subprocess.run(
            [GATE, self.stub, self.baseline, self.path("current.json"), "a.non", "b.non"],
            capture_output=True, text=True, check=False, env=env)

    def test_no_regression_passes_after_one_measurement(self):
        self.measurements(BASELINE)

        completed = self.run_gate()

        self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)
        self.assertEqual(self.calls(), ["bench a.non b.non --format json"])
        self.assertIn("no blocking regression", completed.stdout)

    def test_regression_found_by_every_attempt_fails(self):
        self.measurements(slower(2))

        completed = self.run_gate()

        self.assertEqual(completed.returncode, 1, completed.stdout + completed.stderr)
        self.assertEqual(len(self.calls()), 3)
        self.assertIn("blocking regression confirmed by 3 measurements", completed.stderr)

    def test_regression_not_confirmed_passes(self):
        self.measurements(slower(2), BASELINE)

        completed = self.run_gate()

        self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)
        self.assertEqual(len(self.calls()), 2)
        self.assertIn("attempt 1 of 3, measuring again", completed.stdout)

    def test_number_of_attempts_can_be_set(self):
        self.measurements(slower(2))

        completed = self.run_gate(attempts="1")

        self.assertEqual(completed.returncode, 1, completed.stdout + completed.stderr)
        self.assertEqual(len(self.calls()), 1)

    def test_invalid_number_of_attempts_fails_without_measuring(self):
        self.measurements(BASELINE)
        for attempts in ("0", "-1", "x", ""):
            with self.subTest(attempts=attempts):
                completed = self.run_gate(attempts=attempts)

                self.assertNotEqual(completed.returncode, 0)
                self.assertIn("BENCH_ATTEMPTS must be a positive integer", completed.stderr)
                self.assertEqual(self.calls(), [])

    def test_unusable_baseline_fails_at_once(self):
        self.measurements(BASELINE)
        self.write("baseline.json", "{ not json")

        completed = self.run_gate()

        self.assertEqual(completed.returncode, 2, completed.stdout + completed.stderr)
        self.assertEqual(len(self.calls()), 1, "an input error is not measured again")

    def test_failing_measurement_fails_at_once(self):
        self.measurements("fail\n")

        completed = self.run_gate()

        self.assertEqual(completed.returncode, 3, completed.stdout + completed.stderr)
        self.assertEqual(len(self.calls()), 1)


class CheckG10Test(unittest.TestCase):
    """The checks of check.sh before any build: they exit before building the release preset."""

    @staticmethod
    def run_check(machine):
        env = dict(os.environ)
        env.pop("NG_BENCH_MACHINE", None)
        if machine is not None:
            env["NG_BENCH_MACHINE"] = machine
        return subprocess.run([CHECK, "--only", "G10"], capture_output=True, text=True,
                              check=False, env=env)

    def test_unset_machine_is_a_skip(self):
        completed = self.run_check(None)

        self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)
        self.assertIn("SKIP", completed.stdout)
        self.assertIn("NG_BENCH_MACHINE is not set", completed.stdout)
        self.assertIn("NOT RUN", completed.stdout)
        self.assertNotIn("All selected gates passed", completed.stdout)

    def test_machine_without_a_baseline_is_a_skip(self):
        completed = self.run_check("no-such-machine")

        self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)
        self.assertIn("no baseline bench/baselines/no-such-machine.json", completed.stdout)
        self.assertNotIn("All selected gates passed", completed.stdout)

    def test_machine_that_is_not_a_file_name_fails(self):
        for machine in ("../escape", "a/b", "with space"):
            with self.subTest(machine=machine):
                completed = self.run_check(machine)

                self.assertNotEqual(completed.returncode, 0)
                self.assertIn("must be a file name of bench/baselines/", completed.stderr)


if __name__ == "__main__":
    unittest.main()
