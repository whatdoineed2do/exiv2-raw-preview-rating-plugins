#!/bin/bash

# Default values
AUDIO_FILE=""
HEADER_IMG=""
TAIL_IMG=""
OUTPUT_FILE=""
DYNAMIC_MODE=false
CROP_MODE=false
BEAT_MODE=false
PRESERVE_CONFIG=false
SENSITIVITY="0.3"
FIXED_DURATION="0.7"

usage() {
    echo "Usage: $0 -h <header> -t <tail> -o <output.mp4> -a <audio_file> [-d] [-c] [-b] [-p] [-s sensitivity] <directory_or_file_list...>"
    echo "Options:"
    echo "  -h : Path to header image"
    echo "  -t : Path to tail image"
    echo "  -o : Path to output video mp4"
    echo "  -a : Path to audio file (or /dev/null)"
    echo "  -d : Enable dynamic timing mode"
    echo "  -c : Enable smart crop mode"
    echo "  -b : Enable beat timing mode"
    echo "  -p : Preserve temp concat text config file (deletes temp images only)"
    echo "  -s : Beat detection sensitivity (default: 0.3)"
    exit 1
}

# 1. Parse option flags using getopts
while getopts ":h:t:o:a:dcbps:" opt; do
    case ${opt} in
        h ) HEADER_IMG="$OPTARG" ;;
        t ) TAIL_IMG="$OPTARG" ;;
        o ) OUTPUT_FILE="$OPTARG" ;;
        a ) AUDIO_FILE="$OPTARG" ;;
        d ) DYNAMIC_MODE=true ;;
        c ) CROP_MODE=true ;;
        b ) BEAT_MODE=true ;;
        p ) PRESERVE_CONFIG=true ;;
        s ) SENSITIVITY="$OPTARG" ;;
        \? ) usage ;;
    esac
done
shift $((OPTIND -1))

if [ -z "$HEADER_IMG" ] || [ -z "$TAIL_IMG" ] || [ -z "$OUTPUT_FILE" ] || [ -z "$AUDIO_FILE" ] || [ "$#" -lt 1 ]; then
    usage
fi

if [ ! -f "$HEADER_IMG" ] || [ ! -f "$TAIL_IMG" ] || { [ "$AUDIO_FILE" != "/dev/null" ] && [ ! -f "$AUDIO_FILE" ]; }; then
    echo "Error: Required input files missing." >&2
    exit 1
fi

HEADER_ABS=$(realpath "$HEADER_IMG")
TAIL_ABS=$(realpath "$TAIL_IMG")

