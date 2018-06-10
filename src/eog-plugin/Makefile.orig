%.o	: %.cc  ;	$(CXX) -c $(CXXFLAGS) $<
%.o	: %.c   ;	$(CC)  -c $(CFLAGS) $<


all:	objs libexiv2_ratings.so 

install:	all
	cp libexiv2_ratings.so eog-plugin/exiv2_ratings.plugin /usr/lib64/eog/plugins/


objs:	eog_plugin_exiv2_ratings.o

clean:
	rm -f $(TEST_BINS) *.o *.so

tests:		$(TEST_BINS)

eog_plugin_exiv2_ratings.o:	eog_plugin_exiv2_ratings.cc 
	g++ $(CXXFLAGS) $(DEBUG_FLAGS) -c -fPIC -fpermissive $(shell pkg-config eog --cflags) $(pkg-config libpeas-1.0 --cflags) $(shell pkg-config exiv2 --cflags)  $(shell pkg-config libpeas-gtk-1.0 --cflags) $^

libexiv2_ratings.so:	eog_plugin_exiv2_ratings.o
	g++ -shared -g $(shell pkg-config eog --libs) $(shell pkg-config exiv2 --libs) $^ -o $@
