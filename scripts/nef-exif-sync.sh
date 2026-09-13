#!/bin/bash

# Ensure both arguments are provided
if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <source_nef_dir> <target_jpg_dir>"
    exit 1
fi

SOURCE_DIR="$1"
TARGET_DIR="$2"

# Check if exiftool is installed
if ! command -v exiftool &> /dev/null; then
    echo "Error: exiftool is not installed or not in your PATH."
    exit 1
fi

# Check if source directory exists
if [ ! -d "$SOURCE_DIR" ]; then
    echo "Error: Source directory does not exist: $SOURCE_DIR"
    exit 1
fi

# Check if target directory exists
if [ ! -d "$TARGET_DIR" ]; then
    echo "Error: Target directory does not exist: $TARGET_DIR"
    exit 1
fi

# Print status and run ExifTool
echo "Syncing metadata from NEF to JPG..."
echo "Source: $SOURCE_DIR"
echo "Target: $TARGET_DIR"
echo "--------------------------------------------------"

exiftool \
    -ext jpg \
    -tagsfromfile "${SOURCE_DIR}/%f.NEF" \
    -all:all \
    --previewimage \
    --thumbnailimage \
    -overwrite_original \
    "$TARGET_DIR"

echo "--------------------------------------------------"
echo "Metadata sync complete!"

