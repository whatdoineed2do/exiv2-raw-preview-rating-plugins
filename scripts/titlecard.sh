#!/bin/bash

# Default values
FONT_CHOICE="Oxanium-Regular"
MAIN_SIZE=132
SUB_SIZE=40
FONT_COLOUR=white
BG_COLOUR=black
CANVAS_SIZE=1920x1080

# Usage help function
usage() {
    echo "Usage: $0 [-C canvas size] [-f font_name] [-c font colour] [-b backgroud colour] [-m main_size] [-s sub_size] <main_text> [subtext] <output_filename>"
    exit 1
}

# 1. Parse optional flags using getopts
while getopts ":f:m:s:c:b:C:" opt; do
    case ${opt} in
        f ) FONT_CHOICE="$OPTARG" ;;
        m ) MAIN_SIZE="$OPTARG" ;;
        s ) SUB_SIZE="$OPTARG" ;;
        c ) FONT_COLOUR="$OPTARG" ;;
        b ) BG_COLOUR="$OPTARG" ;;
        C ) CANVAS_SIZE="$OPTARG" ;;
        \? ) echo "Error: Invalid option -$OPTARG" >&2; usage ;;
        : ) echo "Error: Option -$OPTARG requires an argument." >&2; usage ;;
    esac
done
shift $((OPTIND -1))

# 2. Handle optional subtext by counting remaining parameters
if [ "$#" -eq 2 ]; then
    # 2 arguments means: main_text output_filename (No subtext provided)
    MAIN_TEXT="$1"
    SUB_TEXT=""
    OUTPUT_NAME="$2"
elif [ "$#" -eq 3 ]; then
    # 3 arguments means: main_text subtext output_filename
    MAIN_TEXT="$1"
    SUB_TEXT="$2"
    OUTPUT_NAME="$3"
else
    echo "Error: Incorrect number of positional arguments." >&2
    usage
fi

echo "Generating text image asset..."
# 3. Dynamically build the ImageMagick layers (UPDATED FOR FULL-COLOR RECOGNITION)
if [ -n "$SUB_TEXT" ]; then
    echo "Canvas=${CANVAS_SIZE} Colour=$BG_COLOUR | Font='$FONT_CHOICE' Colour=$FONT_COLOUR | Main Size=$MAIN_SIZE | Sub Size=$SUB_SIZE | Mode=Dual-Line"
    
    magick -size ${CANVAS_SIZE} xc:${BG_COLOUR} \
      \( -background none -font "$FONT_CHOICE" -fill ${FONT_COLOUR} \
         \( -gravity center -pointsize "$MAIN_SIZE" caption:"$MAIN_TEXT" -trim +repage \) \
         \( -gravity center -pointsize "$SUB_SIZE" caption:"$SUB_TEXT" -trim +repage \) \
         -gravity center -smush +40 \) \
      -gravity center -colorspace sRGB -type truecolor -composite "$OUTPUT_NAME"
else
    echo "Canvas=$CANVAS_SIZE Colour=$BG_COLOUR | Font='$FONT_CHOICE' Colour=$FONT_COLOUR | Main Size=$MAIN_SIZE | Mode=Single-Line"
    
    magick -size ${CANVAS_SIZE} xc:${BG_COLOUR} \
      \( -background none -font "$FONT_CHOICE" -fill ${FONT_COLOUR} \
         -gravity center -pointsize "$MAIN_SIZE" caption:"$MAIN_TEXT" -trim +repage \) \
      -gravity center -colorspace sRGB -type truecolor -composite "$OUTPUT_NAME"
fi

MXC=$?


# 4. Check the exit status of the magick command
if [ $MXC -eq 0 ]; then
    echo "Successfully generated: $OUTPUT_NAME"
else
    echo "Error: ImageMagick failed to generate the image asset." >&2
    exit $MXC
fi