# Resolve input images
IMAGE_LIST=()
if [ -d "$1" ] && [ "$#" -eq 1 ]; then
    shopt -s nullglob
    for f in "$1"/*.jpg "$1"/*.JPG; do IMAGE_LIST+=("$f"); done
    shopt -u nullglob
else
    for f in "$@"; do IMAGE_LIST+=("$f"); done
fi

FINAL_IMAGES=()
for img in "${IMAGE_LIST[@]}"; do
    [ ! -f "$img" ] && continue
    img_abs=$(realpath "$img")
    if [[ "$img_abs" != "$HEADER_ABS" && "$img_abs" != "$TAIL_ABS" ]]; then
        FINAL_IMAGES+=("$img_abs")
    fi
done

NUM_IMAGES=${#FINAL_IMAGES[@]}
[ "$NUM_IMAGES" -eq 0 ] && { echo "Error: No source images found." >&2; exit 1; }

# Define script name and base directory for temporary files safely
SCRIPT_NAME=$(basename "$0")
TMP_DIR="${TMPDIR:-/tmp}"
TMP_PREFIX="${TMP_DIR}/.${SCRIPT_NAME}-$$"

INPUTS_TXT="${TMP_PREFIX}-inputs.txt"
TMP_BLACK="${TMP_PREFIX}-black.jpg"
TMP_HEADER="${TMP_PREFIX}-header.jpg"
TMP_TAIL="${TMP_PREFIX}-tail.jpg"

# Cleanup function: deletes generated temp images always, preserves text config if -p is set
cleanup() {
    rm -f "$TMP_BLACK" "$TMP_HEADER" "$TMP_TAIL"
    if [ "$PRESERVE_CONFIG" = true ]; then
        echo "Preserved concat config file at: $INPUTS_TXT"
    else
        rm -f "$INPUTS_TXT"
    fi
}
trap cleanup EXIT

# 2. Handle Audio Duration
FIXED_OVERHEAD="4.6"
AUDIO_LEN=0
if [ "$AUDIO_FILE" != "/dev/null" ]; then
    AUDIO_LEN=$(ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 "$AUDIO_FILE")
fi

# 3. Calculate Slide Durations Array
DURATIONS=()
if [ "$BEAT_MODE" = true ] && [ "$AUDIO_FILE" != "/dev/null" ] && command -v aubiocut &> /dev/null; then
    echo "Analyzing audio rhythms with aubio (Sensitivity: $SENSITIVITY)..."
    RAW_BEATS=($(aubiocut -i "$AUDIO_FILE" -b -t "$SENSITIVITY"))
    BEATS=()
    for b in "${RAW_BEATS[@]}"; do
        if (( $(awk "BEGIN {print ($b > 0.01) ? 1 : 0}") )); then BEATS+=("$b"); fi
    done
    
    echo "Detected ${#BEATS[@]} total valid musical beat intervals."
    B_IDX=0
    for ((i=0; i<NUM_IMAGES; i++)); do
        NEXT_IDX=$((B_IDX + 1))
        if [ $NEXT_IDX -lt ${#BEATS[@]} ]; then
            INTERVAL=$(awk "BEGIN {printf \"%.4f\", ${BEATS[$NEXT_IDX]} - ${BEATS[$B_IDX]}}")
            INTERVAL=$(awk "BEGIN {val=$INTERVAL; if(val<0.25) val=0.25; if(val>3.0) val=0.70; print val}")
            DURATIONS+=("$INTERVAL")
            B_IDX=$NEXT_IDX
        else
            DURATIONS+=("$FIXED_DURATION")
        fi
    done
else
    slide_duration="$FIXED_DURATION"
    if [ "$DYNAMIC_MODE" = true ] && [ "$AUDIO_FILE" != "/dev/null" ]; then
        slide_duration=$(awk "BEGIN {printf \"%.4f\", ($AUDIO_LEN - $FIXED_OVERHEAD) / $NUM_IMAGES}")
    fi
    for ((i=0; i<NUM_IMAGES; i++)); do DURATIONS+=("$slide_duration"); done
fi

# Compute overall video play length cleanly
expected_video_duration="2.3"
for dur in "${DURATIONS[@]}"; do
    expected_video_duration=$(awk "BEGIN {print $expected_video_duration + $dur}")
done
expected_video_duration=$(awk "BEGIN {print $expected_video_duration + 2.3}")
echo "Calculated total timeline duration: ${expected_video_duration}s"

# 4. Smart Short Audio Check and Warning System
SHORT_AUDIO_OVERRIDE=false
if [ "$AUDIO_FILE" != "/dev/null" ] && [ "$DYNAMIC_MODE" = false ] && [ "$BEAT_MODE" = false ]; then
    is_audio_shorter=$(awk "BEGIN {print ($AUDIO_LEN < $expected_video_duration) ? 1 : 0}")
    if [ "$is_audio_shorter" -eq 1 ]; then
        echo "************************************************************************"
        echo "WARNING: Provided audio file (${AUDIO_LEN}s) is SHORTER than the"
        echo "         calculated photo slideshow timeline (${expected_video_duration}s)."
        echo "         -> Bypassing abrupt truncation. Video will play to completion."
        echo "************************************************************************"
        SHORT_AUDIO_OVERRIDE=true
    fi
fi

# 5. Build Input Playlist Timeline Track using the fast Concat format
echo "Assembling timeline layout structure..."

# Standardize Header, Tail, and Black images to exact 1920x1080 dimensions
ffmpeg -y -i "$HEADER_ABS" -vf "scale=1920:1080:force_original_aspect_ratio=decrease,pad=1920:1080:(1920-iw)/2:(1080-ih)/2:black" -pix_fmt yuv420p -vframes 1 "$TMP_HEADER" 2>/dev/null
ffmpeg -y -i "$TAIL_ABS"   -vf "scale=1920:1080:force_original_aspect_ratio=decrease,pad=1920:1080:(1920-iw)/2:(1080-ih)/2:black" -pix_fmt yuv420p -vframes 1 "$TMP_TAIL" 2>/dev/null
ffmpeg -y -f lavfi -i color=c=black:s=1920x1080 -pix_fmt yuv420p -vframes 1 "$TMP_BLACK" 2>/dev/null

# Assemble Concat file using normalized header and tail
echo -e "file '$TMP_HEADER'\nduration 2.0\nfile '$TMP_BLACK'\nduration 0.3" > "$INPUTS_TXT"
for i in "${!FINAL_IMAGES[@]}"; do
    echo -e "file '${FINAL_IMAGES[$i]}'\nduration ${DURATIONS[$i]}" >> "$INPUTS_TXT"
done
echo -e "file '$TMP_BLACK'\nduration 0.3\nfile '$TMP_TAIL'\nduration 2.0\nfile '$TMP_TAIL'" >> "$INPUTS_TXT"

# 6. Configure Filters & Audio Maps Safely
FFMPEG_AUDIO_ARGS=()
AUDIO_FILTER=""
MAP_ARGS=("-map" "[final_video_out]")

if [ "$AUDIO_FILE" = "/dev/null" ]; then
    FFMPEG_AUDIO_ARGS+=("-f" "lavfi" "-i" "anullsrc=channel_layout=stereo:sample_rate=44100")
    AUDIO_FILTER=";[1:a]amix=inputs=1[audio_out]"
    MAP_ARGS+=("-map" "[audio_out]")
else
    FFMPEG_AUDIO_ARGS+=("-i" "$AUDIO_FILE" "-f" "lavfi" "-i" "anullsrc=channel_layout=stereo:sample_rate=44100")
    MAP_ARGS+=("-map" "[audio_out]")
    
    if [ "$DYNAMIC_MODE" = true ]; then
        AUDIO_FILTER=";[2:a][1:a]amix=inputs=2:duration=first[audio_out]"
    else
        FADE_DURATION="4.0"
        fade_start=$(awk "BEGIN {val = $expected_video_duration - $FADE_DURATION; print (val > 0) ? val : 0}")
        echo "Audio tracking: Applying ${FADE_DURATION}s fade-out starting at ${fade_start}s (ends at ${expected_video_duration}s)."
        AUDIO_FILTER=";[1:a]afade=t=out:st=${fade_start}:d=${FADE_DURATION},atrim=0:${expected_video_duration}[audio_fade];[2:a][audio_fade]amix=inputs=2:duration=first[audio_out]"
    fi
fi

if [ "$CROP_MODE" = true ]; then
    echo "Layout style: Smart Crop (Landscape fills screen, Portrait uses blurred edges)."
    VIDEO_FILTER="format=yuv420p,split[bg][fg];[bg]scale=1920:1080:force_original_aspect_ratio=increase,crop=1920:1080,gblur=sigma=20[bg_blurred];[fg]scale='if(gt(iw\,ih)\,1920\,-1)':'if(gt(iw\,ih)\,-1\,1080)'[fg_scaled];[bg_blurred][fg_scaled]overlay=(main_w-overlay_w)/2:(main_h-overlay_h)/2,crop=1920:1080,fps=30,format=nv12[final_video_out]"
else
    echo "Layout style: Blurred Background Padding for all images."
    VIDEO_FILTER="format=yuv420p,split[bg][fg];[bg]scale=1920:1080:force_original_aspect_ratio=increase,crop=1920:1080,gblur=sigma=20[bg_blurred];[fg]scale=1920:1080:force_original_aspect_ratio=decrease[fg_scaled];[bg_blurred][fg_scaled]overlay=(main_w-overlay_w)/2:(main_h-overlay_h)/2,fps=30,format=nv12[final_video_out]"
fi

# Append NV12 hardware layout standard inside the global filter string
FILTER_COMPLEX="${VIDEO_FILTER}${AUDIO_FILTER}"

# 7. Execute Hardware Accelerated Render Track
echo "Encoding video using Intel Quick Sync..."
ffmpeg -loglevel info -stats -f concat -safe 0 -i "$INPUTS_TXT" "${FFMPEG_AUDIO_ARGS[@]}" \
  -filter_complex "$FILTER_COMPLEX" "${MAP_ARGS[@]}" \
  -fps_mode cfr -t "$expected_video_duration" \
  -c:v hevc_qsv -preset slow -global_quality 20 "$OUTPUT_FILE"

FFMPEG_EXIT_CODE=$?

if [ $FFMPEG_EXIT_CODE -eq 0 ]; then
    echo "Process complete! File output successfully located at: $OUTPUT_FILE"
else
    echo "Error: Video compilation failed inside FFmpeg during processing loop." >&2
    exit $FFMPEG_EXIT_CODE
fi
