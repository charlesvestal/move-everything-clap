#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(dirname "$SCRIPT_DIR")"

SOURCE_URL="${AIRWINDOWS_MODULEADD_URL:-https://raw.githubusercontent.com/baconpaul/airwin2rack/main/src/ModuleAdd.h}"
INPUT_FILE=""
OUTPUT_FILE="$REPO_ROOT/src/dsp/airwindows_category_map.inc"

usage() {
    cat <<'EOF'
Usage: generate_airwindows_category_map.sh [--input <moduleadd.h>] [--output <path>]

Generates src/dsp/airwindows_category_map.inc from Airwindows registry metadata.

Options:
  --input   Use a local ModuleAdd.h file instead of downloading.
  --output  Output path (default: src/dsp/airwindows_category_map.inc).
EOF
}

while [ $# -gt 0 ]; do
    case "$1" in
        --input)
            INPUT_FILE="${2:-}"
            shift 2
            ;;
        --output)
            OUTPUT_FILE="${2:-}"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown argument: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

tmp_input=""
cleanup() {
    if [ -n "$tmp_input" ] && [ -f "$tmp_input" ]; then
        rm -f "$tmp_input"
    fi
}
trap cleanup EXIT

if [ -z "$INPUT_FILE" ]; then
    tmp_input="$(mktemp)"
    if command -v curl >/dev/null 2>&1; then
        if ! curl -fsSL "$SOURCE_URL" -o "$tmp_input"; then
            if [ -f "$OUTPUT_FILE" ]; then
                echo "Warning: failed to download Airwindows metadata. Keeping existing map at $OUTPUT_FILE" >&2
                exit 0
            fi
            echo "Error: failed to download Airwindows metadata from $SOURCE_URL" >&2
            exit 1
        fi
    elif command -v wget >/dev/null 2>&1; then
        if ! wget -qO "$tmp_input" "$SOURCE_URL"; then
            if [ -f "$OUTPUT_FILE" ]; then
                echo "Warning: failed to download Airwindows metadata. Keeping existing map at $OUTPUT_FILE" >&2
                exit 0
            fi
            echo "Error: failed to download Airwindows metadata from $SOURCE_URL" >&2
            exit 1
        fi
    else
        if [ -f "$OUTPUT_FILE" ]; then
            echo "Warning: curl/wget unavailable. Keeping existing map at $OUTPUT_FILE" >&2
            exit 0
        fi
        echo "Error: curl/wget unavailable and no existing map present." >&2
        exit 1
    fi
    INPUT_FILE="$tmp_input"
fi

if [ ! -f "$INPUT_FILE" ]; then
    echo "Error: input file not found: $INPUT_FILE" >&2
    exit 1
fi

tmp_output="$(mktemp)"
perl -ne '
    if (/registerAirwindow\(\{\"([^\"]+)\",\s*\"([^\"]+)\"/) {
        print "$1\t$2\n";
    }
' "$INPUT_FILE" \
    | sort -u \
    | awk -F '\t' '{ printf("    {\"%s\", \"%s\"},\n", $1, $2); }' > "$tmp_output"

if [ ! -s "$tmp_output" ]; then
    rm -f "$tmp_output"
    if [ -f "$OUTPUT_FILE" ]; then
        echo "Warning: parsed zero categories. Keeping existing map at $OUTPUT_FILE" >&2
        exit 0
    fi
    echo "Error: generated map is empty." >&2
    exit 1
fi

mkdir -p "$(dirname "$OUTPUT_FILE")"
mv "$tmp_output" "$OUTPUT_FILE"
echo "Generated Airwindows category map: $OUTPUT_FILE"
