#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(dirname "$SCRIPT_DIR")"

OUT_FILE="$(mktemp)"
trap 'rm -f "$OUT_FILE"' EXIT

"$REPO_ROOT/scripts/generate_airwindows_category_map.sh" \
    --input "$REPO_ROOT/tests/fixtures/airwindows/moduleadd_sample.h" \
    --output "$OUT_FILE"

grep -q '{"ADClip7", "Clipping"},' "$OUT_FILE"
grep -q '{"Chamber", "Reverb"},' "$OUT_FILE"
grep -q '{"ConsoleXChannel", "Unclassified"},' "$OUT_FILE"

echo "Airwindows category map generation test passed."
