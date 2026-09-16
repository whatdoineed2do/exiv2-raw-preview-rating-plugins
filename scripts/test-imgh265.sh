#!/bin/bash
set -euo pipefail

SCRIPT_BIN="./imgh265.sh"
WORK_DIR="test_run_suite"
ASSETS_DIR="${WORK_DIR}/assets"
OUTPUT_DIR="${WORK_DIR}/outputs"

# -----------------------------------------------------------------------------
# Signal Handling (Preserve assets on user interrupt / kill)
# -----------------------------------------------------------------------------
cleanup_on_signal() {
    echo -e "\n\n[!] Execution interrupted by signal. Terminating script..." >&2
    echo "  -> Note: Assets and temporary test outputs have NOT been deleted." >&2
    echo "  -> Preserved directory: ./${WORK_DIR}" >&2
    exit 130
}

trap cleanup_on_signal SIGINT SIGTERM

# -----------------------------------------------------------------------------
# 0. Environment & Dependency Checks
# -----------------------------------------------------------------------------
if [ ! -x "$SCRIPT_BIN" ]; then
    echo "Error: $SCRIPT_BIN not found or not executable." >&2
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

mkdir -p "$ASSETS_DIR" "$OUTPUT_DIR"

# -----------------------------------------------------------------------------
# 1. Synthetic Test Image Generator (Default System Font)
# -----------------------------------------------------------------------------
generate_img() {
    local file="$1"
    local size="$2"
    local label="$3"
    local color="$4"
    local rot_tag="${5:-1}"

    echo "  -> Generating $file ($size)..."

    local w="${size%x*}"
    local h="${size#*x}"

    $MAGICK_CMD -size "$size" xc:"$color" \
        -stroke white -strokewidth 4 -fill none -draw "rectangle 10,10 $((w-10)),$((h-10))" \
        -stroke none -fill white -pointsize 28 \
        -gravity NorthWest -draw "text 20,20 'TOP-LEFT'" \
        -gravity NorthEast -draw "text 20,20 'TOP-RIGHT'" \
        -gravity SouthWest -draw "text 20,20 'BOTTOM-LEFT'" \
        -gravity SouthEast -draw "text 20,20 'BOTTOM-RIGHT'" \
        -gravity center -pointsize 42 -annotate 0 "$label\n$size" \
        "${ASSETS_DIR}/${file}"

    if [ "$rot_tag" -ne 1 ] && command -v exiftool &>/dev/null; then
        exiftool -overwrite_original -Orientation#="$rot_tag" "${ASSETS_DIR}/${file}" &>/dev/null
    fi
}

echo "================================================================="
echo " STEP 1: GENERATING CORNER-MARKED TEST IMAGES"
echo "================================================================="

generate_img "01_highres_16_9.jpg" "1920x1080" "16:9 High-Res" "navy"
generate_img "02_highres_3_2_landscape.jpg" "3000x2000" "3:2 Landscape\n(Top/Bottom Cropped)" "darkgreen"
generate_img "03_highres_4_3_landscape.jpg" "2048x1536" "4:3 Landscape\n(Top/Bottom Cropped)" "darkslategrey"
generate_img "04_lowres_4_3_landscape.jpg" "640x480" "Low-Res 4:3\n(Blurred BG)" "maroon"
generate_img "05_lowres_16_9_landscape.jpg" "960x540" "Low-Res 16:9\n(Direct Scale)" "purple"
generate_img "06_portrait_3_4.jpg" "1536x2048" "3:4 Portrait\n(Blurred BG)" "darkred"
generate_img "07_exif_rotated_portrait.jpg" "3264x2448" "EXIF Rotated\n(Orientation 6 -> Portrait)" "teal" 6

echo ""

# -----------------------------------------------------------------------------
# 2. Test Execution Across Flag Configurations
# -----------------------------------------------------------------------------
HEADER_IMG="${ASSETS_DIR}/01_highres_16_9.jpg"
TAIL_IMG="${ASSETS_DIR}/07_exif_rotated_portrait.jpg"

run_test() {
    local test_name="$1"
    local out_file="$OUTPUT_DIR/${test_name}.mp4"
    shift
    
    echo "-----------------------------------------------------------------"
    echo " RUNNING TEST: $test_name"
    echo " FLAGS: -y $* $ASSETS_DIR"
    echo "-----------------------------------------------------------------"

    "$SCRIPT_BIN" -y "$@" -o "$out_file" "$ASSETS_DIR"
    echo "  -> Rendered: $out_file"
    echo ""
}

echo "================================================================="
echo " STEP 2: EXECUTING IMGH265 TEST MATRIX"
echo "================================================================="

# 1. Standard Mode (Default pad/blur for all images, auto total length)
run_test "01_standard_mode"

# 2. Smart Crop Mode (-c)
run_test "02_crop_mode" -c

# 3. Fixed Target Total Length (-L 00:00:21)
run_test "03_target_length" -L 00:00:21

# 4. Header & Tail Images (-h and -t)
run_test "04_header_tail" -h "$HEADER_IMG" -t "$TAIL_IMG"

# 5. Full Combination: Smart Crop + Length + Header/Tail (-c -L 00:00:25 -h -t)
run_test "05_crop_length_header_tail" -c -L 00:00:25 -h "$HEADER_IMG" -t "$TAIL_IMG"

# 6. Smart Crop + Dynamic Timing (-c -d)
run_test "06_crop_dynamic_timing" -c -d

echo "================================================================="
echo " ALL TESTS COMPLETE!"
echo " Everything saved under: ./${WORK_DIR}/"
echo "   - Assets  : ./${ASSETS_DIR}/"
echo "   - Outputs : ./${OUTPUT_DIR}/"
echo ""
echo " Clean up command when finished:"
echo "   rm -rf ./${WORK_DIR}"
echo "================================================================="
