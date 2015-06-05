plugin_LTLIBRARIES = libgstwfdmanager.la

libgstwfdmanager_la_SOURCES = gstwfdmanager.c \
			gstwfdrtspsrc.c \
			gstwfdrtprequester.c

libgstwfdmanager_la_SOURCES += wfdrtspmanager.c \
			wfdrtpbuffer/wfdrtpbuffer.c \
			wfdrtpbuffer/gstwfdrtpbuffer.c \
			wfdrtpbuffer/rtpstats.c

libgstwfdmanager_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_CFLAGS)

libgstwfdmanager_la_LIBADD = $(GST_LIBS) $(GST_PLUGINS_BASE_LIBS) $(GST_BASE_LIBS) \
               -lgstrtp-@GST_MAJORMINOR@ -lgstrtsp-@GST_MAJORMINOR@ \
               $(WIN32_LIBS)

libgstwfdmanager_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstwfdmanager_la_LIBTOOLFLAGS = --tag=disable-static

noinst_HEADERS = gstwfdrtspsrc.h \
		gstwfdrtprequester.h

noinst_HEADERS += wfdrtspmanager.h \
		wfdrtpbuffer/wfdrtpbuffer.h \
		wfdrtpbuffer/gstwfdrtpbuffer.h \
		wfdrtpbuffer/rtpstats.h
