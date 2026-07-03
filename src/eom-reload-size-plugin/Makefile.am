ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS}

AM_CXXFLAGS = $(EXIV_CFLAGS) $(GDKPIXBUF_CFLAGS) $(EOM_CFLAGS) $(PEAS_CFLAGS) -I../lib
AM_LDFLAGS = -L../lib

GIT_CODE_DATE = $(VERSION)

plugindir = $(EOM_PLUGINS_DIR)
plugin_LTLIBRARIES = libexiv2-pixbuf-size-reload.la

libexiv2_pixbuf_size_reload_la_CPPFLAGS = $(AM_CPPFLAGS)
libexiv2_pixbuf_size_reload_la_CXXFLAGS = $(AM_CXXFLAGS)
libexiv2_pixbuf_size_reload_la_LDFLAGS = $(AM_LDFLAGS) -avoid-version -module $(no_undefined)

libexiv2_pixbuf_size_reload_la_LIBADD = $(EXIV_LIBS) $(GDKPIXBUF_LIBS) $(EOM_LIBS) $(PEAS_LIBS)

libexiv2_pixbuf_size_reload_la_SOURCES = \
    eom-pixbuf-size-reload.cc

plugin_DATA = eom-pixbuf-size-reload.plugin
