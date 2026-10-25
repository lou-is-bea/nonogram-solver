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
GCOV="${GCOV:-gcov-14}"
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
# include/nonogram/version.h.in is not listed: its @VAR@ placeholders are not C; the generated
# header is compiled alone by the ng_header_check target.
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
# Each entry: <grep -P regex> @@ <grep -P filter on file paths> @@ <mode> @@ <message>
# Mode "code" matches the code with its comments removed; mode "text" matches the raw line (for
# rules about comments themselves).
BANNED=(
    '\bNULL\b @@ . @@ code @@ use nullptr (C23)'
    '(^|[^.>\w])(malloc|calloc|realloc|free|strdup|strndup)\s*\( @@ ^(?!src/core/allocator\.c$) @@ code @@ allocate through ng_allocator (ADR-0004)'
    '\b(strcpy|strcat|sprintf|vsprintf|gets|atoi|atol|atoll|sscanf|strtok|rand|srand|alloca)\s*\( @@ . @@ code @@ forbidden function (CODING_STANDARDS.md §8)'
    '#\s*pragma\s+once @@ . @@ code @@ use an NG_<PATH>_H include guard'
    '#\s*include\s*<stdbool\.h> @@ . @@ code @@ bool is a C23 keyword'
    '#\s*embed\b|\b_BitInt\b @@ . @@ code @@ forbidden C23 feature without an ADR'
    '\bgoto\s+(?!cleanup\b) @@ . @@ code @@ goto is only allowed as "goto cleanup;"'
    '\b(TODO|FIXME)(?!\(#\d+\)) @@ . @@ text @@ TODO/FIXME must reference a ticket: TODO(#42)'
    '\b(printf|puts|exit|_Exit|quick_exit|abort)\s*\(|\bstdout\b|\bstderr\b @@ ^src/(?!cli/) @@ code @@ no printf/exit/abort/stdout/stderr in the library'
)

