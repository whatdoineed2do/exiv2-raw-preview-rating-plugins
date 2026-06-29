#!/bin/bash
#
failed_files=()

for i in "$@"; do
    # 1. Gather all raw metadata text components
    EXIF_DATA="$(exiftool -s3 -p '$LensID @ f/$Aperture' "$i" 2>/dev/null)"
    # EXIF_DATA="$(exiftool -s3 -p '$LensID @ f/$Aperture'$'\n''$CreateDate' "$i" 2>/dev/null)"

    # If the EXIF data is empty, log the failure and skip to the next file
    if [ -z "$EXIF_DATA" ]; then
        failed_files+=("$i")
        continue
    fi

    # params
    IM_TEXT_LOCATION="${IM_TEXT_LOCATION:-northwest}"
    # IM_CROP="-gravity center -crop 2000x800+0+0 +repage"
    # IM_STROKE="-stroke black -strokewidth 2"
    IM_BOUND_TEXT_COLOUR="${IM_BOUND_TEXT_COLOUR:-white}"
    IM_BOUND_BOX_COLOUR="${IM_BOUND_BOX_COLOUR:-grey20}"
    IM_BOUND_BOX_OPACITY="${IM_BOUND_BOX_OPACITY:-0.6}"       # opacity of bounded box, 1 - solid, 0 - fully transparent   
    IM_BOUND_TEXT_Y_OFFSET="${IM_BOUND_TEXT_Y_OFFSET:-0.05}"  # % offset from vertical edge
    IM_BOUND_TEXT_X_OFFSET="${IM_BOUND_TEXT_X_OFFSET:-0.05}"  # % offset from horizontal edge 
    IM_PADDING_RATIO="${IM_PADDING_RATIO:-0.03}"              # % padding ratio of bounded box and text
    IM_FONT="${IM_FONT:-Oxanium-Regular}"

    DIM=$(magick "$i" ${IM_CROP} -format "%wx%h" info:)

    # IM_TEXT="${EXIF_DATA}"$'\n'"${DIM}"
    IM_TEXT="${EXIF_DATA}"

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
            -draw "roundrectangle 0,0 %[fx:w-1],%[fx:h-1] 12,12" \
            -channel A -evaluate multiply ${IM_BOUND_BOX_OPACITY} +channel \
         \) \
         +swap -compose Over -composite \
      \) \
      -gravity ${IM_TEXT_LOCATION} \
      -geometry "+${X_OFF}+${Y_OFF}" \
      -composite \
      "out-$i"
done

if [ ${#failed_files[@]} -ne 0 ]; then
    echo "The following files failed due to missing EXIF data:"
    for failed in "${failed_files[@]}"; do
        echo "  $failed"
    done
fi
