#!/usr/bin/env bash

set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"

if ! command -v clang-format >/dev/null 2>&1; then
    if [ -z "${LINT_IN_NIX:-}" ] && command -v nix >/dev/null 2>&1; then
        LINT_IN_NIX=1 exec nix develop .#ci --command "$0" "$@"
    fi
    echo "clang-format not found (and nix unavailable)" >&2
    exit 1
fi

mode=(-i)
if [ "${1:-}" = "--check" ]; then
    mode=(--dry-run --Werror)
fi

find src include tests \
    \( -name '*.cpp' -o -name '*.cc' -o -name '*.h' -o -name '*.hh' -o -name '*.hpp' \) \
    -print0 | xargs -0 -r clang-format "${mode[@]}"

if [ "${mode[0]}" = "-i" ]; then
    echo "formatted"
else
    echo "lint OK"
fi