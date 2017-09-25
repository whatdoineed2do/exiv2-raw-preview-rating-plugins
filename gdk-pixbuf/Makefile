%.o	: %.cc  ;	$(CXX) -c $(CXXFLAGS) $<
%.o	: %.c   ;	$(CC)  -c $(CFLAGS) $<


all:	objs libexiv2_pixbuf_loader.so
install:	all
	cp libexiv2_pixbuf_loader.so /usr/lib64/gdk-pixbuf-2.0/2.10.0/loaders/
	gdk-pixbuf-query-loaders-64 > /usr/lib64/gdk-pixbuf-2.0/2.10.0/loaders.cache

CXXFLAGS += -fpermissive -Wconversion-null -Wno-write-strings $(DEBUG_FLAGS) -fPIC  $(shell pkg-config gdk-pixbuf-2.0 --cflags) $(shell pkg-config exiv2 --cflags) $(shell pkg-config Magick++ --cflags)


OBJS = ImgFactory.o DbgHlpr.o exiv2_pixbuf_loader.o
TEST_BINS = leak

objs:	$(OBJS)
clean:
	rm -f $(TEST_BINS) *.o *.so

tests:		$(TEST_BINS)

DbgHlpr.o:	DbgHlpr.cc DbgHlpr.h
ImgFactory.o:	ImgFactory.cc ImgFactory.h DbgHlpr.h
exiv2_pixbuf_loader.o:	exiv2_pixbuf_loader.cc ImgFactory.h DbgHlpr.h

libexiv2_pixbuf_loader.so:	$(OBJS)
	g++ -shared $(shell pkg-config gdk-pixbuf-2.0 --libs) $(shell pkg-config exiv2 --libs) $(shell pkg-config Magick++ --libs) $^ -o $@


leak:	leak.cc ImgFactory.h DbgHlpr.h
	g++ -ggdb  $(shell pkg-config exiv2 --cflags) $(shell pkg-config Magick++ --cflags) $^ $(shell pkg-config exiv2 --libs) $(shell pkg-config Magick++ --libs) ImgFactory.o DbgHlpr.o -o $@

