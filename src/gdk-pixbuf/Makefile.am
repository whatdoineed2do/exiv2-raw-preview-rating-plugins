ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS}

AM_CPPFLAGS = -DEXIV2_PXBUF_LDR_VERSION=$(shell git describe --long --tags --dirty --always)
AM_CXXFLAGS = $(IMAGEMAGICK_CFLAGS) $(EXIV_CFLAGS)
AM_LDFLAGS = $(IMAGEMAGICK_LIBS) $(EXIV_LIBS)


lib_LTLIBRARIES = libexiv2_pixbuf_loader.la

libexiv2_pixbuf_loader_la_CPPFLAGS = $(AM_CPPFLAGS) -DNDEBUG
libexiv2_pixbuf_loader_la_CXXFLAGS = $(AM_CXXFLAGS) $(GDKPIXBUF_CFLAGS)
libexiv2_pixbuf_loader_la_LDFLAGS = $(AM_LDFLAGS) $(GDKPIXBUF_LIBS) -avoid-version -module $(no_undefined)
libexiv2_pixbuf_loader_la_SOURCES = \
	version.cc \
	DbgHlpr.cc \
	ImgFactory.cc \
	exiv2_pixbuf_loader.cc 

install:	all
	$(INSTALL) .libs/libexiv2_pixbuf_loader.so $(GDKPIXBUF_MODULES_DIR)/
	gdk-pixbuf-query-loaders-64 > $(GDKPIXBUF_LOADER_CACHE)

#g++ -ggdb  -fopenmp -pthread $(shell pkg-config exiv2 --cflags) $(shell pkg-config Magick++ --cflags) $^ $(shell pkg-config exiv2 --libs) $(shell pkg-config Magick++ --libs) -lpthread -o $@

