# plugindir is set in configure

##############################################################################
# change libgstplugin.la to something more suitable, e.g. libmysomething.la  #
##############################################################################
plugin_LTLIBRARIES = libgstwaylandsrc.la

##############################################################################
# for the next set of variables, rename the prefix if you renamed the .la,   #
#  e.g. libgstplugin_la_SOURCES => libmysomething_la_SOURCES                 #
#       libgstplugin_la_CFLAGS  => libmysomething_la_CFLAGS                  #
#       libgstplugin_la_LIBADD  => libmysomething_la_LIBADD                  #
#       libgstplugin_la_LDFLAGS => libmysomething_la_LDFLAGS                 #
##############################################################################

# sources used to compile this plug-in
libgstwaylandsrc_la_SOURCES = gstwaylandsrc.c

# flags used to compile this plugin
# add other _CFLAGS and _LIBS as needed
libgstwaylandsrc_la_CFLAGS = $(GST_CFLAGS) $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(WAYLAND_CLIENT_CFLAGS) $(DRM_CFLAGS) $(TBM_CFLAGS) $(MMCOMMON_CFLAGS)
libgstwaylandsrc_la_LIBADD = $(GST_LIBS) $(GST_PLUGINS_BASE_LIBS) $(GST_BASE_LIBS) $(WAYLAND_CLIENT_LIBS) $(DRM_LIBS) $(TBM_LIBS)
libgstwaylandsrc_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)

# headers we need but don't want installed
noinst_HEADERS = gstwaylandsrc.h
