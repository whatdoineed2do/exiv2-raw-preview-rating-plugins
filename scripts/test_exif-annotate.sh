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
echo " GENERATING TEST ASSETS (Multi-Aspect/Size & Constant Reference)"
echo "================================================================="

# --- Part A: Multi-Aspect & Multi-Size Assets for Dynamic Sizing ---
$MAGICK_CMD -size 800x800 xc:darkblue \
    -pointsize 24 -fill white -gravity center -annotate 0 "Small 1:1 (800x800)" \
    "${ASSETS_DIR}/asset_small_11.jpg"

$MAGICK_CMD -size 900x600 xc:darkslateblue \
    -pointsize 24 -fill white -gravity center -annotate 0 "Small 3:2 (900x600)" \
    "${ASSETS_DIR}/asset_small_32.jpg"

$MAGICK_CMD -size 2048x1536 xc:darkgreen \
    -pointsize 42 -fill white -gravity center -annotate 0 "Medium 4:3 (2048x1536)" \
    "${ASSETS_DIR}/asset_med_43.jpg"

$MAGICK_CMD -size 3000x2000 xc:maroon \
    -pointsize 54 -fill white -gravity center -annotate 0 "Large 3:2 (3000x2000)" \
    "${ASSETS_DIR}/asset_large_32.jpg"

$MAGICK_CMD -size 2400x2400 xc:midnightblue \
    -pointsize 54 -fill white -gravity center -annotate 0 "Large 1:1 (2400x2400)" \
    "${ASSETS_DIR}/asset_large_11.jpg"

# --- Part B: Constant Reference Asset for Styling Matrix ---
$MAGICK_CMD -size 1500x1000 xc:darkslategray \
    -pointsize 42 -fill white -gravity center -annotate 0 "Constant Base 1500x1000" \
    "${ASSETS_DIR}/asset_1500x1000.jpg"

