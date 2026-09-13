#!/bin/bash

# Default values
AUDIO_FILE=""
HEADER_IMG=""
TAIL_IMG=""
OUTPUT_FILE=""
DYNAMIC_MODE=false
CROP_MODE=false
FIXED_DURATION="0.7"

# Usage help function
usage() {
    echo "Usage: $0 -h <header> -t <tail> -o <output.mp4> -a <audio_file> [-d] [-c] <directory_or_file_list...>"
    echo "Options:"
    echo "  -h : Path to header image"
    echo "  -t : Path to tail image"
    echo "  -o : Path to output video mp4"
    echo "  -a : Path to audio file (or /dev/null)"
    echo "  -d : Enable dynamic timing mode (stretches/shrinks image duration to match audio length)"
    echo "  -c : Enable smart crop mode (crops landscape 3:2 to fill screen, keeps blur padding for portraits)"
    exit 1
}

# 1. Parse option flags using getopts
while getopts ":h:t:o:a:dc" opt; do
    case ${opt} in
        h ) HEADER_IMG="$OPTARG" ;;
        t ) TAIL_IMG="$OPTARG" ;;
        o ) OUTPUT_FILE="$OPTARG" ;;
        a ) AUDIO_FILE="$OPTARG" ;;
        d ) DYNAMIC_MODE=true ;;
        c ) CROP_MODE=true ;;
        \? ) echo "Error: Invalid option -$OPTARG" >&2; usage ;;
        : ) echo "Error: Option -$OPTARG requires an argument." >&2; usage ;;
    esac
done
shift $((OPTIND -1))

# Check that all required flag options were provided
if [ -z "$HEADER_IMG" ] || [ -z "$TAIL_IMG" ] || [ -z "$OUTPUT_FILE" ] || [ -z "$AUDIO_FILE" ]; then
    echo "Error: Missing required option flags (-h, -t, -o, or -a)." >&2
    usage
fi

if [ "$#" -lt 1 ]; then
    echo "Error: You must provide a source directory or a list of images." >&2
    usage
fi

# 2. File and Path Validation
if [ ! -f "$HEADER_IMG" ] || [ ! -f "$TAIL_IMG" ]; then
    echo "Error: Header or Tail file does not exist." >&2
    exit 1
fi

if [ "$AUDIO_FILE" != "/dev/null" ] && [ ! -f "$AUDIO_FILE" ]; then
    echo "Error: Audio file '$AUDIO_FILE' not found." >&2
    exit 1
fi

HEADER_ABS=$(realpath "$HEADER_IMG")
TAIL_ABS=$(realpath "$TAIL_IMG")

