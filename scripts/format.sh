#!/usr/bin/env bash
# Apply clang-format to every project C source and header (configuration: .clang-format).
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.."

CLANG_FORMAT="${CLANG_FORMAT:-clang-format-20}"

git ls-files --cached --others --exclude-standard -- '*.c' '*.h' | while read -r f; do
    if [[ -f "$f" ]]; then
        printf '%s\n' "$f"
    fi
done | xargs -r "$CLANG_FORMAT" -i
