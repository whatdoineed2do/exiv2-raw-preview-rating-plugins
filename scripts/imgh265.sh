#!/bin/bash

# Ensure script-level traps don't trigger inside child processes
set -E

# Define script name and base directory for temporary files safely
SCRIPT_NAME=$(basename "$0")
TMP_DIR="${TMPDIR:-/tmp}"
TMP_PREFIX="${TMP_DIR}/.${SCRIPT_NAME}-$$"
TMP_SLIDES_DIR="${TMP_PREFIX}-slides"

# Set up paths for temp files
INPUTS_TXT="${TMP_PREFIX}-inputs.txt"
TMP_BLACK_CLIP="${TMP_PREFIX}-black.avi"
TMP_HEADER_CLIP="${TMP_PREFIX}-header.avi"
TMP_TAIL_CLIP="${TMP_PREFIX}-tail.avi"

# Default flag values
AUDIO_FILE="/dev/null"
HEADER_IMG=""
TAIL_IMG=""
OUTPUT_FILE=""
TARGET_DURATION_RAW=""
DYNAMIC_MODE=false
CROP_MODE=false
BEAT_MODE=false
PRESERVE_CONFIG=false
OVERWRITE_OUTPUT=""
SENSITIVITY="0.3"
FIXED_DURATION="0.7"

usage() {
    echo "Usage: $0 -o <output.mp4> [-L length] [-h header] [-t tail] [-a audio_file] [-y] [-d] [-c] [-b] [-p] [-s sensitivity] <directory_or_file_list...>"
    echo "Options:"
    echo "  -o : Path to output video mp4 (Required)"
    echo "  -L : Target total length in HH:MM:SS:ms (or HH:MM:SS.ms / seconds)"
    echo "  -h : Path to header image (Optional)"
    echo "  -t : Path to tail image (Optional)"
    echo "  -a : Path to audio file or /dev/null (Optional)"
    echo "  -y : Overwrite output file without asking"
    echo "  -d : Enable dynamic timing mode"
    echo "  -c : Enable smart crop mode (center crop landscape; blur pad portrait; direct scale 16:9)"
    echo "  -b : Enable beat timing mode"
    echo "  -p : Preserve temp concat text config file"
    echo "  -s : Beat detection sensitivity (default: 0.3)"
    exit 1
}

# Helper to convert HH:MM:SS:sss or HH:MM:SS.sss into floating-point total seconds
parse_time_to_seconds() {
    local raw_time="$1"
    awk -v t="$raw_time" 'BEGIN {
        gsub(/\./, ":", t);
        n = split(t, parts, ":");
        if (n == 1) {
            sec = parts[1] + 0.0;
        } else if (n == 2) {
            sec = (parts[1] * 60) + parts[2];
        } else if (n == 3) {
            sec = (parts[1] * 3600) + (parts[2] * 60) + parts[3];
        } else if (n == 4) {
            ms = parts[4];
            while (length(ms) < 3) ms = ms "0";
            ms_val = ("0." ms) + 0.0;
            sec = (parts[1] * 3600) + (parts[2] * 60) + parts[3] + ms_val;
        } else {
            sec = -1;
        }
        printf "%.4f", sec;
    }'
}

# Cleanup handler: Handles exit and Ctrl+C interrupts cleanly
cleanup() {
    local exit_code=$?
    rm -rf "$TMP_SLIDES_DIR"
    rm -f "$TMP_BLACK_CLIP" "$TMP_HEADER_CLIP" "$TMP_TAIL_CLIP" "${TMP_PREFIX}"-heic-*.png
    if [ "$PRESERVE_CONFIG" = true ]; then
        echo "Preserved concat config file at: $INPUTS_TXT"
    else
        rm -f "$INPUTS_TXT"
    fi
    exit $exit_code
}
trap cleanup EXIT INT TERM

