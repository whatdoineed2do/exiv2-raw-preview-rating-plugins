#!/bin/bash
set -euo pipefail

SHOW_USAGE() {
    cat <<EOF
Usage: $(basename "$0") -i <input_image> -o <output_image> [-t | -b]

Options:
  -i  Input image path
  -o  Output image path
  -t  Preserve TOP of image when cropping
  -b  Preserve BOTTOM of image when cropping
      (Default: Centered vertical crop)
EOF
    exit 1
}

INPUT=""
OUTPUT=""
GRAVITY="center"

while getopts "i:o:tbh" opt; do
    case "$opt" in
        i) INPUT="$OPTARG" ;;
        o) OUTPUT="$OPTARG" ;;
        t) GRAVITY="north" ;;
        b) GRAVITY="south" ;;
        h|*) SHOW_USAGE ;;
    esac
done

if [[ -z "$INPUT" || -z "$OUTPUT" ]]; then
    echo "Error: Both -i and -o parameters are required." >&2
    SHOW_USAGE
fi

if [[ ! -f "$INPUT" ]]; then
    echo "Error: Input file '$INPUT' does not exist." >&2
    exit 1
fi

MAGICK_CMD=""
if command -v magick &>/dev/null; then
    MAGICK_CMD="magick"
elif command -v convert &>/dev/null; then
    MAGICK_CMD="convert"
else
    echo "Error: ImageMagick (magick or convert) is required." >&2
    exit 1
fi

# -----------------------------------------------------------------------------
# 1. Inspect Dimensions (Auto-oriented for true visual dimensions)
# -----------------------------------------------------------------------------
# Removed -ping to allow -auto-orient to correctly parse oriented dimensions
READ_DIM=$($MAGICK_CMD "$INPUT" -auto-orient -format "%w %h" info: 2>/dev/null || true)

if [[ -z "$READ_DIM" ]]; then
    echo "Error: Failed to read image dimensions for '$INPUT'." >&2
    exit 1
fi

WIDTH=$(echo "$READ_DIM" | cut -d' ' -f1)
HEIGHT=$(echo "$READ_DIM" | cut -d' ' -f2)

# Verify landscape orientation
if (( HEIGHT >= WIDTH )); then
    echo "Notice: Image '$INPUT' evaluates to portrait/square (${WIDTH}x${HEIGHT}). No changes made."
    exit 0
fi

# -----------------------------------------------------------------------------
# 2. Calculate 16:9 Crop Bounds
# -----------------------------------------------------------------------------
TARGET_H=$(( WIDTH * 9 / 16 ))

if (( TARGET_H > HEIGHT )); then
    echo "Notice: Image aspect ratio is wider than 16:9 (${WIDTH}x${HEIGHT}). Vertical crop not applicable."
    exit 0
fi

echo "Processing: $INPUT (${WIDTH}x${HEIGHT})"
echo "  -> Target 16:9 Crop Dimensions: ${WIDTH}x${TARGET_H}"
echo "  -> Gravity Alignment: $GRAVITY"

# -----------------------------------------------------------------------------
# 3. Apply Auto-Orient & Crop
# -----------------------------------------------------------------------------
$MAGICK_CMD "$INPUT" \
    -auto-orient \
    -gravity "$GRAVITY" \
    -crop "${WIDTH}x${TARGET_H}+0+0" \
    +repage \
    "$OUTPUT"

echo "  -> Saved cropped image to: $OUTPUT"
