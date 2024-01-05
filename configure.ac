AC_PREREQ(2.64)

AC_INIT([exiv2-raw-preview-rating-plugins],
	[v1.4.2.1],
	[https://github.com/whatdoineed2do/exiv2-raw-preview-rating-plugins/issues],
	[exiv2-raw-preview-rating-plugins],
	[https://github.com/whatdoineed2do/exiv2-raw-preview-rating-plugins])

AM_INIT_AUTOMAKE([dist-bzip2 no-dist-gzip])
AC_CONFIG_MACRO_DIRS([m4])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AC_CONFIG_SRCDIR([src])
AC_CONFIG_HEADERS([config.h])

AC_PROG_CC
AC_PROG_CXX
AM_PROG_CC_C_O
AC_HEADER_STDC
AC_PROG_INSTALL
AC_PROG_LIBTOOL
PKG_PROG_PKG_CONFIG

AM_CPPFLAGS="-Wall"
AC_SUBST([AM_CPPFLAGS])

GLIB_GSETTINGS

AC_ARG_ENABLE(eog,
        AS_HELP_STRING([--disable-eog],[do not build EOG exif rating plugin]),
        [ use_eog="${enableval}" ], [ use_eog=yes ] )

if test "x$use_eog" != xno ; then
        PKG_CHECK_MODULES(EOG, [eog >= 3.24.1], [ use_eog=yes ], [ use_eog=no ])
        AH_TEMPLATE([HAVE_EOG], [build EOG exif rating plugin])
        AS_IF([test "x$use_eog" = "xyes"], [
                AC_DEFINE([HAVE_EOG])
                AC_SUBST(EOG_CFLAGS)
                AC_SUBST(EOG_LIBS)
		PKG_CHECK_VAR([EOG_PLUGINS_DIR],[eog],[pluginsdir])
		need_peas=yes
           ])
fi
AM_CONDITIONAL([HAVE_EOG], [test "x$use_eog" = xyes])


AC_ARG_ENABLE(eom,
        AS_HELP_STRING([--disable-eom],[do not build Eye Of Mate exif rating plugin]),
        [ use_eom="${enableval}" ], [ use_eom=yes ] )

if test "x$use_eom" != xno ; then
        PKG_CHECK_MODULES(EOM, [eom >= 1.26.0], [ use_eom=yes ], [ use_eom=no ])
        AH_TEMPLATE([HAVE_EOM], [build Eye Of Mate exif rating plugin])
        AS_IF([test "x$use_eom" = "xyes"], [
                AC_DEFINE([HAVE_EOM])
                AC_SUBST(EOM_CFLAGS)
                AC_SUBST(EOM_LIBS)
		PKG_CHECK_VAR([EOM_PLUGINS_DIR],[eom],[pluginsdir])
		need_peas=yes
           ])
fi
AM_CONDITIONAL([HAVE_EOM], [test "x$use_eom" = xyes])


AC_ARG_ENABLE(gtk,
        AS_HELP_STRING([--disable-gtk],[do not build tests with GTK]),
        [ use_gtk="${enableval}" ], [ use_gtk=yes ] )

if test "x$use_gtk" != xno ; then
        PKG_CHECK_MODULES(GTK, [gtk+-3.0], [ use_gtk=yes ], [ use_gtk=no ])
        AH_TEMPLATE([HAVE_GTK], [build test with GTK ])
        AS_IF([test "x$use_gtk" = "xyes"], [
                AC_DEFINE([HAVE_GTK])
                AC_SUBST(GTK_CFLAGS)
                AC_SUBST(GTK_LIBS)
           ])
fi
AM_CONDITIONAL([HAVE_GTK], [test "x$use_gtk" = xyes])


if test "x$need_peas" != xno ; then
PKG_CHECK_MODULES(PEAS,        [libpeas-1.0     >= 1.20.0])
fi


PKG_CHECK_MODULES(IMAGEMAGICK, [Magick++        >= 6.9.9])
PKG_CHECK_MODULES(EXIV,        [exiv2           >= 0.25])

PKG_CHECK_MODULES(GDKPIXBUF,   [gdk-pixbuf-2.0  >= 2.36.9])
PKG_CHECK_VAR([GDKPIXBUF_MODULES_DIR],[gdk-pixbuf-2.0],[gdk_pixbuf_moduledir])
PKG_CHECK_VAR([GDKPIXBUF_LOADER_CACHE],[gdk-pixbuf-2.0],[gdk_pixbuf_cache_file])

AC_ARG_ENABLE(debug,
        AS_HELP_STRING([--enable-debug],[enable debug code (default is no)]),
        [ debug="${enableval}" ], [ debug=no ] )
if test "x$debug" = xno; then
	AC_DEFINE(NDEBUG,1,[debug code])
fi


if test "x$use_eog" = "xno" ; then
    sed -i "s/eog-plugin//" src/Makefile.am
fi

if test "x$use_eom" = "xno" ; then
    sed -i "s/eom-plugin//" src/Makefile.am
fi


if test "x$need_peas" != "xyes" ; then
    sed -i "s/lib//" src/Makefile.am
fi

AC_OUTPUT([
    Makefile
    src/Makefile
    src/lib/Makefile
    src/eog-plugin/Makefile
    src/eom-plugin/Makefile
    src/gdk-pixbuf/Makefile
    src/gdk-pixbuf/data/Makefile
    tests/Makefile
])
