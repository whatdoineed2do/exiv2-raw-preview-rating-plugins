#all:	gpixbuf gpixbufldr libexiv2_pixbuf_loader.so xmp libexiv2_ratings.so
all:	objs libexiv2_pixbuf_loader.so libexiv2_ratings.so gpixbuf gpixbufldr
install:	all
	cp libexiv2_pixbuf_loader.so /usr/lib64/gdk-pixbuf-2.0/2.10.0/loaders/
	cp libexiv2_ratings.so /usr/lib64/eog/plugins/

#CXXFLAGS += -gdwarf-2 -DNDEBUG
CXXFLAGS += -DNDEBUG

objs:		exiv2_pixbuf_loader.o eog_plugin_exiv2_ratings.o

gpixbuf:	gpixbuf.c
	gcc -g $(shell pkg-config gdk-pixbuf-2.0 --cflags) $(shell pkg-config gdk-pixbuf-2.0 --libs) $^ -o $@

gpixbufldr:	gpixbufldr.c
	gcc -g $(shell pkg-config gdk-pixbuf-2.0 --cflags) $(shell pkg-config gtk+-3.0 --cflags) $(shell pkg-config gdk-pixbuf-2.0 --libs) $(shell pkg-config gtk+-3.0 --libs) $^ -o $@


exiv2_pixbuf_loader.o:	exiv2_pixbuf_loader.cc
	g++ $(CXXFLAGS) -c -fPIC -g $(shell pkg-config gdk-pixbuf-2.0 --cflags) $(shell pkg-config exiv2 --cflags) $(shell pkg-config Magick++ --cflags) $^

libexiv2_pixbuf_loader.so:	exiv2_pixbuf_loader.o
	g++ -shared -g $(shell pkg-config gdk-pixbuf-2.0 --libs) $(shell pkg-config exiv2 --libs) $(shell pkg-config Magick++ --libs) $^ -o $@


xmp:	xmp.cc
	g++ $(CXXFLAGS) -g $(shell pkg-config exiv2 --cflags --libs) $^ -o $@


eog_plugin_exiv2_ratings.o:	eog_plugin_exiv2_ratings.cc 
	g++ $(CXXFLAGS) -c -fPIC -fpermissive -g $(shell pkg-config eog --cflags) $(shell pkg-config exiv2 --cflags)  $(shell pkg-config libpeas-gtk-1.0 --cflags) $^

libexiv2_ratings.so:	eog_plugin_exiv2_ratings.o
	g++ -shared -g $(shell pkg-config eog --libs) $(shell pkg-config exiv2 --libs) $^ -o $@
