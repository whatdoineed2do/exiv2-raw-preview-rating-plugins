#!/bin/bash

# Default values
FONT_CHOICE="Oxanium-Regular"
FONT_COLOUR=white
BG_COLOUR=black
CANVAS_SIZE=1920x1080
MAIN_TEXT=""
SUB_TEXT=""
INPUT_IMAGE=""

# Usage help function
usage() {
    echo "Usage: $0 [-C canvas_size] [-f font_name] [-c font_colour] [-b background_colour] -m \"main_text\" [-s \"subtext\"] -i <input_image> <output_filename>"
    exit 1
}

# 1. Parse optional flags using getopts
while getopts ":f:c:b:C:m:s:i:" opt; do
    case ${opt} in
        f ) FONT_CHOICE="$OPTARG" ;;
        c ) FONT_COLOUR="$OPTARG" ;;
        b ) BG_COLOUR="$OPTARG" ;;
        C ) CANVAS_SIZE="$OPTARG" ;;
        m ) MAIN_TEXT="$OPTARG" ;;
        s ) SUB_TEXT="$OPTARG" ;;
        i ) INPUT_IMAGE="$OPTARG" ;;
        \? ) echo "Error: Invalid option -$OPTARG" >&2; usage ;;
        : ) echo "Error: Option -$OPTARG requires an argument." >&2; usage ;;
    esac
done
shift $((OPTIND -1))

# 2. Remaining positional argument must be the output filename
if [ "$#" -eq 1 ]; then
    OUTPUT_NAME="$1"
else
    echo "Error: Output filename is missing or incorrect positional arguments." >&2
    usage
fi

# Validate required parameters
if [ -z "$MAIN_TEXT" ] || [ -z "$INPUT_IMAGE" ]; then
    echo "Error: Both -m (main text) and -i (input image) are required." >&2
    usage
fi

if [ ! -f "$INPUT_IMAGE" ]; then
    echo "Error: Input image '$INPUT_IMAGE' not found." >&2
    exit 1
fi

# Extract canvas width and height
CANVAS_WIDTH=$(echo "$CANVAS_SIZE" | cut -d'x' -f1)
CANVAS_HEIGHT=$(echo "$CANVAS_SIZE" | cut -d'x' -f2)

# Calculate height of 1 segment out of 24
SEG_H=$((CANVAS_HEIGHT / 24))

# Define exact pixel heights for each section based on the 24-segment layout:
# - Title: Segments 2 to 4 (Height = 3 * SEG_H)
TITLE_H=$((SEG_H * 3))

# - Image Area: Segments 6 to 21 (Height = 16 * SEG_H)
IMG_AREA_H=$((SEG_H * 16))

# - Subtitle: Segments 22 to 23 (Height = 2 * SEG_H)
SUB_H=$((SEG_H * 2))

echo "Generating 24-segment layout..."
echo "Canvas=${CANVAS_SIZE} (Segment Height=${SEG_H}px) | Background=$BG_COLOUR | Font='$FONT_CHOICE'"

# 3. Process layout using ImageMagick with precise layer positioning (-repage)
if [ -n "$SUB_TEXT" ]; then
    # Dual-line mode (Title + Image + Subtitle)
    magick -size ${CANVAS_SIZE} xc:${BG_COLOUR} \
      \( -background none -font "$FONT_CHOICE" -fill "${FONT_COLOUR}" \
         -size "${CANVAS_WIDTH}x${TITLE_H}" -gravity center caption:"$MAIN_TEXT" \
         -repage "+0+$SEG_H" \) \
      \( "$INPUT_IMAGE" -resize "${CANVAS_WIDTH}x${IMG_AREA_H}>" \
         -background "${BG_COLOUR}" -gravity center -extent "${CANVAS_WIDTH}x${IMG_AREA_H}" \
         -repage "+0+$((SEG_H * 5))" \) \
      \( -background none -font "$FONT_CHOICE" -fill "${FONT_COLOUR}" \
         -size "${CANVAS_WIDTH}x${SUB_H}" -gravity center caption:"$SUB_TEXT" \
         -repage "+0+$((SEG_H * 21))" \) \
      -background "${BG_COLOUR}" -layers merge +repage -colorspace sRGB -type truecolor "$OUTPUT_NAME"
else
    # Single-line mode (Title + Image only)
    magick -size ${CANVAS_SIZE} xc:${BG_COLOUR} \
      \( -background none -font "$FONT_CHOICE" -fill "${FONT_COLOUR}" \
         -size "${CANVAS_WIDTH}x${TITLE_H}" -gravity center caption:"$MAIN_TEXT" \
         -repage "+0+$SEG_H" \) \
      \( "$INPUT_IMAGE" -resize "${CANVAS_WIDTH}x${IMG_AREA_H}>" \
         -background "${BG_COLOUR}" -gravity center -extent "${CANVAS_WIDTH}x${IMG_AREA_H}" \
         -repage "+0+$((SEG_H * 5))" \) \
      -background "${BG_COLOUR}" -layers merge +repage -colorspace sRGB -type truecolor "$OUTPUT_NAME"
fi

MXC=$?

# 4. Check exit status
if [ $MXC -eq 0 ]; then
    echo "Successfully generated: $OUTPUT_NAME"
else
    echo "Error: ImageMagick failed to generate the title card asset." >&2
    exit $MXC
fi
