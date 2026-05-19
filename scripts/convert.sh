#!/usr/bin/env bash
set -euo pipefail

# Convert a raw capture session directory to normalised per-frame layout.
#
# Usage:
#   bash scripts/convert.sh <session_dir> [--out-dir <dir>]
#
# Defaults: output goes to output/converted/ relative to the repo root.

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SESSION="${1:?Usage: $0 <session_dir> [--out-dir <dir>]}"
shift
OUT_DIR="$REPO/output/converted"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --out-dir) OUT_DIR="$2"; shift 2 ;;
        *) echo "Unknown arg: $1" >&2; exit 1 ;;
    esac
done

source /opt/miniconda/etc/profile.d/conda.sh
conda activate gtav

python "$REPO/convert.py" "$SESSION" --out-dir "$OUT_DIR"
