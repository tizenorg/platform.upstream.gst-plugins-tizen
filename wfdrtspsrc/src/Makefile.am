plugin_LTLIBRARIES = libgstwfdrtspsrc.la

libgstwfdrtspsrc_la_SOURCES = gstwfdrtspsrc.c gstwfdrtspext.c
libgstwfdrtspsrc_la_SOURCES += wfdrtspmanager.c

libgstwfdrtspsrc_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_CFLAGS) $(VCONF_FLAGS)

libgstwfdrtspsrc_la_LIBADD = $(GST_LIBS) $(GST_PLUGINS_BASE_LIBS) $(GST_BASE_LIBS) \
               $(MM_WFD_COMMON_LIBS) \
               -lgstrtp-@GST_MAJORMINOR@ -lgstrtsp-@GST_MAJORMINOR@ \
               $(WIN32_LIBS) \
               $(VCONF_LIBS)

libgstwfdrtspsrc_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstwfdrtspsrc_la_LIBTOOLFLAGS = --tag=disable-static

noinst_HEADERS = gstwfdrtspsrc.h \
                 gstwfdrtspext.h
noinst_HEADERS += wfdrtspmanager.h

plugin_LTLIBRARIES += libgstwfdrtprequester.la

libgstwfdrtprequester_la_SOURCES = gstwfdrtprequester.c

libgstwfdrtprequester_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_CFLAGS) $(VCONF_FLAGS) $(MM_WFD_COMMON_CFLAGS)
libgstwfdrtprequester_la_LIBADD = $(GST_LIBS) $(GST_PLUGINS_BASE_LIBS) $(GST_BASE_LIBS) \
               $(MM_WFD_COMMON_LIBS) \
               -lgstrtp-@GST_MAJORMINOR@ -lgstrtsp-@GST_MAJORMINOR@ \
               $(WIN32_LIBS) \
               $(VCONF_LIBS)
libgstwfdrtprequester_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstwfdrtprequester_la_LIBTOOLFLAGS = --tag=disable-static

noinst_HEADERS = gstwfdrtprequester.h