# Populate mock EXIF metadata across all test assets
if command -v exiftool &>/dev/null; then
    exiftool -overwrite_original -Model="Nikon D800" -LensModel="Nikkor 18mm f/4 AI" -Aperture="4" -ISO="400" "${ASSETS_DIR}"/*.jpg &>/dev/null
fi

echo "  -> All test assets created and EXIF populated in ./${ASSETS_DIR}/"
echo ""

echo "================================================================="
echo " TESTS 1-5: DYNAMIC SIZING & SCALING ACROSS VARIED ASSETS"
echo "================================================================="

echo "[Test 1] Small Square 1:1 (800x800)..."
"$ANNOTATE_SCRIPT" -o "${OUTPUT_DIR}/test1_small_11_"  -l northwest -S 0.35 "${ASSETS_DIR}/asset_small_11.jpg"

echo "[Test 2] Small Landscape 3:2 (900x600)..."
"$ANNOTATE_SCRIPT" -o "${OUTPUT_DIR}/test2_small_32_"  -l northwest -S 0.35 "${ASSETS_DIR}/asset_small_32.jpg"

echo "[Test 3] Medium 4:3 (2048x1536)..."
"$ANNOTATE_SCRIPT" -o "${OUTPUT_DIR}/test3_med_43_"     -l northwest -S 0.35 "${ASSETS_DIR}/asset_med_43.jpg"

echo "[Test 4] Large Landscape 3:2 (3000x2000)..."
"$ANNOTATE_SCRIPT" -o "${OUTPUT_DIR}/test4_large_32_"  -l northwest -S 0.35 "${ASSETS_DIR}/asset_large_32.jpg"

echo "[Test 5] Large Square 1:1 (2400x2400)..."
"$ANNOTATE_SCRIPT" -o "${OUTPUT_DIR}/test5_large_11_"  -l northwest -S 0.35 "${ASSETS_DIR}/asset_large_11.jpg"

echo ""
echo "================================================================="
echo " TESTS 6-10: STYLING, POSITIONING & SCALING MATRIX (Constant 1500x1000)"
echo "================================================================="

echo "[Test 6] Northwest Location, Default Scaler (0.35), Default Box Style..."
"$ANNOTATE_SCRIPT" -o "${OUTPUT_DIR}/test6_nw_default_" \
    -l northwest -S 0.35 \
    "${ASSETS_DIR}/asset_1500x1000.jpg"

echo "[Test 7] Southeast Location, Smaller Scaler (0.25), High Opacity (0.9)..."
"$ANNOTATE_SCRIPT" -o "${OUTPUT_DIR}/test7_se_scaler025_op09_" \
    -l southeast -S 0.25 -O 0.9 \
    "${ASSETS_DIR}/asset_1500x1000.jpg"

echo "[Test 8] Northeast Location, Larger Scaler (0.5), Custom Offsets (x=0.08, y=0.08)..."
"$ANNOTATE_SCRIPT" -o "${OUTPUT_DIR}/test8_ne_scaler05_offsets_" \
    -l northeast -S 0.5 -x 0.08 -y 0.08 \
    "${ASSETS_DIR}/asset_1500x1000.jpg"

echo "[Test 9] Southwest Location, Custom Padding (0.06), Low Opacity (0.3), High Corner Rounding (r=24)..."
"$ANNOTATE_SCRIPT" -o "${OUTPUT_DIR}/test9_sw_padding_rounding_" \
    -l southwest -S 0.35 -O 0.3 -p 0.06 -r 24 \
    "${ASSETS_DIR}/asset_1500x1000.jpg"

echo "[Test 10] North Location, Custom Text Color (yellow), Custom Box Color (navy), Sharp Corners (r=4)..."
"$ANNOTATE_SCRIPT" -o "${OUTPUT_DIR}/test10_custom_colors_box_" \
    -l north -S 0.35 -c yellow -b navy -r 4 \
    "${ASSETS_DIR}/asset_1500x1000.jpg"

echo ""
echo "================================================================="
echo " TESTS 11-15: FEATURE & EDGE-CASE TEST SUITE"
echo "================================================================="

echo "[Test 11] Standalone Transparent Overlay Mode (-T)..."
"$ANNOTATE_SCRIPT" -T -o "${OUTPUT_DIR}/test11_overlay_" "${ASSETS_DIR}/asset_large_32.jpg"

if [ -f "${OUTPUT_DIR}/test11_overlay_asset_large_32.png" ]; then
    echo "  -> Verifying composition workflow for Test 11 (-T overlay)..."
    $MAGICK_CMD "${ASSETS_DIR}/asset_large_32.jpg" \
        "${OUTPUT_DIR}/test11_overlay_asset_large_32.png" \
        -composite \
        "${OUTPUT_DIR}/test11_composited_result.jpg"
    echo "  -> Composited output saved to ./${OUTPUT_DIR}/test11_composited_result.jpg"
fi

echo "[Test 12] Custom ExifTool Format string (-e)..."
"$ANNOTATE_SCRIPT" -o "${OUTPUT_DIR}/test12_custom_format_" \
    -e 'ISO $ISO @ $Model' \
    "${ASSETS_DIR}/asset_med_43.jpg"

echo "[Test 13] Image Cropping (-C)..."
"$ANNOTATE_SCRIPT" -o "${OUTPUT_DIR}/test13_cropped_" \
    -C "1500x1000+500+500" \
    "${ASSETS_DIR}/asset_large_32.jpg"

echo "[Test 14] Missing EXIF error handling..."
$MAGICK_CMD -size 1000x1000 xc:black "${ASSETS_DIR}/img_no_exif.jpg"

echo "  -> Testing strict failure when EXIF is missing (expected error message below):"
"$ANNOTATE_SCRIPT" -o "${OUTPUT_DIR}/test14_fail_" "${ASSETS_DIR}/img_no_exif.jpg" || echo "  -> Successfully caught missing EXIF!"

echo "[Test 15] Missing EXIF with -F bypass and custom text (-t)..."
"$ANNOTATE_SCRIPT" -o "${OUTPUT_DIR}/test15_pass_" \
    -F -t "Manual Annotation Only (No EXIF)" \
    "${ASSETS_DIR}/img_no_exif.jpg"

echo ""
echo "================================================================="
echo " ALL TESTS COMPLETED SUCCESSFULLY!"
echo " Inspect output variations inside ./${OUTPUT_DIR}/"
echo "================================================================="
