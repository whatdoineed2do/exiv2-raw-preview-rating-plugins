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
echo " RUNNING TEST MATRIX (Scalers, Locations, Opacities, Offsets, Padding)"
echo "================================================================="

# Test 1: Default Settings (Scaler 0.35, Northwest, Default Opacity/Offsets)
echo "[Test 1] Default Scaler (0.35) & Northwest Location..."
"$ANNOTATE_SCRIPT" -o "${OUTPUT_DIR}/test1_default_" -R "1200x" -F -t "Nikkor 18mm f/4 AI @ f/4.0" "${ASSETS_DIR}"/*.jpg

# Test 2: Smaller Scaler (0.25), Southeast Location, High Opacity (0.9)
echo "[Test 2] Smaller Scaler (0.25), Southeast, High Opacity (0.9)..."
"$ANNOTATE_SCRIPT" -o "${OUTPUT_DIR}/test2_se_025_op09_" -R "1200x" -S 0.25 -l southeast -O 0.9 -F -t "Nikkor 18mm f/4 AI @ f/4.0" "${ASSETS_DIR}"/*.jpg

# Test 3: Larger Scaler (0.5), Northeast Location, Custom Offsets (x=0.08, y=0.08)
echo "[Test 3] Larger Scaler (0.5), Northeast, Custom Offsets..."
"$ANNOTATE_SCRIPT" -o "${OUTPUT_DIR}/test3_ne_05_offsets_" -R "1200x" -S 0.5 -l northeast -x 0.08 -y 0.08 -F -t "Nikkor 18mm f/4 AI @ f/4.0" "${ASSETS_DIR}"/*.jpg

# Test 4: Custom Padding (0.06), Southwest Location, Low Opacity (0.3)
echo "[Test 4] Southwest, Padding (0.06), Low Opacity (0.3)..."
"$ANNOTATE_SCRIPT" -o "${OUTPUT_DIR}/test4_sw_pad06_op03_" -R "1200x" -S 0.35 -l southwest -O 0.3 -p 0.06 -F -t "Nikkor 18mm f/4 AI @ f/4.0" "${ASSETS_DIR}"/*.jpg

echo ""
echo "================================================================="
echo " ALL TESTS COMPLETED SUCCESSFULLY!"
echo " Inspect output variations inside ./${OUTPUT_DIR}/"
echo "================================================================="
