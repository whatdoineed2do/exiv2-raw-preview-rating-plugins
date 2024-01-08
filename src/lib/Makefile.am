ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS}

AM_CXXFLAGS = $(EXIV_CFLAGS) $(PEAS_CFLAGS) -fPIC -I$(top_srcdir)
AM_LDFLAGS = $(EXIV_LIBS)

noinst_LIBRARIES = libexiv2-ratings-utils.a

libexiv2_ratings_utils_a_CPPFLAGS = $(AM_CPPFLAGS)
libexiv2_ratings_utils_a_CFLAGS = $(AM_CXXFLAGS)
libexiv2_ratings_utils_a_SOURCES = ExifProxy.cc DbgHlpr.cc
