#!/usr/bin/env bash
# Run the quality gates of docs/TESTING.md §3, in order, stopping at the first failure.
#
# Usage:
#   scripts/check.sh               # G0..G9 (mandatory before asking for a review)
#   scripts/check.sh --quick       # G0..G3 only, for fast iteration
#   scripts/check.sh --only G0,G2  # selected gates (used by the CI jobs)
#
# Gates that are not active yet at the current roadmap step are reported as SKIP, never as
# passed.
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.."

CLANG_FORMAT="${CLANG_FORMAT:-clang-format-20}"
CLANG_TIDY="${CLANG_TIDY:-clang-tidy-20}"
CPPCHECK="${CPPCHECK:-cppcheck}"
JOBS="${JOBS:-$(nproc)}"

ALL_GATES=(G0 G1 G2 G3 G4 G5 G6 G7 G8 G9)
QUICK_GATES=(G0 G1 G2 G3)

usage() {
    sed -n '2,10p' "$0" | sed 's/^# \{0,1\}//'
}

selected=("${ALL_GATES[@]}")
case "${1:-}" in
    "") ;;
    --quick) selected=("${QUICK_GATES[@]}") ;;
    --only)
        [[ -n "${2:-}" ]] || { usage >&2; exit 2; }
        IFS=',' read -r -a selected <<< "$2"
        ;;
    -h | --help) usage; exit 0 ;;
    *) usage >&2; exit 2 ;;
esac

say() { printf '\n\033[1;34m==> %s\033[0m\n' "$*"; }
SKIPPED=()
skip() {
    SKIPPED+=("$current_gate")
    printf '\033[1;33mSKIP\033[0m %s\n' "$*"
}
fail() { printf '\033[1;31mFAIL\033[0m %s\n' "$*" >&2; exit 1; }

# Project C sources and headers, tracked or new (ignored files such as build/ excluded).
c_files() {
    git ls-files --cached --others --exclude-standard -- '*.c' '*.h' | while read -r f; do
        if [[ -f "$f" ]]; then
            printf '%s\n' "$f"
        fi
    done
}

preset() {
    local name="$1"
    cmake --preset "$name" > /dev/null
    cmake --build --preset "$name" -- -j "$JOBS"
}

# --- G0: formatting ------------------------------------------------------------------------
gate_G0() {
    say "G0 formatting ($CLANG_FORMAT)"
    c_files | xargs -r "$CLANG_FORMAT" --dry-run --Werror
}

# --- G1: warning-free build, GCC and Clang ---------------------------------------------------
gate_G1() {
    say "G1 build without warnings (dev = GCC 14, dev-clang = Clang 20)"
    preset dev
    preset dev-clang
}

# --- G2: static analysis -----------------------------------------------------------------------
# Forbidden constructs that the compiler and clang-tidy do not catch.
# Each entry: <grep -P regex> @@ <grep -P filter on file paths> @@ <message>
BANNED=(
    '\bNULL\b @@ . @@ use nullptr (C23)'
    '(^|[^.>\w])(malloc|calloc|realloc|free)\s*\( @@ ^(?!src/core/allocator\.c$) @@ allocate through ng_allocator (ADR-0004)'
    '\b(strcpy|strcat|sprintf|vsprintf|gets|atoi|atol|atoll|sscanf|strtok|rand|srand|alloca)\s*\( @@ . @@ forbidden function (CODING_STANDARDS.md §8)'
    '#\s*pragma\s+once @@ . @@ use an NG_<PATH>_H include guard'
    '#\s*include\s*<stdbool\.h> @@ . @@ bool is a C23 keyword'
    '#\s*embed\b|\b_BitInt\b @@ . @@ forbidden C23 feature without an ADR'
    '\bgoto\s+(?!cleanup\b) @@ . @@ goto is only allowed as "goto cleanup;"'
    '\b(TODO|FIXME)(?!\(#\d+\)) @@ . @@ TODO/FIXME must reference a ticket: TODO(#42)'
    '\b(printf|puts|exit|_Exit|quick_exit|abort)\s*\(|\bstdout\b|\bstderr\b @@ ^src/(?!cli/) @@ no printf/exit/abort/stdout/stderr in the library'
)

