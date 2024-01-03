ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS}

AM_CXXFLAGS = -Wno-deprecated -fpermissive -Wconversion-null -Wno-write-strings
AM_LDFLAGS = 

EXTRA_PROGRAMS = gpixbuf gpixbufldr leak magick  magick-omp xmp

leak_CPPFLAGS = $(AM_CPPFLAGS)
leak_CXXFLAGS = $(AM_CXXFLAGS) $(GDKPIXBUF_CFLAGS) $(IMAGEMAGICK_CFLAGS) $(EXIV_CFLAGS) -I../src/gdk-pixbuf
leak_LDFLAGS = $(AM_LDFLAGS) $(GDKPIXBUF_LIBS) $(IMAGEMAGICK_LIBS) $(EXIV_LIBS)
leak_LDADD = ../src/gdk-pixbuf/.libs/libpixbufloader-exiv2-rawpreview.la
leak_SOURCES = \
    leak.cc

gpixbuf_CPPFLAGS = $(AM_CPPFLAGS)
gpixbuf_CFLAGS = $(AM_CXXFLAGS) $(GDKPIXBUF_CFLAGS)
gpixbuf_LDFLAGS = $(AM_LDFLAGS) $(GDKPIXBUF_LIBS)
gpixbuf_SOURCES = \
    gpixbuf.c 

xmp_CPPFLAGS = $(AM_CPPFLAGS)
xmp_CXXFLAGS = $(AM_CXXFLAGS) $(EXIV_CFLAGS)
xmp_LDFLAGS = $(AM_LDFLAGS) $(EXIV_LIBS)
xmp_SOURCES = \
    xmp.cc 

magick_CPPFLAGS = $(AM_CPPFLAGS)
magick_CXXFLAGS = $(AM_CXXFLAGS) $(IMAGEMAGICK_CFLAGS)
magick_LDFLAGS = $(AM_LDFLAGS) $(IMAGEMAGICK_LIBS)
magick_SOURCES = \
    magick.cc 

magick_omp_CPPFLAGS = $(AM_CPPFLAGS) -DUSE_MAGICK_PLUSPLUS
magick_omp_CXXFLAGS = $(AM_CXXFLAGS) $(IMAGEMAGICK_CFLAGS)
magick_omp_LDFLAGS = $(AM_LDFLAGS) $(IMAGEMAGICK_LIBS)
magick_omp_SOURCES = \
    magick-omp.cc 

# dirty...
gpixbufldr_CPPFLAGS = $(AM_CPPFLAGS)
gpixbufldr_CFLAGS = $(AM_CXXFLAGS) $(shell pkg-config --cflags gtk+-3.0) $(GDKPIXBUF_CFLAGS)
gpixbufldr_LDFLAGS = $(AM_LDFLAGS) $(shell pkg-config --libs gtk+-3.0) $(GDKPIXBUF_LIBS)
gpixbufldr_SOURCES = \
    gpixbufldr.c 

all:
install:

tests:	$(EXTRA_PROGRAMS)
clean:
	rm -f $(EXTRA_PROGRAMS) *.o

#g++ -ggdb  -fopenmp -pthread $(shell pkg-config exiv2 --cflags) $(shell pkg-config Magick++ --cflags) $^ $(shell pkg-config exiv2 --libs) $(shell pkg-config Magick++ --libs) -lpthread -o $@
