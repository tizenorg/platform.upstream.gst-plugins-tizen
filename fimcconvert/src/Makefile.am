# plugindir is set in configure

##############################################################################
# change libgstplugin.la to something more suitable, e.g. libmysomething.la  #
##############################################################################
plugin_LTLIBRARIES = libgstfimcconvert.la

##############################################################################
# for the next set of variables, rename the prefix if you renamed the .la,   #
#  e.g. libgstplugin_la_SOURCES => libmysomething_la_SOURCES                 #
#       libgstplugin_la_CFLAGS  => libmysomething_la_CFLAGS                  #
#       libgstplugin_la_LIBADD  => libmysomething_la_LIBADD                  #
#       libgstplugin_la_LDFLAGS => libmysomething_la_LDFLAGS                 #
##############################################################################

# sources used to compile this plug-in
libgstfimcconvert_la_SOURCES = gstfimcconvert.c fimc_drm_api.c

# flags used to compile this plugin
# add other _CFLAGS and _LIBS as needed
libgstfimcconvert_la_CFLAGS = $(GST_CFLAGS) $(GST_VIDEO_CFLAGS) $(DRM_CFLAGS) $(XFIXES_CFLAGS) $(DRI2PROTO_CFLAGS) $(DRI2_CFLAGS) $(TBM_CFLAGS)
libgstfimcconvert_la_LIBADD = $(GST_LIBS) $(GST_PLUGINS_BASE_LIBS) $(GST_BASE_LIBS) $(GST_VIDEO_LIBS) $(DRM_LIBS) $(XFIXES_LIBS) $(DRI2PROTO_LIBS) $(DRI2_LIBS) $(TBM_LIBS)
libgstfimcconvert_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)

# headers we need but don't want installed
noinst_HEADERS = gstfimcconvert.h fimc_drm_api.h