check_banned() {
    local found=0 entry regex filter message files hits
    for entry in "${BANNED[@]}"; do
        regex="${entry%% @@ *}"
        message="${entry##* @@ }"
        filter="${entry#* @@ }"
        filter="${filter%% @@ *}"
        files=$(c_files | grep -P -- "$filter" || true)
        [[ -n "$files" ]] || continue
        # Matches on comment-only lines are ignored.
        hits=$(printf '%s\n' "$files" | xargs -r grep -nHP -- "$regex" \
            | grep -vP '^[^:]+:\d+:\s*(\*|/\*|//)' || true)
        if [[ -n "$hits" ]]; then
            printf '%s\n' "$hits" | sed "s|\$|    <- $message|" >&2
            found=1
        fi
    done
    return "$found"
}

gate_G2() {
    say "G2 static analysis"
    preset dev-clang

    echo "-- $CLANG_TIDY"
    c_files | grep -P '\.c$' | xargs -r -P "$JOBS" -n 1 "$CLANG_TIDY" -p build/dev-clang --quiet

    echo "-- $CPPCHECK"
    # Criterion's registration macros are opaque to cppcheck, so the Criterion test files are
    # left to clang-tidy; everything else is analysed.
    "$CPPCHECK" --enable=warning,style,performance,portability --inline-suppr \
        --error-exitcode=1 --quiet -I include -I build/dev-clang/include src tests/support

    echo "-- forbidden constructs"
    check_banned || fail "forbidden constructs found (see above)"
}

# --- G3..G5: test suites ------------------------------------------------------------------------
gate_G3() {
    say "G3 unit tests"
    preset dev
    ctest --preset dev -L unit -j "$JOBS"
}

gate_G4() {
    say "G4 integration tests"
    preset dev
    ctest --preset dev -L integration -j "$JOBS"
}

gate_G5() {
    say "G5 property and differential tests"
    skip "active from M3 (no property test yet)"
}

# --- G6: sanitizers ---------------------------------------------------------------------------------
gate_G6() {
    say "G6 sanitizers (asan + LSan, ubsan + integer)"
    preset asan
    ctest --preset asan -j "$JOBS"
    preset ubsan
    ctest --preset ubsan -j "$JOBS"
}

# --- G7..G9: active from M1 -------------------------------------------------------------------------
gate_G7() {
    say "G7 Valgrind memcheck"
    skip "active from M1"
}

gate_G8() {
    say "G8 coverage"
    skip "active from M1"
}

gate_G9() {
    say "G9 quick fuzzing"
    skip "active from M1 (first fuzz target: fuzz_non_parser)"
}

for gate in "${selected[@]}"; do
    if ! declare -F "gate_$gate" > /dev/null; then
        fail "unknown gate '$gate' (expected one of: ${ALL_GATES[*]})"
    fi
done

# Gates run as plain commands: calling them as `gate || ...` would disable `set -e` inside the
# function and let a failing command in the middle of a gate go unnoticed.
current_gate=""
report_failure() {
    local rc=$?
    if ((rc != 0)) && [[ -n "$current_gate" ]]; then
        printf '\033[1;31mFAIL\033[0m gate %s (exit code %d)\n' "$current_gate" "$rc" >&2
    fi
}
trap report_failure EXIT

for gate in "${selected[@]}"; do
    current_gate="$gate"
    "gate_$gate"
done
current_gate=""

if ((${#SKIPPED[@]} > 0)); then
    say "No gate failed. NOT RUN (inactive at this roadmap step): ${SKIPPED[*]}"
else
    say "All selected gates passed: ${selected[*]}"
fi
