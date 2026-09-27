#!/bin/bash
#

# Default values, falling back to environment variables if set
IM_TEXT_LOCATION="${IM_TEXT_LOCATION:-northwest}"
IM_BOUND_TEXT_COLOUR="${IM_BOUND_TEXT_COLOUR:-white}"
IM_BOUND_BOX_COLOUR="${IM_BOUND_BOX_COLOUR:-grey20}"
IM_BOUND_BOX_OPACITY="${IM_BOUND_BOX_OPACITY:-0.6}"
IM_BOUND_TEXT_Y_OFFSET="${IM_BOUND_TEXT_Y_OFFSET:-0.05}"
IM_BOUND_TEXT_X_OFFSET="${IM_BOUND_TEXT_X_OFFSET:-0.05}"
IM_PADDING_RATIO="${IM_PADDING_RATIO:-0.03}"
IM_ROUNDING_RADIUS="${IM_ROUNDING_RADIUS:-12}"
IM_FONT="${IM_FONT:-Oxanium-Regular}"
EXIF_FORMAT="${EXIF_FORMAT:-\$LensID @ f/\$Aperture}"

OUT_PREFIX=exif-annotated

usage() {
    echo "Usage: $(basename $0) [options] file1 [file2 ...]"
    echo ""
    echo "Annotates image with EXIF data and option text"
    echo ""
    echo "Options:"
    echo "  -l <loc>     Text location        (${IM_TEXT_LOCATION})"
    echo "  -c <color>   Text color           (${IM_BOUND_TEXT_COLOUR})"
    echo "  -b <color>   Box background color (${IM_BOUND_BOX_COLOUR})"
    echo "  -O <val>     Box opacity 0 to 1   (${IM_BOUND_BOX_OPACITY})"
    echo "  -y <val>     Y offset ratio       (${IM_BOUND_TEXT_Y_OFFSET})"
    echo "  -x <val>     X offset ratio       (${IM_BOUND_TEXT_X_OFFSET})"
    echo "  -p <val>     Padding ratio        (${IM_PADDING_RATIO})"
    echo "  -r <val>     Box corner rounding  (${IM_ROUNDING_RADIUS})"
    echo "  -f <font>    Font name            (${IM_FONT})"
    echo "  -e <format>  Exiftool format      (${EXIF_FORMAT})"
    echo "  -t <text>    additional annotation"
    echo "  -o <prefix>  output prefix        (${OUT_PREFIX})"
    echo "  -h           Display this help message and exit"
    exit 0
}

# Parse options using getopts
while getopts "l:c:b:O:y:x:p:r:f:o:t:e:h" opt; do
    case "${opt}" in
        l) IM_TEXT_LOCATION="$OPTARG" ;;
        c) IM_BOUND_TEXT_COLOUR="$OPTARG" ;;
        b) IM_BOUND_BOX_COLOUR="$OPTARG" ;;
        O) IM_BOUND_BOX_OPACITY="$OPTARG" ;;
        y) IM_BOUND_TEXT_Y_OFFSET="$OPTARG" ;;
        x) IM_BOUND_TEXT_X_OFFSET="$OPTARG" ;;
        p) IM_PADDING_RATIO="$OPTARG" ;;
        r) IM_ROUNDING_RADIUS="$OPTARG" ;;
        f) IM_FONT="$OPTARG" ;;
        o) OUT_PREFIX="$OPTARG" ;;
        t) EXTRA_TEXT="$OPTARG" ;;
        e) EXIF_FORMAT="$OPTARG" ;;
        h) usage ;;
        ?) usage ;;
    esac
done
shift $((OPTIND -1))

# Check if any files were provided
if [ $# -eq 0 ]; then
    echo "Error: No input files specified."
    echo ""
    usage
fi

failed_files=()

for i in "$@"; do
    if [ ! -z "${EXIF_FORMAT}" ]; then
	# 1. Gather all raw metadata text components using the specified format
	EXIF_DATA="$(exiftool -s3 -p "$EXIF_FORMAT" "$i" 2>/dev/null)"

	# If the EXIF data is empty, log the failure and skip to the next file
	if [ -z "$EXIF_DATA" ]; then
	    failed_files+=("$i")
	    continue
	fi
    fi

    DIM=$(magick "$i" ${IM_CROP} -format "%wx%h" info:)
    IM_TEXT="${EXIF_DATA}${EXTRA_TEXT}"

    IM_LEN=$(echo "$IM_TEXT" | awk '{ if (length($0) > max) max = length($0) } END { print max }')

    P_SIZE=$(magick "$i" ${IM_CROP} -format "%[fx:(w*0.5)/($IM_LEN*0.6)]" info:)
    X_OFF=$(magick "$i" ${IM_CROP} -format "%[fx:int(w*${IM_BOUND_TEXT_X_OFFSET})]" info:)
    Y_OFF=$(magick "$i" ${IM_CROP} -format "%[fx:int(h*${IM_BOUND_TEXT_Y_OFFSET})]" info:)

    # 3. Generate the final cropped image with perfectly left-aligned text lines
    magick "$i" \
      ${IM_CROP} \
      \( -background none \
         -fill ${IM_BOUND_TEXT_COLOUR} ${IM_STROKE} \
         -font ${IM_FONT} \
         -pointsize "$P_SIZE" \
         -gravity west \
         label:"$IM_TEXT" \
         -trim +repage \
         -bordercolor none \
         -border "%[fx:int(w*${IM_PADDING_RATIO})]x%[fx:int(w*(${IM_PADDING_RATIO}*0.8))]" \
         \( +clone \
            -alpha transparent \
            -fill "${IM_BOUND_BOX_COLOUR}" \
            -draw "roundrectangle 0,0 %[fx:w-1],%[fx:h-1] ${IM_ROUNDING_RADIUS},${IM_ROUNDING_RADIUS}" \
            -channel A -evaluate multiply ${IM_BOUND_BOX_OPACITY} +channel \
         \) \
         +swap -compose Over -composite \
      \) \
      -gravity ${IM_TEXT_LOCATION} \
      -geometry "+${X_OFF}+${Y_OFF}" \
      -composite \
      "${OUT_PREFIX}$i"
done

if [ ${#failed_files[@]} -ne 0 ]; then
    echo "The following files failed data:"
    for failed in "${failed_files[@]}"; do
        echo "  $failed"
    done
    exit 1
fi
