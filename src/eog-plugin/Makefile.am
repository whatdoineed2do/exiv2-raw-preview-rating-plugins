ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS}

AM_CPPFLAGS = -fopenmp -pthread -fpermissive -Wconversion-null -Wno-write-strings $(DEBUG_FLAGS) -fPIC
AM_CXXFLAGS = $(EXIV_CFLAGS) $(GDKPIXBUF_CFLAGS) $(EOG_CFLAGS) $(PEAS_CFLAGS) -Wno-deprecated
AM_LDFLAGS = $(EXIV_LIBS) $(GDKPIXBUF_LIBS) $(EOG_LIBS) $(PEAS_LIBS) 


lib_LTLIBRARIES = libexiv2_ratings.la

libexiv2_ratings_la_CPPFLAGS = $(AM_CPPFLAGS)
libexiv2_ratings_la_CXXFLAGS = $(AM_CXXFLAGS)
libexiv2_ratings_la_LDFLAGS = $(AM_LDFLAGS) -avoid-version -module $(no_undefined)
libexiv2_ratings_la_SOURCES = \
    eog_plugin_exiv2_ratings.cc eog_plugin_exiv2_ratings.h

install:	all
	$(INSTALL) .libs/libexiv2_ratings.so $(EOG_PLUGINS_DIR)/
	$(INSTALL) exiv2_ratings.plugin      $(EOG_PLUGINS_DIR)/