# Resolve image source inputs
IMAGE_LIST=()
if [ -d "$1" ] && [ "$#" -eq 1 ]; then
    shopt -s nullglob
    for f in "$1"/*.jpg "$1"/*.JPG; do
        IMAGE_LIST+=("$f")
    done
    shopt -u nullglob
else
    for f in "$@"; do
        IMAGE_LIST+=("$f")
    done
fi

# Filter out header and tail from the image pool
FINAL_IMAGES=()
for img in "${IMAGE_LIST[@]}"; do
    [ ! -f "$img" ] && continue
    img_abs=$(realpath "$img")
    if [[ "$img_abs" != "$HEADER_ABS" && "$img_abs" != "$TAIL_ABS" ]]; then
        FINAL_IMAGES+=("$img_abs")
    fi
done

NUM_IMAGES=${#FINAL_IMAGES[@]}
if [ "$NUM_IMAGES" -eq 0 ]; then
    echo "Error: No valid main source images found to process." >&2
    exit 1
fi

# 3. Handle Duration Calculation
FIXED_OVERHEAD="4.6"
slide_duration="$FIXED_DURATION"
AUDIO_LEN=0

if [ "$AUDIO_FILE" != "/dev/null" ]; then
    AUDIO_LEN=$(ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 "$AUDIO_FILE")
fi

if [ "$DYNAMIC_MODE" = true ]; then
    if [ "$AUDIO_FILE" = "/dev/null" ]; then
        echo "Warning: Dynamic mode requested but no audio provided. Falling back to 0.7s per slide."
    else
        echo "Calculating dynamic image frame lengths..."
        is_long_enough=$(awk "BEGIN {print ($AUDIO_LEN > $FIXED_OVERHEAD) ? 1 : 0}")
        if [ "$is_long_enough" -eq 1 ]; then
            slide_duration=$(awk "BEGIN {printf \"%.4f\", ($AUDIO_LEN - $FIXED_OVERHEAD) / $NUM_IMAGES}")
            echo "Audio Length: ${AUDIO_LEN}s | Main Images: $NUM_IMAGES"
            echo "Calculated perfect frame duration per image: ${slide_duration}s"
        else
            echo "Warning: Audio file too short for fixed dynamic overhead. Falling back to 0.7s."
            DYNAMIC_MODE=false
        fi
    fi
fi

# Calculate total expected video length based on the final slide duration configuration
expected_video_duration=$(awk "BEGIN {print 4.6 + ($NUM_IMAGES * $slide_duration)}")

# 4. Smart Short Audio Check and Warning System
SHORT_AUDIO_OVERRIDE=false
if [ "$AUDIO_FILE" != "/dev/null" ] && [ "$DYNAMIC_MODE" = false ]; then
    is_audio_shorter=$(awk "BEGIN {print ($AUDIO_LEN < $expected_video_duration) ? 1 : 0}")
    if [ "$is_audio_shorter" -eq 1 ]; then
        echo "************************************************************************"
        echo "WARNING: Provided audio file (${AUDIO_LEN}s) is SHORTER than the"
        echo "         calculated photo slideshow timeline (${expected_video_duration}s)."
        echo "         -> Bypassing abrupt truncation. Video will play to completion,"
        echo "            and the end of the video will play in silence."
        echo "         -> Tip: Run with the '-d' flag if you want photos to fit the track!"
        echo "************************************************************************"
        SHORT_AUDIO_OVERRIDE=true
    fi
fi

# 5. Build Input Mappings using Bash Arrays (Fixes Space Mismatch)
echo "Building playlist timeline for $NUM_IMAGES photos..."
FFMPEG_INPUT_ARGS=()
FILTER_COMPLEX=""
INDEX=0

# Add Header
FFMPEG_INPUT_ARGS+=("-loop" "1" "-t" "2.0" "-i" "$HEADER_ABS")
FILTER_COMPLEX+="[$INDEX:v]scale=1920:1080:force_original_aspect_ratio=increase,crop=1920:1080,format=yuv420p[v$INDEX];"
INDEX=$((INDEX + 1))

# Add Pre-gap (Black frame)
FFMPEG_INPUT_ARGS+=("-f" "lavfi" "-t" "0.3" "-i" "color=c=black:s=1920x1080:r=30")
FILTER_COMPLEX+="[$INDEX:v]format=yuv420p[v$INDEX];"
INDEX=$((INDEX + 1))

# Add Slides Loop with Smart Aspect Ratio Detection
if [ "$CROP_MODE" = true ]; then
    echo "Layout style: Smart Crop (Landscape fills screen, Portrait uses blurred edges)."
else
    echo "Layout style: Blurred Background Padding for all images."
fi

for img_abs in "${FINAL_IMAGES[@]}"; do
    FFMPEG_INPUT_ARGS+=("-loop" "1" "-t" "$slide_duration" "-i" "$img_abs")
    
    if [ "$CROP_MODE" = true ]; then
        FILTER_COMPLEX+="[$INDEX:v]format=yuv420p,split[bg][fg];"
        FILTER_COMPLEX+="[bg]scale=1920:1080:force_original_aspect_ratio=increase,crop=1920:1080,gblur=sigma=20[bg_blurred];"
        FILTER_COMPLEX+="[fg]scale='if(gt(iw\,ih)\,1920\,-1)':'if(gt(iw\,ih)\,-1\,1080)'[fg_scaled];"
        FILTER_COMPLEX+="[bg_blurred][fg_scaled]overlay=(main_w-overlay_w)/2:(main_h-overlay_h)/2,crop=1920:1080[v$INDEX];"
    else
        FILTER_COMPLEX+="[$INDEX:v]format=yuv420p,split[bg][fg];"
        FILTER_COMPLEX+="[bg]scale=1920:1080:force_original_aspect_ratio=increase,crop=1920:1080,gblur=sigma=20[bg_blurred];"
        FILTER_COMPLEX+="[fg]scale=1920:1080:force_original_aspect_ratio=decrease[fg_scaled];"
        FILTER_COMPLEX+="[bg_blurred][fg_scaled]overlay=(main_w-overlay_w)/2:(main_h-overlay_h)/2[v$INDEX];"
    fi
    INDEX=$((INDEX + 1))
done

# Add Post-gap (Black frame)
FFMPEG_INPUT_ARGS+=("-f" "lavfi" "-t" "0.3" "-i" "color=c=black:s=1920x1080:r=30")
FILTER_COMPLEX+="[$INDEX:v]format=yuv420p[v$INDEX];"
INDEX=$((INDEX + 1))

# Add Tail
FFMPEG_INPUT_ARGS+=("-loop" "1" "-t" "2.0" "-i" "$TAIL_ABS")
FILTER_COMPLEX+="[$INDEX:v]scale=1920:1080:force_original_aspect_ratio=increase,crop=1920:1080,format=yuv420p[v$INDEX];"
INDEX=$((INDEX + 1))

# Append the Concat block sequence instruction and explicitly force 30fps output
CONCAT_INPUTS=""
for ((i=0; i<INDEX; i++)); do
    CONCAT_INPUTS+="[v$i]"
done
FILTER_COMPLEX+="${CONCAT_INPUTS}concat=n=${INDEX}:v=1:a=0,fps=30[video_out]"

# 6. Define Audio Mapping Arguments safely using array mechanics
AUDIO_OUT_MAP=("-map" "[final_video_out]")

if [ "$AUDIO_FILE" != "/dev/null" ]; then
    FFMPEG_INPUT_ARGS+=("-i" "$AUDIO_FILE")
    AUDIO_INDEX=$INDEX
    
    if [ "$DYNAMIC_MODE" = true ]; then
        echo "Audio tracking: Full runtime sync alignment chosen."
        AUDIO_OUT_MAP+=("-map" "$AUDIO_INDEX:a" "-shortest")
    elif [ "$SHORT_AUDIO_OVERRIDE" = true ]; then
        AUDIO_OUT_MAP+=("-map" "$AUDIO_INDEX:a")
    else
        fade_start=$(awk "BEGIN {print $expected_video_duration - 5.0}")
        echo "Audio tracking: Fixed timing mode. Applying 2-second audio fade-out starting at ${fade_start}s."
        FILTER_COMPLEX+=";[$AUDIO_INDEX:a]afade=t=out:st=${fade_start}:d=5.0[audio_out]"
        AUDIO_OUT_MAP+=("-map" "[audio_out]" "-shortest")
    fi
fi

# Append NV12 hardware layout standard inside the global filter string
FILTER_COMPLEX+=";[video_out]format=nv12[final_video_out]"

# 7. Execute Intel QSV Hardware Encoder Command safely with Array Expansion
echo "Encoding video using Intel Quick Sync..."
# Crucial addition: We add '-t' right before the output name to force a hard cutoff at the calculated duration.
ffmpeg -y -loglevel info -stats "${FFMPEG_INPUT_ARGS[@]}" \
  -filter_complex "$FILTER_COMPLEX" \
  "${AUDIO_OUT_MAP[@]}" \
  -t "$expected_video_duration" \
  -c:v hevc_qsv -preset fast "$OUTPUT_FILE"

FFMPEG_EXIT_CODE=$?

# 8. Verify the Exit Status
if [ $FFMPEG_EXIT_CODE -eq 0 ]; then
    echo "Process complete! File output successfully located at: $OUTPUT_FILE"
else
    echo "Error: Video compilation failed inside FFmpeg during processing loop." >&2
    exit 1
fi

