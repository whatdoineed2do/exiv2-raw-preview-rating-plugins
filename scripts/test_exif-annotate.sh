#!/bin/bash
set -euo pipefail

ANNOTATE_SCRIPT="./exif-annotate.sh"
WORK_DIR="test_annotate_suite"
ASSETS_DIR="${WORK_DIR}/assets"
OUTPUT_DIR="${WORK_DIR}/outputs"

cleanup_on_signal() {
    echo -e "\n\n[!] Interrupted. Preserving test directory: ./${WORK_DIR}" >&2
    exit 130
}
trap cleanup_on_signal SIGINT SIGTERM

if [ ! -x "$ANNOTATE_SCRIPT" ]; then
    echo "Error: $ANNOTATE_SCRIPT not found or not executable." >&2
    exit 1
fi

MAGICK_CMD=""
if command -v magick &>/dev/null; then
    MAGICK_CMD="magick"
elif command -v convert &>/dev/null; then
    MAGICK_CMD="convert"
else
    echo "Error: ImageMagick required." >&2
    exit 1
fi

mkdir -p "$ASSETS_DIR" "$OUTPUT_DIR"

echo "================================================================="
echo " GENERATING TEST IMAGES OF VARYING RESOLUTIONS"
echo "================================================================="

# 1. Small resolution (400x300)
$MAGICK_CMD -size 400x300 xc:darkblue \
    -pointsize 20 -fill white -gravity center -annotate 0 "Small 400x300" \
    "${ASSETS_DIR}/img_small_400x300.jpg"

# 2. Medium resolution (2048x1536)
$MAGICK_CMD -size 2048x1536 xc:darkgreen \
    -pointsize 48 -fill white -gravity center -annotate 0 "Medium 2048x1536" \
    "${ASSETS_DIR}/img_medium_2048x1536.jpg"

# 3. High resolution (4000x3000)
$MAGICK_CMD -size 4000x3000 xc:maroon \
    -pointsize 72 -fill white -gravity center -annotate 0 "High-Res 4000x3000" \
    "${ASSETS_DIR}/img_high_4000x3000.jpg"

echo "  -> Assets created in ./${ASSETS_DIR}/"
echo ""

echo "================================================================="
echo " RUNNING ANNOTATION SCRIPT WITH UNIFORM TARGET SIZE (-R 1200x)"
echo "================================================================="

# Run annotation script targeting 1200px width with custom fallback text (-F -t)
"$ANNOTATE_SCRIPT" -o "${OUTPUT_DIR}/annotated_" -R "1200x" -F -t "Test Label @ f/2.8" "${ASSETS_DIR}"/*.jpg

echo ""
echo "================================================================="
echo " TEST COMPLETE!"
echo " Check outputs in ./${OUTPUT_DIR}/"
echo " Verify that the text and rounded boxes are identical in size"
echo " across all generated output images regardless of input resolution."
echo "================================================================="
