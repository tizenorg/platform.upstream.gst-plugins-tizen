plugin_LTLIBRARIES = libgstwfdtsdemux.la

libgstwfdtsdemux_la_SOURCES = \
	gstwfdh264parser.c \
	wfdnalutils.c \
	gstwfdtssection.c \
	gstwfdtsdescriptor.c \
	wfdtspacketizer.c \
	wfdtsbase.c	\
	wfdtsparse.c \
	wfdtsdemux.c	\
	gstwfdtsdemux.c \
	wfdpesparse.c

libgstwfdtsdemux_la_CFLAGS = \
	$(GST_PLUGINS_BASE_CFLAGS) \
	$(GST_BASE_CFLAGS) $(GST_CFLAGS) \
	-DWFD_SPEC
libgstwfdtsdemux_la_LIBADD = \
	$(GST_PLUGINS_BASE_LIBS) \
	-lgsttag-@GST_MAJORMINOR@ \
	-lgstpbutils-@GST_MAJORMINOR@ \
	$(GST_BASE_LIBS) \
	$(GST_LIBS)
libgstwfdtsdemux_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstwfdtsdemux_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS)

noinst_HEADERS = \
	gstwfdh264parser.h \
	gstwfdts-private.h \
	wfdnalutils.h \
	gstwfdtssection.h \
	gstwfdtsdescriptor.h \
	gstwfdtsdefs.h   \
	gstwfdtsdesc.h   \
	wfdtsbase.h	\
	wfdtspacketizer.h \
	wfdtsparse.h \
	wfdtsdemux.h	\
	wfdpesparse.h