# 1. Parse option flags using getopts
while getopts ":h:t:o:a:L:ydcbps:" opt; do
    case ${opt} in
        h ) HEADER_IMG="$OPTARG" ;;
        t ) TAIL_IMG="$OPTARG" ;;
        o ) OUTPUT_FILE="$OPTARG" ;;
        a ) AUDIO_FILE="$OPTARG" ;;
        L ) TARGET_DURATION_RAW="$OPTARG" ;;
        y ) OVERWRITE_OUTPUT="-y" ;;
        d ) DYNAMIC_MODE=true ;;
        c ) CROP_MODE=true ;;
        b ) BEAT_MODE=true ;;
        p ) PRESERVE_CONFIG=true ;;
        s ) SENSITIVITY="$OPTARG" ;;
        \? ) echo "Error: Invalid option -$OPTARG" >&2; usage ;;
        : ) echo "Error: Option -$OPTARG requires an argument." >&2; usage ;;
    esac
done
shift $((OPTIND -1))

# Validate required parameters
if [ -z "$OUTPUT_FILE" ] || [ "$#" -lt 1 ]; then
    echo "Error: Missing output file (-o) or source image input list." >&2
    usage
fi

# Validate input files if explicitly provided
if [ -n "$HEADER_IMG" ] && [ ! -f "$HEADER_IMG" ]; then
    echo "Error: Specified header image does not exist: $HEADER_IMG" >&2
    exit 1
fi
if [ -n "$TAIL_IMG" ] && [ ! -f "$TAIL_IMG" ]; then
    echo "Error: Specified tail image does not exist: $TAIL_IMG" >&2
    exit 1
fi
if [ "$AUDIO_FILE" != "/dev/null" ] && [ ! -f "$AUDIO_FILE" ]; then
    echo "Error: Specified audio file does not exist: $AUDIO_FILE" >&2
    exit 1
fi

HEADER_ABS=""
[ -n "$HEADER_IMG" ] && HEADER_ABS=$(realpath "$HEADER_IMG")

TAIL_ABS=""
[ -n "$TAIL_IMG" ] && TAIL_ABS=$(realpath "$TAIL_IMG")