# Prints "file:line:text" for every line of the files listed on stdin that matches the Perl
# regex $1. In mode "code", the line is scanned from left to right: string and character
# literals are blanked (so that "/*" or "//" inside a string is not taken for a comment, and
# their text is not taken for code), then block comments (including those that span several
# lines) and line comments are removed. A line that merely starts with '*' is code, e.g.
# "*out = NULL;". #include lines keep their path, which some rules check.
scan_lines() {
    # Left-to-right scan of each line, with the comment and literal state carried over to the
    # next line: identifiers and preprocessing numbers (C23 digit separators, 1'000) are copied
    # as code, so their apostrophes never open a character literal; literals are blanked, also
    # across a backslash-newline; comments are dropped. The header name of an #include line is
    # kept for the rules that check include paths.
    NG_RE="$1" NG_MODE="$2" xargs -r perl -ne '
        BEGIN { $re = qr/$ENV{NG_RE}/; $code_mode = $ENV{NG_MODE} eq "code"; }
        my $rest = $_;
        my $code = "";
        if (!$code_mode) {
            $code = $rest;
            $rest = "";
        } elsif (!$in_comment && !$in_literal
                 && $rest =~ s{^(\s*#\s*include\s*(?:"[^"\n]*"|<[^>\n]*>))}{}) {
            $code = $1;
        }
        while (length $rest) {
            if ($in_comment) {
                if ($rest =~ s{^.*?\*/}{}s) { $in_comment = 0; } else { $rest = ""; }
            } elsif ($in_literal) {
                my $q = $in_literal;
                if ($rest =~ s{^(?:\\.|[^\Q$q\E\\\n])*\Q$q\E}{}) {
                    $in_literal = "";
                } else {
                    $in_literal = "" unless $rest =~ m{\\\n\z};
                    $rest = "";
                }
            } elsif ($rest =~ s{^([A-Za-z_]\w*|\.?\d(?:\x27?[\w.]|[eEpP][+-])*)}{}) {
                $code .= $1;
            } elsif ($rest =~ s{^(["\x27])}{}) {
                $in_literal = $1;
                $code .= "\"\"";
            } elsif ($rest =~ s{^/\*}{}) {
                $in_comment = 1;
            } elsif ($rest =~ s{^//.*}{}s) {
                $rest = "";
            } else {
                $rest =~ s{^(.)}{}s;
                $code .= $1;
            }
        }
        print "$ARGV:$.:$_" if $code =~ $re;
        if (eof) { close ARGV; $in_comment = 0; $in_literal = ""; }'
}

split_entry() {
    local entry="$1"
    regex="${entry%% @@ *}"
    entry="${entry#* @@ }"
    filter="${entry%% @@ *}"
    entry="${entry#* @@ }"
    mode="${entry%% @@ *}"
    message="${entry#* @@ }"
}

check_banned() {
    local found=0 entry regex filter mode message files hits
    for entry in "${BANNED[@]}"; do
        split_entry "$entry"
        files=$(c_files | grep -P -- "$filter" || true)
        [[ -n "$files" ]] || continue
        hits=$(printf '%s\n' "$files" | scan_lines "$regex" "$mode" || true)
        if [[ -n "$hits" ]]; then
            printf '%s\n' "$hits" | sed "s|\$|    <- $message|" >&2
            found=1
        fi
    done
    return "$found"
}

# The forbidden-construct search is itself checked before use: every line of
# scripts/selftest/banned_hits.txt must be reported by at least one rule, and no line of
# banned_clean.txt (comments, member calls, allowed forms) may be.
selftest_banned() {
    local entry regex filter mode message all_hits=""
    for entry in "${BANNED[@]}"; do
        split_entry "$entry"
        all_hits+=$(printf '%s\n' scripts/selftest/banned_hits.txt scripts/selftest/banned_clean.txt \
            | scan_lines "$regex" "$mode" || true)
        all_hits+=$'\n'
    done
    local expected missed clean_hits
    expected=$(grep -cvE '^\s*$' scripts/selftest/banned_hits.txt)
    missed=$(grep -nvE '^\s*$' scripts/selftest/banned_hits.txt | cut -d: -f1 | while read -r n; do
        grep -q "^scripts/selftest/banned_hits.txt:$n:" <<< "$all_hits" || echo "$n"
    done)
    clean_hits=$(grep '^scripts/selftest/banned_clean.txt:' <<< "$all_hits" || true)
    if [[ -n "$missed" || -n "$clean_hits" ]]; then
        [[ -z "$missed" ]] || printf 'self-test: banned_hits.txt lines not reported: %s\n' $missed >&2
        [[ -z "$clean_hits" ]] || printf 'self-test: false positives:\n%s\n' "$clean_hits" >&2
        return 1
    fi
    echo "self-test: $expected forbidden constructs reported, no false positive"
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
    selftest_banned || fail "the forbidden-construct search failed its self-test"
    check_banned || fail "forbidden constructs found (see above)"
}

# --- G3..G5: test suites ------------------------------------------------------------------------
gate_G3() {
    say "G3 unit tests (dev, then release for the NDEBUG fallbacks of contract violations)"
    preset dev
    ctest --preset dev -L unit -j "$JOBS"
    preset release
    ctest --preset release -L unit -j "$JOBS"
}

gate_G4() {
    say "G4 integration tests"
    preset dev
    ctest --preset dev -L integration -j "$JOBS"
}

gate_G5() {
    say "G5 property and differential tests"
    preset dev
    ctest --preset dev -L property -j "$JOBS"
}

# --- G6: sanitizers ---------------------------------------------------------------------------------
gate_G6() {
    say "G6 sanitizers (asan + LSan, ubsan + integer)"
    preset asan
    ctest --preset asan -j "$JOBS"
    preset ubsan
    ctest --preset ubsan -j "$JOBS"
}

# --- G7: Valgrind memcheck ---------------------------------------------------------------------------
# Criterion runs each test in a child process and ignores the child's exit code, so the gate
# reads the per-process Valgrind logs instead of relying on exit codes.
VALGRIND_OPTIONS=(--trace-children=yes --leak-check=full --errors-for-leak-kinds=definite,indirect
    --error-exitcode=1 --suppressions=tests/valgrind/criterion.supp)

gate_G7() {
    say "G7 Valgrind memcheck (unit tests and CLI, preset dev)"
    preset dev
    local logs=build/dev/valgrind
    rm -rf "$logs"
    mkdir -p "$logs"
    local test_binary
    for test_binary in build/dev/tests/unit/test_* build/dev/tests/property/test_prop_*; do
        echo "-- $test_binary"
        valgrind "${VALGRIND_OPTIONS[@]}" --log-file="$logs/$(basename "$test_binary").%p.log" \
            "$test_binary" > /dev/null 2>&1 || true
    done
    echo "-- nonogram --version"
    valgrind "${VALGRIND_OPTIONS[@]}" --log-file="$logs/nonogram.%p.log" \
        build/dev/src/cli/nonogram --version > /dev/null 2>&1 || true

    local bad
    bad=$(grep -lE 'ERROR SUMMARY: [1-9]' "$logs"/*.log || true)
    if [[ -n "$bad" ]]; then
        printf '%s\n' "$bad" | xargs cat >&2
        fail "Valgrind reported errors (logs in $logs)"
    fi
    echo "$(find "$logs" -name '*.log' | wc -l) processes checked, no error"
}

# --- G8: coverage -------------------------------------------------------------------------------------
# Thresholds of docs/TESTING.md §6, per module: <module> <min line %> <min branch %>
COVERAGE_THRESHOLDS=(
    "verify 95 90"
    "core 90 80"
    "io 90 80"
    "render 85 75"
    "solvers 85 75"
    "cli 75 0"
)

gate_G8() {
    say "G8 coverage (preset coverage, gcovr)"
    preset coverage
    find build/coverage -name '*.gcda' -delete
    ctest --preset coverage -j "$JOBS" > /dev/null
    local entry module line_min branch_min
    for entry in "${COVERAGE_THRESHOLDS[@]}"; do
        read -r module line_min branch_min <<< "$entry"
        if ! compgen -G "src/$module/*.c" > /dev/null \
            && ! compgen -G "src/$module/**/*.c" > /dev/null; then
            echo "-- $module: no source yet"
            continue
        fi
        echo "-- $module (lines >= $line_min %, branches >= $branch_min %)"
        # Only the project objects are searched: CMake's compiler probes are instrumented too.
        local gcovr_args=(--root . --gcov-executable "$GCOV" build/coverage/src
            --filter "src/$module/" --exclude-unreachable-branches)
        local report rc=0
        report=$(gcovr "${gcovr_args[@]}" --print-summary \
            --fail-under-line "$line_min" --fail-under-branch "$branch_min") || rc=$?
        if ((rc != 0)); then
            printf '%s\n' "$report" >&2
            fail "coverage of $module below threshold"
        fi
        printf '%s\n' "$report" | grep -E '^(lines|branches):'
    done
}

# Seeds: the whole puzzle corpus; new inputs found by libFuzzer accumulate in build/fuzz/corpus.
FUZZ_SECONDS="${FUZZ_SECONDS:-60}"

gate_G9() {
    say "G9 quick fuzzing (${FUZZ_SECONDS} s per target, preset fuzz)"
    preset fuzz
    local target=non_parser
    local work="build/fuzz/corpus/$target"
    local artifacts="build/fuzz/artifacts/$target/"
    mkdir -p "$work" "$artifacts"
    "build/fuzz/tests/fuzz/fuzz_$target" "$work" tests/data/puzzles/* \
        -dict=tests/fuzz/non.dict -max_total_time="$FUZZ_SECONDS" -timeout=10 \
        -artifact_prefix="$artifacts" -print_final_stats=1 2>&1 | tail -n 20
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
