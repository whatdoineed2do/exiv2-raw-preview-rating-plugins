#!/bin/bash

# Check if correct arguments are provided
if [ "$#" -ne 3 ]; then
    echo "Usage: $0 <input_video> <input_audio> <output_video>"
    exit 1
fi

VIDEO_IN="$1"
AUDIO_IN="$2"
VIDEO_OUT="$3"

# 1. Get durations using ffprobe
V_DUR=$(ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 "$VIDEO_IN")
A_DUR=$(ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 "$AUDIO_IN")

# 2. Calculate delay in milliseconds using bc (rounds to closest integer)
DELAY=$(echo "($V_DUR - $A_DUR) * 1000 / 2" | bc -l | awk '{print int($1)}')

# Check if audio is longer than video
if [ "$DELAY" -lt 0 ]; then
    echo "Error: Audio file is longer than the video file."
    exit 1
fi

echo "Calculated Delay: ${DELAY}ms"

# 3. Run FFmpeg command (Stream-copies video, encodes audio to AAC)
ffmpeg -i "$VIDEO_IN" -i "$AUDIO_IN" \
  -filter_complex "[1:a]adelay=${DELAY}|${DELAY}[delayed];[delayed]apad[padded]" \
  -map 0:v -map "[padded]" \
  -c:v copy \
  -c:a aac \
  -shortest \
  "$VIDEO_OUT"