# Resolve input images (supports jpg, jpeg, png, webp, gif, heic)
IMAGE_LIST=()
if [ -d "$1" ] && [ "$#" -eq 1 ]; then
    shopt -s nullglob nocaseglob
    for f in "$1"/*.{jpg,jpeg,png,webp,gif,heic,heif}; do IMAGE_LIST+=("$f"); done
    shopt -u nullglob nocaseglob
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
[ "$NUM_IMAGES" -eq 0 ] && { echo "Error: No valid source images found." >&2; exit 1; }

# 2. Handle Audio Duration & Dependency Verification
AUDIO_LEN=0
if [ "$AUDIO_FILE" != "/dev/null" ]; then
    AUDIO_LEN=$(ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 "$AUDIO_FILE")
fi

# Determine active overhead timing dynamically
HEADER_OVERHEAD="0.0"
TAIL_OVERHEAD="0.0"

[ -n "$HEADER_ABS" ] && HEADER_OVERHEAD="2.3" # 2.0s card + 0.3s black spacer
[ -n "$TAIL_ABS" ]   && TAIL_OVERHEAD="2.3"   # 0.3s black spacer + 2.0s card

FIXED_OVERHEAD=$(awk "BEGIN {printf \"%.4f\", $HEADER_OVERHEAD + $TAIL_OVERHEAD}")

# 3. Calculate Slide Durations Array
DURATIONS=()

if [ -n "$TARGET_DURATION_RAW" ]; then
    TARGET_SECONDS=$(parse_time_to_seconds "$TARGET_DURATION_RAW")
    is_valid_target=$(awk "BEGIN {print ($TARGET_SECONDS > $FIXED_OVERHEAD) ? 1 : 0}")

    if [ "$is_valid_target" -ne 1 ]; then
        echo "Error: Specified length ($TARGET_SECONDSs) must be greater than header/tail overhead (${FIXED_OVERHEAD}s)." >&2
        exit 1
    fi

    slide_duration=$(awk "BEGIN {printf \"%.4f\", ($TARGET_SECONDS - $FIXED_OVERHEAD) / $NUM_IMAGES}")
    echo "Explicit Length Target (-L): $TARGET_DURATION_RAW (${TARGET_SECONDS}s)"
    echo "  -> Override active: Each of $NUM_IMAGES slides set to ${slide_duration}s."

    for ((i=0; i<NUM_IMAGES; i++)); do DURATIONS+=("$slide_duration"); done

elif [ "$BEAT_MODE" = true ]; then
    if ! command -v aubiocut &>/dev/null; then
        echo "Error: Beat mode (-b) requires 'aubiocut', but it is not installed or in PATH." >&2
        exit 1
    fi

    if [ "$AUDIO_FILE" != "/dev/null" ]; then
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
        for ((i=0; i<NUM_IMAGES; i++)); do DURATIONS+=("$FIXED_DURATION"); done
    fi
else
    slide_duration="$FIXED_DURATION"
    if [ "$DYNAMIC_MODE" = true ] && [ "$AUDIO_FILE" != "/dev/null" ]; then
        slide_duration=$(awk "BEGIN {val = ($AUDIO_LEN - $FIXED_OVERHEAD) / $NUM_IMAGES; print (val < 0.25) ? 0.25 : val}")
        echo "Dynamic Mode Active: Per-slide duration set to ${slide_duration}s across $NUM_IMAGES images."
    fi
    for ((i=0; i<NUM_IMAGES; i++)); do DURATIONS+=("$slide_duration"); done
fi

expected_video_duration="$FIXED_OVERHEAD"
for dur in "${DURATIONS[@]}"; do
    expected_video_duration=$(awk "BEGIN {print $expected_video_duration + $dur}")
done
echo "Calculated total timeline duration: ${expected_video_duration}s"

# 4. Short Audio Warning System
if [ -z "$TARGET_DURATION_RAW" ] && [ "$AUDIO_FILE" != "/dev/null" ] && [ "$DYNAMIC_MODE" = false ] && [ "$BEAT_MODE" = false ]; then
    is_audio_shorter=$(awk "BEGIN {print ($AUDIO_LEN < $expected_video_duration) ? 1 : 0}")
    if [ "$is_audio_shorter" -eq 1 ]; then
        echo "************************************************************************"
        echo "WARNING: Audio file (${AUDIO_LEN}s) is SHORTER than timeline (${expected_video_duration}s)."
        echo "         -> Video will play to completion with trailing silence."
        echo "************************************************************************"
    fi
fi

# 5. Preprocess Slides into Lightweight AVI Intermediate Clips
mkdir -p "$TMP_SLIDES_DIR"
echo "Preprocessing $NUM_IMAGES images into intermediate slide clips..."

STD_FILTER="scale=1920:1080:force_original_aspect_ratio=decrease,pad=1920:1080:(1920-iw)/2:(1080-ih)/2:black,setsar=1,format=yuv420p"

# Render black spacer clip only if needed
if [ -n "$HEADER_ABS" ] || [ -n "$TAIL_ABS" ]; then
    ffmpeg -y -loglevel error -threads 0 -f lavfi -i color=c=black:s=1920x1080:r=30 -vf "format=yuv420p" -r 30 -t 0.3 -c:v mjpeg -q:v 2 "$TMP_BLACK_CLIP" || exit 1
fi

# Render header clip if provided
if [ -n "$HEADER_ABS" ]; then
    ffmpeg -y -loglevel error -threads 0 -loop 1 -i "$HEADER_ABS" -vf "$STD_FILTER" -r 30 -t 2.0 -c:v mjpeg -q:v 2 "$TMP_HEADER_CLIP" || exit 1
fi

# Render tail clip if provided
if [ -n "$TAIL_ABS" ]; then
    ffmpeg -y -loglevel error -threads 0 -loop 1 -i "$TAIL_ABS" -vf "$STD_FILTER" -r 30 -t 2.0 -c:v mjpeg -q:v 2 "$TMP_TAIL_CLIP" || exit 1
fi

VALID_CLIPS=()
FAILED_COUNT=0
TOTAL_SLIDE_DURATION=0

# Render each slide sequentially
for i in "${!FINAL_IMAGES[@]}"; do
    img="${FINAL_IMAGES[$i]}"
    clip_out=$(printf "%s/clip_%04d.avi" "$TMP_SLIDES_DIR" "$i")
    dur="${DURATIONS[$i]}"

    ext="${img##*.}"
    ext_lc=$(echo "$ext" | tr '[:upper:]' '[:lower:]')

    src_img="$img"
    if [[ "$ext_lc" == "heic" || "$ext_lc" == "heif" ]]; then
        tmp_heic_png="${TMP_PREFIX}-heic-${i}.png"
        if command -v heif-convert &>/dev/null; then
            heif-convert "$img" "$tmp_heic_png" &>/dev/null && src_img="$tmp_heic_png"
        elif command -v magick &>/dev/null; then
            magick "$img" "$tmp_heic_png" &>/dev/null && src_img="$tmp_heic_png"
        fi
    fi

    # Read visually oriented image dimensions via ffprobe (accounting for EXIF rotation)
    img_w=0
    img_h=0
    # Extract dimensions using ffprobe with fallback defaults
    eval $(ffprobe -v error -select_streams v:0 -show_entries stream=width,height -of flat "$src_img" 2>/dev/null | sed 's/streams.stream.0./img_/')
    
    # Fallback to exiftool if ffprobe failed to extract width/height
    if [ -z "$img_w" ] || [ "$img_w" -eq 0 ] 2>/dev/null; then
        if command -v exiftool &>/dev/null; then
            img_w=$(exiftool -s3 -ImageWidth "$src_img" 2>/dev/null)
            img_h=$(exiftool -s3 -ImageHeight "$src_img" 2>/dev/null)
        fi
    fi

    # Calculate metrics with strict non-zero checking
    EVAL_RESULT=$(awk -v w="${img_w:-0}" -v h="${img_h:-0}" 'BEGIN {
        if (h <= 0 || w <= 0) {
            print "0 0"; # invalid dimensions fallback
            exit;
        }
        ratio = w / h;
        target = 16.0 / 9.0;
        diff = ratio - target;
        if (diff < 0) diff = -diff;
        
        is_16_9 = (diff < 0.02) ? 1 : 0;
        is_portrait = (h > w) ? 1 : 0;
        
        print is_16_9 " " is_portrait;
    }')

    IS_16_9=$(echo "$EVAL_RESULT" | awk '{print $1}')
    IS_PORTRAIT=$(echo "$EVAL_RESULT" | awk '{print $2}')

    if [ "$CROP_MODE" = true ]; then
        if [ "$IS_16_9" -eq 1 ]; then
            # Direct scale for 16:9 matching frames
            FILTER_PRESET="scale=1920:1080,setsar=1,format=yuv420p"
        elif [ "$IS_PORTRAIT" -eq 1 ]; then
            # Strict Portrait (H > W): Blurred background padding
            FILTER_PRESET="format=yuv420p,split[bg][fg];[bg]scale=1920:1080:force_original_aspect_ratio=increase,crop=1920:1080,gblur=sigma=20[bg_blurred];[fg]scale=1920:1080:force_original_aspect_ratio=decrease[fg_scaled];[bg_blurred][fg_scaled]overlay=(main_w-overlay_w)/2:(main_h-overlay_h)/2,setsar=1"
        else
            # 3:2, 4:3, etc. Landscape (W > H): Scale & Center Crop top/bottom
            FILTER_PRESET="scale=1920:1080:force_original_aspect_ratio=increase,crop=1920:1080,setsar=1,format=yuv420p"
        fi
    else
        # Standard mode: Fit everything over blurred background padding
        FILTER_PRESET="format=yuv420p,split[bg][fg];[bg]scale=1920:1080:force_original_aspect_ratio=increase,crop=1920:1080,gblur=sigma=20[bg_blurred];[fg]scale=1920:1080:force_original_aspect_ratio=decrease[fg_scaled];[bg_blurred][fg_scaled]overlay=(main_w-overlay_w)/2:(main_h-overlay_h)/2,setsar=1"
    fi

    ffmpeg -y -loglevel error -threads 0 -loop 1 -i "$src_img" -vf "$FILTER_PRESET" -r 30 -t "$dur" -c:v mjpeg -q:v 2 "$clip_out"

    if [ -s "$clip_out" ]; then
        VALID_CLIPS+=("$clip_out")
        TOTAL_SLIDE_DURATION=$(awk "BEGIN {printf \"%.6f\", $TOTAL_SLIDE_DURATION + $dur}")
    else
        echo "Warning: Skipped corrupt or unreadable image ($i): $(basename "$img")" >&2
        rm -f "$clip_out" 2>/dev/null
        ((FAILED_COUNT++))
    fi
done

expected_video_duration=$(awk "BEGIN {printf \"%.6f\", $FIXED_OVERHEAD + $TOTAL_SLIDE_DURATION}")

echo "Intermediate Preprocessing Summary:"
echo "  - Total Valid Slides Rendered: ${#VALID_CLIPS[@]}"
echo "  - Failed/Skipped Images: ${FAILED_COUNT}"
echo "  - Adjusted Video Duration: ${expected_video_duration}s"

[ "${#VALID_CLIPS[@]}" -eq 0 ] && { echo "Error: Zero slides rendered successfully." >&2; exit 1; }

# Build Concat Manifest File
> "$INPUTS_TXT"

if [ -n "$HEADER_ABS" ]; then
    echo "file '$TMP_HEADER_CLIP'" >> "$INPUTS_TXT"
    echo "file '$TMP_BLACK_CLIP'" >> "$INPUTS_TXT"
fi

for clip in "${VALID_CLIPS[@]}"; do
    echo "file '$clip'" >> "$INPUTS_TXT"
done

if [ -n "$TAIL_ABS" ]; then
    echo "file '$TMP_BLACK_CLIP'" >> "$INPUTS_TXT"
    echo "file '$TMP_TAIL_CLIP'" >> "$INPUTS_TXT"
fi

# 6. Configure Audio Input
FFMPEG_AUDIO_ARGS=()
AUDIO_FILTER=""

if [ "$AUDIO_FILE" = "/dev/null" ]; then
    FFMPEG_AUDIO_ARGS+=("-f" "lavfi" "-i" "anullsrc=channel_layout=stereo:sample_rate=44100")
    AUDIO_FILTER="[1:a]amix=inputs=1[audio_out]"
else
    FFMPEG_AUDIO_ARGS+=("-i" "$AUDIO_FILE" "-f" "lavfi" "-i" "anullsrc=channel_layout=stereo:sample_rate=44100")
    if [ -n "$TARGET_DURATION_RAW" ] || [ "$DYNAMIC_MODE" = true ]; then
        AUDIO_FILTER="[1:a][2:a]amix=inputs=2:duration=first[audio_out]"
    else
        FADE_DURATION="4.0"
        fade_start=$(awk "BEGIN {val = $expected_video_duration - $FADE_DURATION; print (val > 0) ? val : 0}")
        AUDIO_FILTER="[1:a]afade=t=out:st=${fade_start}:d=${FADE_DURATION},atrim=0:${expected_video_duration}[audio_fade];[2:a][audio_fade]amix=inputs=2:duration=first[audio_out]"
    fi
fi

# 7. Final Hardware Accelerated Encoding Pass
echo "Encoding master slideshow using Intel Quick Sync..."
ffmpeg $OVERWRITE_OUTPUT -loglevel info -stats \
  -f concat -safe 0 -auto_convert 1 -i "$INPUTS_TXT" \
  "${FFMPEG_AUDIO_ARGS[@]}" \
  -filter_complex "$AUDIO_FILTER" \
  -map "0:v" -map "[audio_out]" \
  -vf "format=nv12" \
  -fps_mode cfr -t "$expected_video_duration" \
  -c:v hevc_qsv -preset slow -global_quality 20 "$OUTPUT_FILE"

FFMPEG_EXIT_CODE=$?

if [ $FFMPEG_EXIT_CODE -eq 0 ]; then
    echo "Process complete! File output successfully located at: $OUTPUT_FILE"
else
    echo "Error: Video compilation failed inside FFmpeg." >&2
    exit $FFMPEG_EXIT_CODE
fi
