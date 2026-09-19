ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS}

AM_CXXFLAGS = $(EXIV_CFLAGS) $(GDKPIXBUF_CFLAGS) $(EOM_CFLAGS) $(PEAS_CFLAGS) $(IMAGEMAGICK_CFLAGS) -I$(top_srcdir)/src/lib
AM_LDFLAGS = $(EXIV_LIBS) $(GDKPIXBUF_LIBS) $(EOM_LIBS) $(PEAS_LIBS) $(IMAGEMAGICK_LIBS) -L../lib

GIT_CODE_DATE=$(VERSION)

# Define the dynamic plugin library target using Automake standards
plugin_LTLIBRARIES = libexiv2-rating.la
plugindir = $(EOM_PLUGINS_DIR)

libexiv2_rating_la_CPPFLAGS = $(AM_CPPFLAGS)
libexiv2_rating_la_CXXFLAGS = $(AM_CXXFLAGS)
libexiv2_rating_la_LDFLAGS = $(AM_LDFLAGS) -avoid-version -module -shared -lexiv2-ratings-utils
libexiv2_rating_la_SOURCES = \
    eom-exiv2-rating.cc
