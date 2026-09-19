ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS}

AM_CXXFLAGS = $(EXIV_CFLAGS) $(GDKPIXBUF_CFLAGS) $(EOG_CFLAGS) $(PEAS_CFLAGS) $(IMAGEMAGICK_CFLAGS) -I$(top_srcdir)/src/lib
AM_LDFLAGS = $(EXIV_LIBS) $(GDKPIXBUF_LIBS) $(EOG_LIBS) $(PEAS_LIBS) $(IMAGEMAGICK_LIBS) -L../lib

GIT_CODE_DATE=$(VERSION)

# Define the plugin library target
plugin_LTLIBRARIES = libexiv2-rating.la
plugindir = $(EOG_PLUGINS_DIR)

libexiv2_rating_la_CPPFLAGS = $(AM_CPPFLAGS)
libexiv2_rating_la_CXXFLAGS = $(AM_CXXFLAGS)
libexiv2_rating_la_LDFLAGS = $(AM_LDFLAGS) -avoid-version -module -shared -lexiv2-ratings-utils
libexiv2_rating_la_SOURCES = \
    eog-exiv2-rating.cc

# Define the metadata plugin file target
plugin_DATA = exiv2-rating.plugin

exiv2-rating.plugin: exiv2-rating.tmpl $(top_builddir)/config.h
	$(AM_V_GEN)sed -e "s/REPLACE_ME_VERSION/$(VERSION)/g" -e "s/REPLACE_ME_DATE/$(GIT_CODE_DATE)/g" $(srcdir)/exiv2-rating.tmpl > $@

# Automatically clean generated plugin file
CLEANFILES = exiv2-rating.plugin

