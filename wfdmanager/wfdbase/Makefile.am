lib_LTLIBRARIES = libgstwfdbase.la

#libgstwfdbase_includedir =		\
#	$(includedir)/gstreamer-@GST_API_VERSION@/gst/wfd

#libgstwfdbase_include_HEADERS =	\
#	gstwfdbasesrc.h

noinst_HEADERS = gstwfdbasesrc.h wfdrtspconfigmessage.h wfdrtspmacro.h

libgstwfdbase_la_SOURCES = gstwfdbasesrc.c

libgstwfdbase_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_CFLAGS)

libgstwfdbase_la_LIBADD = $(GST_LIBS) $(GST_PLUGINS_BASE_LIBS) $(GST_BASE_LIBS) \
               -lgstrtp-@GST_API_VERSION@ -lgstrtsp-@GST_API_VERSION@

libgstwfdbase_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstwfdbase_la_LIBTOOLFLAGS = --tag=disable-static

