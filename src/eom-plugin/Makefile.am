ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS}

AM_CXXFLAGS = $(EXIV_CFLAGS) $(GDKPIXBUF_CFLAGS) $(EOM_CFLAGS) $(PEAS_CFLAGS)
AM_LDFLAGS = $(EXIV_LIBS) $(GDKPIXBUF_LIBS) $(EOM_LIBS) $(PEAS_LIBS) 

GIT_CODE_DATE=$(VERSION)

lib_LTLIBRARIES = libexiv2_rating.la

libexiv2_rating_la_CPPFLAGS = $(AM_CPPFLAGS)
libexiv2_rating_la_CXXFLAGS = $(AM_CXXFLAGS)
libexiv2_rating_la_LDFLAGS = $(AM_LDFLAGS) -avoid-version -module $(no_undefined)
libexiv2_rating_la_SOURCES = \
    eom-exiv2-rating.cc

install:	all 
	$(INSTALL)         .libs/libexiv2_rating.so $(EOM_PLUGINS_DIR)/
	$(INSTALL) -m 0644 exiv2_rating.plugin      $(EOM_PLUGINS_DIR)/
