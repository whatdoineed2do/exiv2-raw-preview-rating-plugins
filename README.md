# `gdk pixbuf` Viewer for _RAW_ preview images and Gnome EOG/EOM EXIF rating plugins

This project provides 3x plugins:
* a `gdk pixbuf loader` that handles RAW files (primarily Nikon NEF, Canon CR2 and DNGs) that loads the largest embedded preview image available (via `exiv2`) - the [RAW file support is dependant on `exiv2`](https://dev.exiv2.org/projects/exiv2/wiki/Supported_image_formats).  The largest embedded preview image is dependant on the underlying device but the user can choose to scale for easier handling by `gsettings set org.gtk.gdk-pixbuf.exiv2-rawpreview scale-limit 1632`.  Set `G_MESSAGES_DEBUG=gdk-pixbuf.exiv2-rawpreview` to examine values as `pixbuf` runs.

The available controls, as seen via `for i in $(gsettings list-keys org.gtk.gdk-pixbuf.exiv2-rawpreview); do echo "'$i'  $(gsettings describe org.gtk.gdk-pixbuf.exiv2-rawpreview $i)"; done`:
  * `scale-limit` - scaling of preview image to display
  * `convert-srgb` - attempt to convert colourspace to sRGB
  * `auto-orientate` - disable/enable correct image orientation; you may want this to be _false_ and allow image viewers to auto orientate to avoid double rotation
  * `annotation-font` - font for overlayed EXIF (names as recognised by `ImageMagick`, see: `convert -list font`)
  * `annotation-percent-height` - size of overlayed EXIF details based on height (set to 0 to disable and to use explicit `font-size`)
  * `annotation-font-size - font size for EXIF
* 2x Linux desktop image viewer plugins that can set/unset EXIF/XMP rating via the `R` keybinding - relies on `exiv2` for supported EXIF images:
  * [Eye of Gnome](https://wiki.gnome.org/Apps/EyeOfGnome) (`eog`) 
  * [Eye of Mate](https://wiki.mate-desktop.org/mate-desktop/applications/eom/) (`eom`) 

## Intended Usage
For use when reviewing and making _first cut_ selections from RAW files from within a Linux graphical environment: a precursor ahead of editting your RAW files where a faster lightweight workflow is required (no need for a VM with CaptureNX or Lightroom etc or native Linux RawTherapee etc).

The _scale-limit_ is meant to help optimise needless scaling of images: if you have a 36 megapixel RAW image it may have a small number of embedded preview images of varying and increasing sizes, which is what Nikon cameras tend to do: a Nikon RAW file may embed preview images of 570x, 1632x and full (36mp) 7360x. If your screen is only 1600 pixels wide, there may be no point in using the 36mp preview image that is then scaled to fit screen.  In such an situation where you are working with a known size of embedded preview image sizes, you can set _scale-limit_ to the desired size to avoid potential needless image scaling for display.

## Setting/Unsetting EXIF/XMP Rating
Using `eog` or `eom`, open any files and use `r` key to toggle rating on the current image file - the rating is only saved when moving away from the current image;  if you toggle multiple times that leaves the file in the original _rated_ state, no rating update is written to the file.

Ratings are represented in the `XMP Rating` tag with a value of `5`.  Use `exiv2 -px foo.NEF` to validate rating flag is set on the file.

Current rating is displayed on the bottom right of the statusbar.

## Dependancies
Uses `gdk-pixbuf`, `exiv2` and `Image Magick`.  By default the build scripts will try to build all plugins if development dependenacies are satisfied.
