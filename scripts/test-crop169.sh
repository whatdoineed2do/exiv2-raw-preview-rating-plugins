#!/bin/bash
set -euo pipefail

CROP_SCRIPT="./crop169.sh"
WORK_DIR="test_crop169_suite"
ASSETS_DIR="${WORK_DIR}/assets"
OUTPUT_DIR="${WORK_DIR}/outputs"

cleanup_on_signal() {
    echo -e "\n\n[!] Execution interrupted by signal. Terminating script..." >&2
    echo "  -> Preserved directory: ./${WORK_DIR}" >&2
    exit 130
}

trap cleanup_on_signal SIGINT SIGTERM

if [ ! -x "$CROP_SCRIPT" ]; then
    echo "Error: $CROP_SCRIPT not found or not executable." >&2
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

# -----------------------------------------------------------------------------
# 1. Asset Generator
# -----------------------------------------------------------------------------
generate_asset() {
    local file="$1"
    local size="$2"
    local label="$3"
    local color="$4"
    local rot_tag="${5:-1}"

    echo "  -> Generating $file ($size, $label)..."

    local w="${size%x*}"
    local h="${size#*x}"

    $MAGICK_CMD -size "$size" xc:"$color" \
        -stroke white -strokewidth 4 -fill none -draw "rectangle 10,10 $((w-10)),$((h-10))" \
        -stroke none -fill white -pointsize 24 \
        -gravity NorthWest -draw "text 20,20 'TOP-LEFT'" \
        -gravity NorthEast -draw "text 20,20 'TOP-RIGHT'" \
        -gravity SouthWest -draw "text 20,20 'BOTTOM-LEFT'" \
        -gravity SouthEast -draw "text 20,20 'BOTTOM-RIGHT'" \
        -gravity North -draw "text 0,20 '--- TOP EDGE SAFE ZONE ---'" \
        -gravity South -draw "text 0,20 '--- BOTTOM EDGE SAFE ZONE ---'" \
        -gravity center -pointsize 32 -annotate 0 "$label\n$size" \
        "${ASSETS_DIR}/${file}"

    if [ "$rot_tag" -ne 1 ]; then
        if command -v exiftool &>/dev/null; then
            exiftool -overwrite_original -Orientation#="$rot_tag" "${ASSETS_DIR}/${file}" &>/dev/null
        else
            echo "     [Warning]: exiftool not found; EXIF orientation tag $rot_tag not applied to $file" >&2
        fi
    fi
}

echo "================================================================="
echo " STEP 1: GENERATING TEST ASSETS (LANDSCAPE + PORTRAIT/EXIF)"
echo "================================================================="

generate_asset "01_3_2_landscape.jpg" "3000x2000" "3:2 Standard Landscape" "darkgreen"
generate_asset "02_4_3_landscape.jpg" "2048x1536" "4:3 Classic Landscape" "darkslategrey"
generate_asset "03_5_4_landscape.jpg" "2500x2000" "5:4 Large Format" "navy"
generate_asset "04_16_9_landscape.jpg" "1920x1080" "16:9 Native Aspect" "maroon"
generate_asset "05_3_4_portrait.jpg" "1536x2048" "3:4 Standard Portrait (Skip)" "darkred"
generate_asset "06_exif_rotated_portrait.jpg" "3264x2448" "EXIF Rotated (Orientation 6 -> Portrait Skip)" "teal" 6

echo ""

# -----------------------------------------------------------------------------
# 2. Permutation Test Runner
# -----------------------------------------------------------------------------
run_crop_permutation() {
    local input_file="$1"
    local mode="$2"
    local flag_arg="$3"

    local base_name
    base_name="$(basename "$input_file" .jpg)"
    base_name="$(basename "$base_name" .JPG)"
    local out_file="${OUTPUT_DIR}/${base_name}_crop_${mode}.jpg"

    echo "-----------------------------------------------------------------"
    echo " TESTING: $base_name | MODE: $mode ($flag_arg)"
    echo "-----------------------------------------------------------------"

    if [ -n "$flag_arg" ]; then
        "$CROP_SCRIPT" -i "$input_file" -o "$out_file" "$flag_arg" || true
    else
        "$CROP_SCRIPT" -i "$input_file" -o "$out_file" || true
    fi

    if [[ -f "$out_file" ]]; then
        echo "  [OUTPUT GENERATED]: $out_file"
    else
        echo "  [NO OUTPUT GENERATED]: Correctly skipped non-landscape file."
    fi
    echo ""
}

echo "================================================================="
echo " STEP 2: RUNNING CROP PERMUTATIONS (-t / -b / DEFAULT)"
echo "================================================================="

# Enable nullglob so loop gracefully handles empty matches
shopt -s nullglob
assets_list=("${ASSETS_DIR}"/*.jpg "${ASSETS_DIR}"/*.JPG)

if [ ${#assets_list[@]} -eq 0 ]; then
    echo "Error: No asset images found in ./${ASSETS_DIR}" >&2
    exit 1
fi

for img in "${assets_list[@]}"; do
    run_crop_permutation "$img" "center" ""
    run_crop_permutation "$img" "top" "-t"
    run_crop_permutation "$img" "bottom" "-b"
done

echo "================================================================="
echo " ALL TEST PERMUTATIONS COMPLETE!"
echo " Inspect ./${OUTPUT_DIR}/ to verify portrait files were skipped."
echo "================================================================="
