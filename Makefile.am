
ACLOCAL_AMFLAGS = -I common/m4

aclocaldir = $(datadir)/aclocal

SUBDIRS = common

if GST_TIZEN_USE_ENCODEBIN
SUBDIRS += encodebin
endif

if GST_TIZEN_USE_EVASIMAGESINK
SUBDIRS += evasimagesink
endif

#if GST_TIZEN_USE_EVASPIXMAPSINK
#SUBDIRS += evaspixmapsink
#endif

if GST_TIZEN_USE_XVIMAGESRC
SUBDIRS += xvimagesrc
endif

if GST_TIZEN_USE_DRMSRC
SUBDIRS += drmsrc
endif

if GST_TIZEN_USE_TOGGLE
SUBDIRS += toggle
endif

if GST_TIZEN_USE_PD_PUSHSRC
SUBDIRS += pdpushsrc
endif

if GST_TIZEN_USE_AUDIOTP
SUBDIRS += audiotp
endif

if GST_TIZEN_USE_AUDIOEQ
SUBDIRS += audioeq
endif

#if GST_TIZEN_USE_FIMCCONVERT
#SUBDIRS += fimcconvert
#endif

#if GST_TIZEN_USE_WFDRTSPSRC
#SUBDIRS += wfdrtspsrc
#endif

DIST_SUBDIRS = common

if GST_TIZEN_USE_ENCODEBIN
DIST_SUBDIRS += encodebin
endif

if GST_TIZEN_USE_EVASIMAGESINK
DIST_SUBDIRS += evasimagesink
endif

if GST_TIZEN_USE_XVIMAGESRC
DIST_SUBDIRS += xvimagesrc
endif

if GST_TIZEN_USE_DRMSRC
DIST_SUBDIRS += drmsrc
endif

if GST_TIZEN_USE_TOGGLE
DIST_SUBDIRS += toggle
endif

if GST_TIZEN_USE_FIMCCONVERT
DIST_SUBDIRS += fimcconvert
endif


#if GST_TIZEN_USE_WFDRTSPSRC
#DIST_SUBDIRS += wfdrtspsrc
#endif

EXTRA_DIST = \
	gstreamer.spec gstreamer.spec.in \
	configure.ac autogen.sh depcomp \
	MAINTAINERS ABOUT-NLS RELEASE gstreamer.doap \
	$(win32)

check: check-exports

