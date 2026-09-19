AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I m4

# Core directories built unconditionally
SUBDIRS = gdk-pixbuf gdk-pixbuf/data

if HAVE_EOG
SUBDIRS += lib eog-plugin
endif

if HAVE_EOM
if !HAVE_EOG
SUBDIRS += lib
endif
SUBDIRS += eom-plugin eom-reload-size-plugin eom-imgprextr-plugin
endif

