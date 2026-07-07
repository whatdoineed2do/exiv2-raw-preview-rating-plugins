ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS}

AM_CXXFLAGS = $(EXIV_CFLAGS) $(GDKPIXBUF_CFLAGS) $(EOM_CFLAGS) $(PEAS_CFLAGS) -I../lib
AM_LDFLAGS = -L../lib

GIT_CODE_DATE = $(VERSION)

# Target location where eom searches for dynamic extensions
plugindir = $(EOM_PLUGINS_DIR)

plugin_LTLIBRARIES = libimgprextr-plugin.la

# FIX: Changed prefixes to match the new libimgprextr-plugin.la target name
libimgprextr_plugin_la_SOURCES = \
    imgprextr-plugin.h \
    imgprextr-plugin.cc

libimgprextr_plugin_la_CPPFLAGS = \
    $(EOM_CFLAGS) \
    -I$(top_srcdir)

libimgprextr_plugin_la_LDFLAGS = \
    -am \
    -module \
    -avoid-version \
    $(EOM_LIBS)

# Install metadata file targets
# Note: Use plugin__DATA if your build tool demands explicit directory binding
plugin_DATA = imgprextr.plugin

# GSettings compilation pipeline integration
gsettings_SCHEMAS = org.mate.eom.plugins.imgprextr.gschema.xml
@GSETTINGS_RULES@

EXTRA_DIST = \
    $(plugin_DATA) \
    $(gsettings_SCHEMAS)

CLEANFILES = \
    $(gsettings_SCHEMAS:.xml=.valid)
