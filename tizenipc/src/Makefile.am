# plugindir is set in configure

##############################################################################
# change libgstplugin.la to something more suitable, e.g. libmysomething.la  #
##############################################################################
plugin_LTLIBRARIES = libgsttizenipcsink.la libgsttizenipcsrc.la

##############################################################################
# for the next set of variables, rename the prefix if you renamed the .la,   #
#  e.g. libgstplugin_la_SOURCES => libmysomething_la_SOURCES                 #
#       libgstplugin_la_CFLAGS  => libmysomething_la_CFLAGS                  #
#       libgstplugin_la_LIBADD  => libmysomething_la_LIBADD                  #
#       libgstplugin_la_LDFLAGS => libmysomething_la_LDFLAGS                 #
##############################################################################

# sources used to compile this plug-in
libgsttizenipcsink_la_SOURCES = gsttizenipcsink.c

# flags used to compile this plugin
# add other _CFLAGS and _LIBS as needed
libgsttizenipcsink_la_CFLAGS = $(GST_CFLAGS) $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(TBM_CFLAGS) $(MMCOMMON_CFLAGS)
libgsttizenipcsink_la_LIBADD = $(GST_LIBS) $(GST_PLUGINS_BASE_LIBS) $(GST_BASE_LIBS) $(TBM_LIBS)
libgsttizenipcsink_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)

# sources used to compile this plug-in
libgsttizenipcsrc_la_SOURCES = gsttizenipcsrc.c

# flags used to compile this plugin
# add other _CFLAGS and _LIBS as needed
libgsttizenipcsrc_la_CFLAGS = $(GST_CFLAGS) $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(TBM_CFLAGS) $(MMCOMMON_CFLAGS)
libgsttizenipcsrc_la_LIBADD = $(GST_LIBS) $(GST_PLUGINS_BASE_LIBS) $(GST_BASE_LIBS) $(TBM_LIBS)
libgsttizenipcsrc_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)

# headers we need but don't want installed
noinst_HEADERS = gsttizenipcsink.h \
                 gsttizenipcsrc.h

