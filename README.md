# `gdk pixbuf` Viewer for _RAW_ preview images and Gnome EOG/EOM EXIF rating plugins

This project provides 3x plugins:
* a `gdk pixbuf loader` that handles RAW files (primarily Nikon NEF, Canon CR2 and DNGs) that loads the largest embedded preview image available (via `exiv2`) - the [RAW file support is dependant on `exiv2`](https://dev.exiv2.org/projects/exiv2/wiki/Supported_image_formats).  The largest embedded preview image is dependant on the underlying device but the user can choose to scale for easier handling by setting the environment variable.  The avaiable controls:
  * `EXIV2_PIXBUF_LOADER_SCALE_LIMIT` - scaling of preview image to display
  * `EXIV2_PIXBUF_LOADER_CONVERT_SRGB` - attempt to convert colourspace to sRGB
  * `EXIV2_PIXBUF_LOADER_ROTATE` - disable/enable correct image orientation
  * `EXIV2_PIXBUF_LOADER_FONT` - font for overlayed EXIF
* 2x Linux desktop image viewer plugins that can set/unset EXIF/XMP rating via the `R` keybinding - relies on `exiv2` for supported EXIF images:
  * [Eye of Gnome](https://wiki.gnome.org/Apps/EyeOfGnome) (`eog`) 
  * [Eye of Mate](https://wiki.mate-desktop.org/mate-desktop/applications/eom/) (`eom`) 

## Intended Usage
For use when reviewing and making _first cut_ selections from RAW files from within a Linux graphical environment: a precursor ahead of editting your RAW files where a faster lightweight workflow is required (no need for a VM with CaptureNX or Lightroom etc or native Linux RawTherapee etc).

## Setting/Unsetting EXIF/XMP Rating
Using `eog` or `eom`, open any files and use `r` key to toggle rating on the current image file - the rating is only saved when moving away from the current image;  if you toggle multiple times that leaves the file in the original _rated_ state, no rating update is written to the file.

Ratings are represented in the `XMP Rating` tag with a value of `5`.  Use `exiv2 -px foo.NEF` to validate rating flag is set on the file.

Current rating is displayed on the bottom right of the statusbar.

## Dependancies
Uses `gdk-pixbuf`, `exiv2` and `Image Magick`.  By default the build scripts will try to build all plugins if development dependenacies are satisfied.
