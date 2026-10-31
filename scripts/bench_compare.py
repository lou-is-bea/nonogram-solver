#!/usr/bin/env python3
"""Compare a benchmark measurement with a baseline (docs/TESTING.md §4.8, gate G10, NF-10).

Usage:
    scripts/bench_compare.py <baseline.json> <current.json> [--time-threshold <%>]
                             [--memory-threshold <%>] [--time-floor-ns <ns>]
                             [--time-mode block|report]

Both files are outputs of "nonogram bench --format json" (schema nonogram-bench/1, FILE_FORMATS.md
§4) with the same solver. For each puzzle of the baseline:
- a peak memory more than --memory-threshold % above the baseline (10 by default) is a blocking
  regression: the peak is deterministic;
- a median time more than --time-threshold % above the baseline (10 by default) is a blocking
  regression when the baseline median is at least --time-floor-ns (1 ms by default) and
  --time-mode is "block"; otherwise it is only reported, since short times are noisy;
- improvements (any for memory, beyond the threshold for time), changed counters (result, nodes,
  line solves, solutions, workspace) and puzzles absent from the baseline are reported.

Exit codes: 0 no blocking regression; 1 at least one blocking regression; 2 usage error or
unreadable, malformed or incomparable input. Only the standard library is used.
"""

import argparse
import json
import math
import re
import sys

SCHEMA = "nonogram-bench/1"
COUNTERS = ("result", "nodes", "line_solves", "solutions_found", "workspace_bytes")
INTEGER_FIELDS = ("nodes", "line_solves", "solutions_found", "workspace_bytes", "peak_bytes",
                  "median_ns", "min_ns", "max_ns")
RESULTS = ("SOLVED", "UNSOLVABLE", "INCOMPLETE", "LIMIT_REACHED")


class InputError(Exception):
    """An input file cannot be compared; the message names the file and the problem."""


def non_negative(text):
    try:
        value = float(text)
    except ValueError:
        raise argparse.ArgumentTypeError(f"not a number: {text!r}") from None
    if not math.isfinite(value) or value < 0:
        raise argparse.ArgumentTypeError(f"must be a finite, non-negative number: {text!r}")
    return value


def non_negative_integer(text):
    # str.isdigit() also accepts digits such as "²" that int() rejects: ASCII only.
    if re.fullmatch(r"[0-9]+", text) is None:
        raise argparse.ArgumentTypeError(f"not a non-negative integer: {text!r}")
    return int(text)


def parse_args(argv):
    parser = argparse.ArgumentParser(
        description="Compare a nonogram-bench/1 measurement with a baseline.")
    parser.add_argument("baseline", help="baseline measurement (bench/baselines/<machine>.json)")
    parser.add_argument("current", help="measurement to judge")
    parser.add_argument("--time-threshold", type=non_negative, default=10.0,
                        help="time regression threshold in %% (default 10)")
    parser.add_argument("--memory-threshold", type=non_negative, default=10.0,
                        help="peak memory regression threshold in %% (default 10)")
    parser.add_argument("--time-floor-ns", type=non_negative_integer, default=1_000_000,
                        help="baseline medians below this are too noisy to block (default 1 ms)")
    parser.add_argument("--time-mode", choices=("block", "report"), default="block",
                        help="whether time regressions above the floor block (default block)")
    return parser.parse_args(argv)


def is_count(value):
    # bool is an int in Python: reject it explicitly.
    return isinstance(value, int) and not isinstance(value, bool) and value >= 0


def check_result(path, entry):
    if not isinstance(entry, dict) or not isinstance(entry.get("puzzle"), str):
        raise InputError(f"{path}: every result needs a 'puzzle' string")
    puzzle = entry["puzzle"]
    if entry.get("result") not in RESULTS:
        raise InputError(f"{path}: {puzzle}: missing or invalid 'result'")
    for field in INTEGER_FIELDS:
        if not is_count(entry.get(field)):
            raise InputError(f"{path}: {puzzle}: missing or invalid '{field}'")


