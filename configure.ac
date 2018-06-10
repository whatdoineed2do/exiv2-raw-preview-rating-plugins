AC_PREREQ(2.64)
AC_INIT([eog-plugin-exif-rating-raw-viewer], [x.x.x], [https://github.com/whatdoineed2do/eog-plugin-exif-rating-raw-viewer/issues], [https://github.com/whatdoineed2do/eog-plugin-exif-rating-raw-viewer], [whatdoineed2do@nospam-github.com])
AM_INIT_AUTOMAKE([dist-bzip2 no-dist-gzip])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES])
AC_CONFIG_SRCDIR([src/])
AC_CONFIG_HEADERS([config.h])

AC_PROG_CC
AC_PROG_CXX
AC_HEADER_STDC
AC_PROG_INSTALL
PKG_PROG_PKG_CONFIG

PKG_CHECK_MODULES(EOG,         [eog             >= 3.24.1])
PKG_CHECK_MODULES(IMAGEMAGICK, [Magick++        >= 6.9.9])
PKG_CHECK_MODULES(EXIV,        [exiv2           >= 0.25])
PKG_CHECK_MODULES(GDKPIXBUF,   [gdk-pixbuf-2.0  >= 2.36.9])

AC_ARG_ENABLE([debug-code],
	[AS_HELP_STRING([--enable-debug],
	[enable debug (default is no)])],
	[no_debug=false],
	[no_debug=true])
if test "$no_debug" = true; then
        building_debug=no
        AC_DEFINE(NDEBUG,1,[debug code])
else
        building_debug=yes
        AC_DEFINE(DEBUG,1,[debug code])
fi


AC_OUTPUT([
Makefile
eog-plugin/Makefile
gdk-pixbuf/Makefile
])
