%.o	: %.cc  ;	$(CXX) -c $(CXXFLAGS) $<
%.o	: %.c   ;	$(CC)  -c $(CFLAGS) $<


DEBUG_FLAGS=-DNDEBUG -O2
#DEBUG_FLAGS = -g
CXXFLAGS += -Wconversion-null -Wno-write-strings $(DEBUG_FLAGS)

all:	objs
	$(MAKE) -C gdk-pixbuf all DEBUG_FLAGS="$(DEBUG_FLAGS)"
	$(MAKE) -C eog-plugin all DEBUG_FLAGS="$(DEBUG_FLAGS)"

install:	all
	$(MAKE) -C gdk-pixbuf install
	$(MAKE) -C eog-plugin install


TEST_BINS = gpixbuf gpixbufldr xmp leak mag magick-opm-plus magick-opm-core

objs:
	$(MAKE) -C gdk-pixbuf objs DEBUG_FLAGS="$(DEBUG_FLAGS)"
	$(MAKE) -C eog-plugin objs DEBUG_FLAGS="$(DEBUG_FLAGS)"

clean:
	rm -f $(TEST_BINS) *.o *.so
	$(MAKE) -C gdk-pixbuf clean
	$(MAKE) -C eog-plugin clean


tests:		$(TEST_BINS)

gpixbuf:	tests/gpixbuf.c
	gcc $(shell pkg-config gdk-pixbuf-2.0 --cflags) $(shell pkg-config gdk-pixbuf-2.0 --libs) $^ -o $@
gpixbufldr:	tests/gpixbufldr.c
	gcc $(shell pkg-config gdk-pixbuf-2.0 --cflags) $(shell pkg-config gtk+-3.0 --cflags) $(shell pkg-config gdk-pixbuf-2.0 --libs) $(shell pkg-config gtk+-3.0 --libs) $^ -o $@
xmp:	tests/xmp.cc
	g++ $(CXXFLAGS) $(shell pkg-config exiv2 --cflags --libs) $^ -o $@
leak:	tests/leak.cc
	g++ -g  $(shell pkg-config exiv2 --cflags) $(shell pkg-config Magick++ --cflags) $^ $(shell pkg-config exiv2 --libs) $(shell pkg-config Magick++ --libs) -o $@

mag:	tests/mag.cc
	g++ -g  $(shell pkg-config Magick++ --cflags) $^  $(shell pkg-config Magick++ --libs) -o $@

magick-opm-plus:	tests/magick-opm.cc
	g++ -DUSE_MAGICK_PLUSPLUS -fopenmp -pthread -g  $(shell pkg-config MagickCore --cflags) $(shell pkg-config Magick++ --cflags) $^  $(shell pkg-config MagickCore --libs) $(shell pkg-config Magick++ --libs) -lpthread -o $@
magick-opm-core:	tests/magick-opm.cc
	g++                       -fopenmp -pthread -g  $(shell pkg-config MagickCore --cflags) $(shell pkg-config Magick++ --cflags) $^  $(shell pkg-config MagickCore --libs) $(shell pkg-config Magick++ --libs) -lpthread -o $@
