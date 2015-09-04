SUBDIRS = wfdbase

plugin_LTLIBRARIES = libgstwfdmanager.la

noinst_HEADERS = wfdrtpbuffer/wfdrtpbuffer.h \
		wfdrtpbuffer/gstwfdrtpbuffer.h \
		wfdrtpbuffer/rtpstats.h \
		wfdsrc/gstwfdsrc.h

libgstwfdmanager_la_SOURCES = gstwfdmanager.c \
			wfdsrc/gstwfdsrc.c \
			wfdrtpbuffer/wfdrtpbuffer.c \
			wfdrtpbuffer/gstwfdrtpbuffer.c \
			wfdrtpbuffer/rtpstats.c

libgstwfdmanager_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_CFLAGS)

libgstwfdmanager_la_LIBADD = $(GST_LIBS) $(GST_PLUGINS_BASE_LIBS) $(GST_BASE_LIBS) \
		-lgstrtp-@GST_API_VERSION@ -lgstrtsp-@GST_API_VERSION@ \
		$(top_builddir)/wfdmanager/wfdbase/libgstwfdbase.la

libgstwfdmanager_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstwfdmanager_la_LIBTOOLFLAGS = --tag=disable-static

