ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS}

AM_CXXFLAGS = $(IMAGEMAGICK_CFLAGS) $(EXIV_CFLAGS)
AM_LDFLAGS = $(IMAGEMAGICK_LIBS) $(EXIV_LIBS)


lib_LTLIBRARIES = libpixbufloader-exiv2-rawpreview.la

libpixbufloader_exiv2_rawpreview_la_CPPFLAGS = $(AM_CPPFLAGS) -DNDEBUG
libpixbufloader_exiv2_rawpreview_la_CXXFLAGS = $(AM_CXXFLAGS) $(GDKPIXBUF_CFLAGS)
libpixbufloader_exiv2_rawpreview_la_LDFLAGS = $(AM_LDFLAGS) $(GDKPIXBUF_LIBS) -avoid-version -module $(no_undefined)
libpixbufloader_exiv2_rawpreview_la_SOURCES = \
	version.cc \
	DbgHlpr.cc \
	ColorProfiles.cc \
	Env.cc \
	ImgXfrm.cc \
	ImgFactory.cc \
	exiv2_pixbuf_loader.cc 

install:	all
	$(INSTALL) .libs/libpixbufloader-exiv2-rawpreview.so $(GDKPIXBUF_MODULES_DIR)/
	gdk-pixbuf-query-loaders-64 > $(GDKPIXBUF_LOADER_CACHE)

#g++ -ggdb  -fopenmp -pthread $(shell pkg-config exiv2 --cflags) $(shell pkg-config Magick++ --cflags) $^ $(shell pkg-config exiv2 --libs) $(shell pkg-config Magick++ --libs) -lpthread -o $@