def load(path):
    """The measurement in @p path, checked, as {"solver": str, "results": {puzzle: entry}}."""
    try:
        with open(path, encoding="utf-8") as file:
            text = file.read()
    except OSError as error:
        raise InputError(f"cannot read {path}: {error.strerror}") from None
    except UnicodeDecodeError:
        raise InputError(f"{path}: not UTF-8 text") from None
    try:
        data = json.loads(text)
    except json.JSONDecodeError as error:
        raise InputError(f"{path}: not valid JSON ({error})") from None
    except RecursionError:
        raise InputError(f"{path}: JSON nested too deeply") from None
    if not isinstance(data, dict):
        raise InputError(f"{path}: not a JSON object")
    if data.get("schema") != SCHEMA:
        raise InputError(f"{path}: schema {data.get('schema')!r}, expected {SCHEMA!r}")
    if not isinstance(data.get("solver"), str) or not isinstance(data.get("results"), list):
        raise InputError(f"{path}: missing or invalid 'solver' or 'results'")
    results = {}
    for entry in data["results"]:
        check_result(path, entry)
        if entry["puzzle"] in results:
            raise InputError(f"{path}: {entry['puzzle']} appears twice")
        results[entry["puzzle"]] = entry
    return {"solver": data["solver"], "results": results}


def percent(before, after):
    return 100.0 * (after - before) / before


def compare_memory(puzzle, before, after, args):
    """Lines about the peak memory, and whether one is a blocking regression."""
    if after > before and before == 0:
        return [f"{puzzle}: memory regression (0 -> {after} bytes) [BLOCKING]"], True
    if after > before and percent(before, after) > args.memory_threshold:
        return [f"{puzzle}: memory regression +{percent(before, after):.1f} % "
                f"({before} -> {after} bytes) [BLOCKING]"], True
    if after < before:
        return [f"{puzzle}: memory improved {percent(before, after):.1f} % "
                f"({before} -> {after} bytes)"], False
    return [], False


def compare_time(puzzle, before, after, args):
    """Lines about the median time, and whether one is a blocking regression."""
    if before == 0 or after == before:
        return [], False
    change = percent(before, after)
    span = f"({before} -> {after} ns)"
    if change < -args.time_threshold:
        return [f"{puzzle}: time improved {change:.1f} % {span}"], False
    if change <= args.time_threshold:
        return [], False  # within the noise, in either direction
    line = f"{puzzle}: time regression +{change:.1f} % {span}"
    if before < args.time_floor_ns:
        return [f"{line} [reported: baseline below the {args.time_floor_ns} ns floor]"], False
    if args.time_mode == "report":
        return [f"{line} [reported: --time-mode report]"], False
    return [f"{line} [BLOCKING]"], True


def compare(baseline, current, args):
    """Report lines and whether a blocking regression was found."""
    if baseline["solver"] != current["solver"]:
        raise InputError(f"current measurement: solver '{current['solver']}', "
                         f"baseline solver '{baseline['solver']}'")
    for puzzle in baseline["results"]:
        if puzzle not in current["results"]:
            raise InputError(f"{puzzle}: in the baseline but not measured")
    lines = []
    blocking = False
    for puzzle, before in baseline["results"].items():
        after = current["results"][puzzle]
        for counter in COUNTERS:
            if before[counter] != after[counter]:
                lines.append(f"{puzzle}: {counter} changed ({before[counter]} -> {after[counter]})")
        for check, field in ((compare_memory, "peak_bytes"), (compare_time, "median_ns")):
            found, blocks = check(puzzle, before[field], after[field], args)
            lines.extend(found)
            blocking = blocking or blocks
    lines.extend(f"{puzzle}: not in the baseline" for puzzle in current["results"]
                 if puzzle not in baseline["results"])
    return lines, blocking


def main(argv):
    args = parse_args(argv)
    try:
        baseline = load(args.baseline)
        current = load(args.current)
        lines, blocking = compare(baseline, current, args)
    except InputError as error:
        print(f"bench_compare: {error}", file=sys.stderr)
        return 2
    except Exception as error:
        # Exit code 1 means "blocking regression": a bug of this script must not look like one.
        print(f"bench_compare: internal error: {error!r}", file=sys.stderr)
        return 2
    print(f"bench_compare: {args.current} against {args.baseline} (solver "
          f"{baseline['solver']}, {len(baseline['results'])} puzzles; time > "
          f"{args.time_threshold:g} % above {args.time_floor_ns} ns ({args.time_mode}), "
          f"memory > {args.memory_threshold:g} %)")
    for line in lines:
        print(f"  {line}")
    print("bench_compare: blocking regression found" if blocking
          else "bench_compare: no blocking regression")
    return 1 if blocking else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
