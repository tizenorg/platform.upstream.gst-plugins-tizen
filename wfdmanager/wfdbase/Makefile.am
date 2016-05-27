lib_LTLIBRARIES = libgstwfdbase.la

includelibgstwfdbasedir = $(includedir)/wfdmanager/wfdbase

includelibgstwfdbase_HEADERS =	gstwfdbasesrc.h

noinst_HEADERS = gstwfdsinkmessage.h

libgstwfdbase_la_SOURCES = gstwfdbasesrc.c \
			gstwfdsinkmessage.c

libgstwfdbase_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_CFLAGS)

libgstwfdbase_la_LIBADD = $(GST_LIBS) $(GST_PLUGINS_BASE_LIBS) $(GST_BASE_LIBS) \
               -lgstrtp-@GST_MAJORMINOR@ -lgstrtsp-@GST_MAJORMINOR@

libgstwfdbase_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstwfdbase_la_LIBTOOLFLAGS = --tag=disable-static

