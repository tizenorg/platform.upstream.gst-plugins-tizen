plugin_LTLIBRARIES = libgsttizenwlsink.la

libgsttizenwlsink_la_SOURCES =  \
	gstwaylandsink.c \
	waylandpool.c \
	wldisplay.c \
	wlwindow.c \
	wlvideoformat.c \
	scaler-protocol.c \
	tizen-wlvideoformat.c

libgsttizenwlsink_la_CFLAGS = $(GST_CFLAGS) $(GST_PLUGINS_BASE_CFLAGS) \
                               $(WAYLAND_CFLAGS) $(GST_PLUGINS_BAD_CFLAGS) $(DRM_CFLAGS) $(TBM_CFLAGS) $(MMCOMMON_CFLAGS) \
							   $(GST_WAYLAND_CFLAGS)
libgsttizenwlsink_la_LIBADD = \
	$(GST_PLUGINS_BASE_LIBS) \
	-lgstvideo-$(GST_MAJORMINOR) \
	$(WAYLAND_LIBS) $(DRM_LIBS) $(TBM_LIBS) $(GST_WAYLAND_LIBS)
libgsttizenwlsink_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgsttizenwlsink_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS)

noinst_HEADERS = \
	gstwaylandsink.h \
	waylandpool.h \
	wldisplay.h \
	wlwindow.h \
	wlvideoformat.h \
	scaler-client-protocol.h \
	tizen-wlvideoformat.h

EXTRA_DIST = scaler.xml
CLEANFILES = scaler-protocol.c scaler-client-protocol.h

%-protocol.c : %.xml
	$(wayland_scanner) code < $< > $@

%-client-protocol.h : %.xml
	$(wayland_scanner) client-header < $< > $@

gstwaylandsink.c: scaler-client-protocol.h

waylandpool.c: scaler-client-protocol.h

wldisplay.c: scaler-client-protocol.h

wlwindow.c: scaler-client-protocol.h
