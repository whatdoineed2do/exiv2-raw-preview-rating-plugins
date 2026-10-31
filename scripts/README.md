# Scripts Directory

This directory contains utility scripts supporting RAW image previews, metadata handling, and file rating plugins for image viewers such as Eye of Mate (`eom`) and Eye of Gnome (`eog`).

## Script Summaries

| Script | Description |
| :--- | :--- |
| **`crop169.sh`** | Takes input image and crops it ready for generating 1080HD video; can crop prioritising the top or bottom of the image|
| **`centreav.sh`** | Takes input video and audio file and 'centres' the audio for the video |
| **`exif-annotate.sh`** | Annotates input image file(s) with embedded EXIF on the output; various params to handle text (annotation) overlay, resizing and output quality |
| **`imgh265.sh`** | Automated slideshow video generation pipeline wrapping FFmpeg, ImageMagick, and ExifTool to convert image collections into H.265 MP4 videos with intelligent layout handling |
| **`nef-exif-sync.sh`** | Syncs EXIF data from a source directory with matching files in a target directory |
