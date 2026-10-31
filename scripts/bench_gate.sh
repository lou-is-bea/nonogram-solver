#!/usr/bin/env bash
# Measure and compare for gate G10 (docs/TESTING.md §3), called by scripts/check.sh once the
# release preset is built; tested by tests/scripts/test_bench_gate.py with a stub executable.
#
# Usage:
#   scripts/bench_gate.sh <nonogram> <baseline.json> <current.json> <puzzle.non>...
#
# Runs "<nonogram> bench <puzzle.non>... --format json" into <current.json> and compares it with
# <baseline.json> (scripts/bench_compare.py), up to BENCH_ATTEMPTS times (default 3). Right
# after the CPU-bound gates (G9 fuzzing), a first measurement can be more than 10 % slow: the
# gate fails only when every attempt finds a blocking regression (not necessarily on the same
# puzzle); a memory regression, being deterministic, is found every time.
#
# Exit codes: 0 no blocking regression; 1 a blocking regression found by every attempt; any
# other non-zero code at once, with no new attempt: an invalid BENCH_ATTEMPTS, a failed
# measurement (code of the executable), or an input bench_compare.py cannot compare (code 2).
set -euo pipefail

if (($# < 4)); then
    sed -n '5,6p' "$0" | sed 's/^# \{0,1\}//' >&2
    exit 2
fi
nonogram="$1"
baseline="$2"
current="$3"
shift 3

# Unset means the default; an empty or invalid value is an error, never a silent default.
attempts="${BENCH_ATTEMPTS-3}"
if [[ ! "$attempts" =~ ^[1-9][0-9]*$ ]]; then
    echo "G10: BENCH_ATTEMPTS must be a positive integer, got '$attempts'" >&2
    exit 2
fi

compare="$(dirname "${BASH_SOURCE[0]}")/bench_compare.py"
for ((attempt = 1; attempt <= attempts; ++attempt)); do
    "$nonogram" bench "$@" --format json > "$current"
    rc=0
    python3 "$compare" "$baseline" "$current" || rc=$?
    if ((rc != 1)); then
        exit "$rc"
    fi
    if ((attempt < attempts)); then
        echo "G10: blocking regression in attempt $attempt of $attempts, measuring again"
    fi
done
echo "G10: blocking regression confirmed by $attempts measurements" >&2
exit 1
