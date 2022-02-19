ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS}

AM_CXXFLAGS = $(EXIV_CFLAGS) $(GDKPIXBUF_CFLAGS) $(EOG_CFLAGS) $(PEAS_CFLAGS)
AM_LDFLAGS = $(EXIV_LIBS) $(GDKPIXBUF_LIBS) $(EOG_LIBS) $(PEAS_LIBS) 

GIT_CODE_DATE=$(VERSION)

lib_LTLIBRARIES = libexiv2_ratings.la

libexiv2_ratings_la_CPPFLAGS = $(AM_CPPFLAGS)
libexiv2_ratings_la_CXXFLAGS = $(AM_CXXFLAGS)
libexiv2_ratings_la_LDFLAGS = $(AM_LDFLAGS) -avoid-version -module $(no_undefined)
libexiv2_ratings_la_SOURCES = \
    eog_plugin_exiv2_ratings.cc

exiv2_ratings.plugin:      exiv2_ratings.tmpl ../../config.h
	sed -e "s/REPLACE_ME_VERSION/$(VERSION)/g" -e "s/REPLACE_ME_DATE/$(GIT_CODE_DATE)/g" exiv2_ratings.tmpl > exiv2_ratings.plugin

install:	all exiv2_ratings.plugin
	$(INSTALL)         .libs/libexiv2_ratings.so $(EOG_PLUGINS_DIR)/
	$(INSTALL) -m 0644 exiv2_ratings.plugin      $(EOG_PLUGINS_DIR)/
