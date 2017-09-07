all:	objs libexiv2_pixbuf_loader.so libexiv2_ratings.so 
install:	all
	cp libexiv2_pixbuf_loader.so /usr/lib64/gdk-pixbuf-2.0/2.10.0/loaders/
	gdk-pixbuf-query-loaders-64 > /usr/lib64/gdk-pixbuf-2.0/2.10.0/loaders.cache
	cp libexiv2_ratings.so eog-plugin/exiv2_ratings.plugin /usr/lib64/eog/plugins/

# DEBUG_FLAGS = -DNDEBUG
DEBUG_FLAGS = -g
CXXFLAGS += -Wconversion-null $(DEBUG_FLAGS)


TEST_BINS = gpixbuf gpixbufldr xmp leak mag

objs:		exiv2_pixbuf_loader.o eog_plugin_exiv2_ratings.o
clean:
	rm -f $(TEST_BINS) *.o *.so

tests:		$(TEST_BINS)

gpixbuf:	tests/gpixbuf.c
	gcc $(shell pkg-config gdk-pixbuf-2.0 --cflags) $(shell pkg-config gdk-pixbuf-2.0 --libs) $^ -o $@

gpixbufldr:	tests/gpixbufldr.c
	gcc $(shell pkg-config gdk-pixbuf-2.0 --cflags) $(shell pkg-config gtk+-3.0 --cflags) $(shell pkg-config gdk-pixbuf-2.0 --libs) $(shell pkg-config gtk+-3.0 --libs) $^ -o $@


exiv2_pixbuf_loader.o:	gdk-pixbuf/exiv2_pixbuf_loader.cc
	g++ $(CXXFLAGS) -c -fPIC  $(shell pkg-config gdk-pixbuf-2.0 --cflags) $(shell pkg-config exiv2 --cflags) $(shell pkg-config Magick++ --cflags) $^

libexiv2_pixbuf_loader.so:	exiv2_pixbuf_loader.o
	g++ -shared $(shell pkg-config gdk-pixbuf-2.0 --libs) $(shell pkg-config exiv2 --libs) $(shell pkg-config Magick++ --libs) $^ -o $@


xmp:	tests/xmp.cc
	g++ $(CXXFLAGS) $(shell pkg-config exiv2 --cflags --libs) $^ -o $@


eog_plugin_exiv2_ratings.o:	eog-plugin/eog_plugin_exiv2_ratings.cc 
	g++ $(CXXFLAGS) -c -fPIC -fpermissive $(shell pkg-config eog --cflags) $(pkg-config libpeas-1.0 --cflags) $(shell pkg-config exiv2 --cflags)  $(shell pkg-config libpeas-gtk-1.0 --cflags) $^

libexiv2_ratings.so:	eog_plugin_exiv2_ratings.o
	g++ -shared -g $(shell pkg-config eog --libs) $(shell pkg-config exiv2 --libs) $^ -o $@

leak:	tests/leak.cc
	g++ -g  $(shell pkg-config exiv2 --cflags) $(shell pkg-config Magick++ --cflags) $^ $(shell pkg-config exiv2 --libs) $(shell pkg-config Magick++ --libs) -o $@

mag:	tests/mag.cc
	g++ -g  $(shell pkg-config Magick++ --cflags) $^  $(shell pkg-config Magick++ --libs) -o $@
