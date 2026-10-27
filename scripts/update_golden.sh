#!/usr/bin/env bash
# Rewrite the CLI reference outputs (tests/data/expected) from the current build.
#
# Every rewritten file must be reviewed in the diff before committing, and the PR must justify
# the change (TESTING.md §4.5). Usage: scripts/update_golden.sh [ctest -R regex]
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.."

cmake --preset dev > /dev/null
cmake --build --preset dev
NG_UPDATE_GOLDEN=1 ctest --preset dev -L integration ${1:+-R "$1"} > /dev/null || true
ctest --preset dev -L integration ${1:+-R "$1"}
git status --short -- tests/data/expected
