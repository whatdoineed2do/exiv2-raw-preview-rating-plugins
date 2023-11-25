ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS}

AM_CXXFLAGS = $(EXIV_CFLAGS) $(GDKPIXBUF_CFLAGS) $(EOG_CFLAGS) $(PEAS_CFLAGS) -I../lib
AM_LDFLAGS = $(EXIV_LIBS) $(GDKPIXBUF_LIBS) $(EOG_LIBS) $(PEAS_LIBS) -L../lib

GIT_CODE_DATE=$(VERSION)

lib_LTLIBRARIES = libexiv2-rating.la

libexiv2_rating_la_CPPFLAGS = $(AM_CPPFLAGS)
libexiv2_rating_la_CXXFLAGS = $(AM_CXXFLAGS)
libexiv2_rating_la_LDFLAGS = $(AM_LDFLAGS) -avoid-version -module $(no_undefined) -lexiv2-ratings-utils
libexiv2_rating_la_SOURCES = \
    eog-exiv2-rating.cc


exiv2-rating.plugin:      exiv2-rating.tmpl ../../config.h
	sed -e "s/REPLACE_ME_VERSION/$(VERSION)/g" -e "s/REPLACE_ME_DATE/$(GIT_CODE_DATE)/g" exiv2-rating.tmpl > exiv2-rating.plugin

install:	all exiv2_rating.plugin
	$(INSTALL)         .libs/libexiv2-rating.so $(EOG_PLUGINS_DIR)/
	$(INSTALL) -m 0644 exiv2-rating.plugin      $(EOG_PLUGINS_DIR)/
