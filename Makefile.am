
ACLOCAL_AMFLAGS = -I common/m4

aclocaldir = $(datadir)/aclocal

SUBDIRS = common

if GST_EXT_USE_EXT_ENCODEBIN
SUBDIRS += encodebin
endif

if GST_EXT_USE_EXT_EVASIMAGESINK
SUBDIRS += evasimagesink
endif

#if GST_EXT_USE_EXT_EVASPIXMAPSINK
#SUBDIRS += evaspixmapsink
#endif

#if GST_EXT_USE_EXT_XVIMAGESRC
#SUBDIRS += xvimagesrc
#endif

if GST_EXT_USE_EXT_DRMSRC
SUBDIRS += drmsrc
endif

if GST_EXT_USE_EXT_TOGGLE
SUBDIRS += toggle
endif

if GST_EXT_USE_EXT_PD_PUSHSRC
SUBDIRS += pdpushsrc
endif

if GST_EXT_USE_EXT_AUDIOTP
SUBDIRS += audiotp
endif

if GST_EXT_USE_EXT_AUDIOEQ
SUBDIRS += audioeq
endif

#if GST_EXT_USE_EXT_FIMCCONVERT
#SUBDIRS += fimcconvert
#endif

#if GST_EXT_USE_EXT_WFDRTSPSRC
#SUBDIRS += wfdrtspsrc
#endif

DIST_SUBDIRS = common

if GST_EXT_USE_EXT_ENCODEBIN
DIST_SUBDIRS += encodebin
endif

if GST_EXT_USE_EXT_EVASIMAGESINK
DIST_SUBDIRS += evasimagesink
endif

if GST_EXT_USE_EXT_XVIMAGESRC
DIST_SUBDIRS += xvimagesrc
endif

if GST_EXT_USE_EXT_DRMSRC
DIST_SUBDIRS += drmsrc
endif

if GST_EXT_USE_EXT_TOGGLE
DIST_SUBDIRS += toggle
endif

if GST_EXT_USE_EXT_FIMCCONVERT
DIST_SUBDIRS += fimcconvert
endif


#if GST_EXT_USE_EXT_WFDRTSPSRC
#DIST_SUBDIRS += wfdrtspsrc
#endif

EXTRA_DIST = \
	gstreamer.spec gstreamer.spec.in \
	configure.ac autogen.sh depcomp \
	MAINTAINERS ABOUT-NLS RELEASE gstreamer.doap \
	$(win32)

check: check-exports

