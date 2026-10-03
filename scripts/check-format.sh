#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."
mapfile -t sources < <(find include src tests -type f \( -name '*.hpp' -o -name '*.cpp' \) -print | sort)
clang-format --dry-run --Werror "${sources[@]}"
