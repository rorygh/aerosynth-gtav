#!/usr/bin/env bash
set -euo pipefail

# Downloads the sample GTA V captures zip from Google Drive and extracts to output/.

FILE_ID="1njlaXBjVSoRkwQ3WJ1mj5ViGHl6tWdyH"
DEST_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/output"
TMP_ZIP=/tmp/gtav_samples.zip

mkdir -p "$DEST_DIR"

echo "Downloading sample captures..."
wget -q --show-progress \
    "https://drive.google.com/uc?export=download&id=${FILE_ID}&confirm=t" \
    -O "$TMP_ZIP"

echo "Extracting to $DEST_DIR..."
unzip -o "$TMP_ZIP" -d "$DEST_DIR"
rm "$TMP_ZIP"

echo "Done. Contents:"
find "$DEST_DIR" -type f | sort
