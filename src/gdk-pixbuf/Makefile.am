ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS}

AM_CPPFLAGS = -DEXIV2_PXBUF_LDR_VERSION=$(shell git describe --long --tags --dirty --always)
AM_CXXFLAGS = $(IMAGEMAGICK_CFLAGS) $(EXIV_CFLAGS) -Wno-deprecated -fpermissive -Wconversion-null -Wno-write-strings
AM_LDFLAGS = $(IMAGEMAGICK_LIBS) $(EXIV_LIBS)


lib_LTLIBRARIES = libexiv2_pixbuf_loader.la
bin_PROGRAMS = leak

libexiv2_pixbuf_loader_la_CPPFLAGS = $(AM_CPPFLAGS)
libexiv2_pixbuf_loader_la_CXXFLAGS = $(AM_CXXFLAGS) $(GDKPIXBUF_CFLAGS)
libexiv2_pixbuf_loader_la_LDFLAGS = $(AM_LDFLAGS) $(GDKPIXBUF_LIBS) -avoid-version -module $(no_undefined)
libexiv2_pixbuf_loader_la_SOURCES = \
	version.cc \
	DbgHlpr.cc \
	ImgFactory.cc \
	exiv2_pixbuf_loader.cc 

leak_CPPFLAGS = $(AM_CPPFLAGS)
leak_CXXFLAGS = $(AM_CXXFLAGS)
leak_LDFLAGS = $(AM_LDFLAGS)
leak_LDADD =
leak_SOURCES = \
    leak.cc ImgFactory.cc DbgHlpr.cc

install:	all
	$(INSTALL) .libs/libexiv2_pixbuf_loader.so $(GDKPIXBUF_MODULES_DIR)/
	gdk-pixbuf-query-loaders-64 > $(GDKPIXBUF_LOADER_CACHE)

#g++ -ggdb  -fopenmp -pthread $(shell pkg-config exiv2 --cflags) $(shell pkg-config Magick++ --cflags) $^ $(shell pkg-config exiv2 --libs) $(shell pkg-config Magick++ --libs) -lpthread -o $@

