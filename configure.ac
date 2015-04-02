
AC_INIT(gst-plugins-tizen, 1.0)
 
dnl versions of gstreamer and plugins-base
GST_MAJORMINOR=1.0
GST_REQUIRED=1.2.0
GSTPB_REQUIRED=1.2.0
EFL_REQUIRED=1.0.0

dnl fill in your package name and version here
dnl the fourth (nano) number should be 0 for a release, 1 for CVS,
dnl and 2... for a prerelease

dnl when going to/from release please set the nano correctly !
dnl releases only do Wall, cvs and prerelease does Werror too
AS_VERSION(gst-plugin, GST_PLUGIN_VERSION, 0, 10, 0, 1,
    GST_PLUGIN_CVS="no", GST_PLUGIN_CVS="yes")

dnl AM_MAINTAINER_MODE provides the option to enable maintainer mode
AM_MAINTAINER_MODE

#AM_INIT_AUTOMAKE($PACKAGE, $VERSION)
AM_INIT_AUTOMAKE

#AC_CONFIG_MACRO_DIR([m4])

dnl make aclocal work in maintainer mode
dnl AC_SUBST(ACLOCAL_AMFLAGS, "-I m4")

dnl Add parameters for aclocal
AC_SUBST(ACLOCAL_AMFLAGS, "-I m4 -I common/m4")

AM_CONFIG_HEADER(config.h)

dnl check for tools
AC_PROG_CC
AC_PROG_CXX
AC_PROG_LIBTOOL

dnl decide on error flags
AS_COMPILER_FLAG(-Wall, GST_WALL="yes", GST_WALL="no")
                                                                                
if test "x$GST_WALL" = "xyes"; then
   GST_ERROR="$GST_ERROR -Wall"
                                                                                
#   if test "x$GST_PLUGIN_CVS" = "xyes"; then
#     AS_COMPILER_FLAG(-Werror,GST_ERROR="$GST_ERROR -Werror",GST_ERROR="$GST_ERROR")
#   fi
fi

dnl Check for pkgconfig first
AC_CHECK_PROG(HAVE_PKGCONFIG, pkg-config, yes, no)

dnl Give error and exit if we don't have pkgconfig
if test "x$HAVE_PKGCONFIG" = "xno"; then
  AC_MSG_ERROR(you need to have pkgconfig installed !)
fi

dnl Now we're ready to ask for gstreamer libs and cflags
dnl And we can also ask for the right version of gstreamer
AM_CONDITIONAL([IS_VODA_SDK], [test "x$DISTRO" = "xvodafone-sdk"])
AM_CONDITIONAL([ISPROTECTOR_VODA_SDK], [test "x$DISTRO" = "xvodafone-sdk" && test "x$MACHINE" = "xprotector"])
AM_CONDITIONAL([ISVOLANS_VODA_SDK], [test "x$DISTRO" = "xvodafone-sdk" && test "x$MACHINE" = "xvolans"])
AM_CONDITIONAL([ISPROTECTOR_TARGET], [test "x$ARCH" = "xarm" && test "x$MACHINE" = "xprotector"])
AM_CONDITIONAL([ISVOLANS_TARGET], [test "x$ARCH" = "xarm" && test "x$MACHINE" = "xvolans"])

PKG_CHECK_MODULES(GST, \
  gstreamer-$GST_MAJORMINOR >= $GST_REQUIRED,
  HAVE_GST=yes,HAVE_GST=no)

dnl Give error and exit if we don't have gstreamer
if test "x$HAVE_GST" = "xno"; then
  AC_MSG_ERROR(you need gstreamer development packages installed !)
fi

dnl append GST_ERROR cflags to GST_CFLAGS
GST_CFLAGS="$GST_CFLAGS $GST_ERROR"

dnl make GST_CFLAGS and GST_LIBS available
PKG_CHECK_MODULES(GST, gstreamer-1.0 >= 1.2.0)
AC_SUBST(GST_CFLAGS)
AC_SUBST(GST_LIBS)

dnl make GST_MAJORMINOR available in Makefile.am
AC_SUBST(GST_MAJORMINOR)

dnl If we need them, we can also use the base class libraries
PKG_CHECK_MODULES(GST_BASE, gstreamer-base-$GST_MAJORMINOR >= $GST_REQUIRED,
                  HAVE_GST_BASE=yes, HAVE_GST_BASE=no)

dnl Give a warning if we don't have gstreamer libs
dnl you can turn this into an error if you need them
if test "x$HAVE_GST_BASE" = "xno"; then
  AC_MSG_NOTICE(no GStreamer base class libraries found (gstreamer-base-$GST_MAJORMINOR))
fi

dnl make _CFLAGS and _LIBS available
AC_SUBST(GST_BASE_CFLAGS)
AC_SUBST(GST_BASE_LIBS)

dnl If we need them, we can also use the gstreamer-plugins-base libraries
PKG_CHECK_MODULES(GSTPB_BASE,
                  gstreamer-plugins-base-$GST_MAJORMINOR >= $GSTPB_REQUIRED,
                  HAVE_GSTPB_BASE=yes, HAVE_GSTPB_BASE=no)

dnl Give a warning if we don't have gstreamer libs
dnl you can turn this into an error if you need them
if test "x$HAVE_GSTPB_BASE" = "xno"; then
  AC_MSG_NOTICE(no GStreamer Plugins Base libraries found (gstreamer-plugins-base-$GST_MAJORMINOR))
fi

dnl make _CFLAGS and _LIBS available
AC_SUBST(GSTPB_BASE_CFLAGS)
AC_SUBST(GSTPB_BASE_LIBS)

dnl If we need them, we can also use the gstreamer-controller libraries
PKG_CHECK_MODULES(GSTCTRL,
                  gstreamer-controller-$GST_MAJORMINOR >= $GSTPB_REQUIRED,
                  HAVE_GSTCTRL=yes, HAVE_GSTCTRL=no)

dnl Give a warning if we don't have gstreamer-controller
dnl you can turn this into an error if you need them
if test "x$HAVE_GSTCTRL" = "xno"; then
  AC_MSG_NOTICE(no GStreamer Controller libraries found (gstreamer-controller-$GST_MAJORMINOR))
fi

dnl make _CFLAGS and _LIBS available
AC_SUBST(GSTCTRL_CFLAGS)
AC_SUBST(GSTCTRL_LIBS)

dnl set the plugindir where plugins should be installed
if test "x${prefix}" = "x$HOME"; then
  plugindir="$HOME/.gstreamer-$GST_MAJORMINOR/plugins"
else
  plugindir="\$(libdir)/gstreamer-$GST_MAJORMINOR"
fi
AC_SUBST(plugindir)

dnl set proper LDFLAGS for plugins
#GST_PLUGIN_LDFLAGS='-module -avoid-version -export-symbols-regex [_]*\(gst_\|Gst\|GST_\).*'
GST_PLUGIN_LDFLAGS='-module -avoid-version -export-symbols-regex [_]*\(gst_\|Gst\|GST_\|FIR_\|arkamys_malloc\|arkamys_free\).*'
#GST_PLUGIN_LDFLAGS='-module -avoid-version'
AC_SUBST(GST_PLUGIN_LDFLAGS)


PKG_CHECK_MODULES(GST_AUDIO, gstreamer-audio-$GST_MAJORMINOR >= $GST_REQUIRED )


dnl make _CFLAGS and _LIBS available
AC_SUBST(GST_AUDIO_CFLAGS)
AC_SUBST(GST_AUDIO_LIBS)

PKG_CHECK_MODULES(GST_VIDEO, gstreamer-video-$GST_MAJORMINOR >= $GST_REQUIRED)

dnl make _CFLAGS and _LIBS available
AC_SUBST(GST_VIDEO_CFLAGS)
AC_SUBST(GST_VIDEO_LIBS)

PKG_CHECK_MODULES(DRM, libdrm)
AC_SUBST(DRM_CFLAGS)
AC_SUBST(DRM_LIBS)

PKG_CHECK_MODULES(DRM_EXYNOS, libdrm_exynos)
AC_SUBST(DRM_EXYNOS_CFLAGS)
AC_SUBST(DRM_EXYNOS_LIBS)

PKG_CHECK_MODULES(TBM, libtbm)
AC_SUBST(TBM_CFLAGS)
AC_SUBST(TBM_LIBS)

dnl required package for evasimagesink/evaspixmapsink
PKG_CHECK_MODULES(EFL, [
  evas >= $EFL_REQUIRED
  ecore >= $EFL_REQUIRED
], [
  AC_SUBST(EFL_CFLAGS)
  AC_SUBST(EFL_LIBS)
], [
  AC_MSG_ERROR([
      You need to install or upgrade the EFL development
      packages on your system. On debian-based systems these are
      libevas-dev and libecore-dev.
      The minimum version required is $EFL_REQUIRED.
  ])
])

dnl *** belows are related to evaspixmapsink plug-ins ***
AG_GST_ARG_WITH_PACKAGE_NAME
AG_GST_ARG_WITH_PACKAGE_ORIGIN

dnl set license and copyright notice
GST_LICENSE="LGPL"
AC_DEFINE_UNQUOTED(GST_LICENSE, "$GST_LICENSE", [GStreamer license])
AC_SUBST(GST_LICENSE)

echo
AC_MSG_NOTICE([Checking libraries for evaspixmapsink plugin])
echo
dnl *** X11 ***
translit(dnm, m, l) AM_CONDITIONAL(USE_X, true)
AG_GST_CHECK_FEATURE(X, [X libraries and plugins],
                  [evaspixmapsink], [
  AC_PATH_XTRA
  ac_cflags_save="$CFLAGS"
  ac_cppflags_save="$CPPFLAGS"
  CFLAGS="$CFLAGS $X_CFLAGS"
  CPPFLAGS="$CPPFLAGS $X_CFLAGS"

  dnl now try to find the HEADER
  AC_CHECK_HEADER(X11/Xlib.h, HAVE_X="yes", HAVE_X="no")

  if test "x$HAVE_X" = "xno"
  then
    AC_MSG_NOTICE([cannot find X11 development files])
  else
    dnl this is much more than we want
    X_LIBS="$X_LIBS $X_PRE_LIBS $X_EXTRA_LIBS"
    dnl AC_PATH_XTRA only defines the path needed to find the X libs,
    dnl it does not add the libs; therefore we add them here
    X_LIBS="$X_LIBS -lX11"
    AC_SUBST(X_CFLAGS)
    AC_SUBST(X_LIBS)

    dnl xvimagesrc _CFLAGS and _LIB available

    PKG_CHECK_MODULES(DRI2, libdri2)
    AC_SUBST(DRI2_CFLAGS)
    AC_SUBST(DRI2_LIBS)

    PKG_CHECK_MODULES(X11, x11)
    AC_SUBST(X11_CFLAGS)
    AC_SUBST(X11_LIBS)

    PKG_CHECK_MODULES(XEXT, xext)
    AC_SUBST(XEXT_CFLAGS)
    AC_SUBST(XEXT_LIBS)

    PKG_CHECK_MODULES(XV, xv)
    AC_SUBST(XV_CFLAGS)
    AC_SUBST(XV_LIBS)

    PKG_CHECK_MODULES(XDAMAGE, xdamage)
    AC_SUBST(XDAMAGE_CFLAGS)
    AC_SUBST(XDAMAGE_LIBS)

    PKG_CHECK_MODULES(ECORE_X, ecore-x)
    AC_SUBST(ECORE_X_CFLAGS)
    AC_SUBST(ECORE_X_LIBS)

  fi
  AC_SUBST(HAVE_X)
  CFLAGS="$ac_cflags_save"
  CPPFLAGS="$ac_cppflags_save"
])

dnl Check for Xv extension
translit(dnm, m, l) AM_CONDITIONAL(USE_XVIDEO, true)
AG_GST_CHECK_FEATURE(XVIDEO, [X11 XVideo extensions],
                  [evaspixmapsink], [
AG_GST_CHECK_XV
])

dnl check for X Shm
translit(dnm, m, l) AM_CONDITIONAL(USE_XSHM, true)
AG_GST_CHECK_FEATURE(XSHM, [X Shared Memory extension], , [
  if test x$HAVE_X = xyes; then
    AC_CHECK_LIB(Xext, XShmAttach,
		 HAVE_XSHM="yes", HAVE_XSHM="no",
		 $X_LIBS)
    if test "x$HAVE_XSHM" = "xyes"; then
      XSHM_LIBS="-lXext"
    else
      dnl On AIX, it is in XextSam instead, but we still need -lXext
      AC_CHECK_LIB(XextSam, XShmAttach,
		   HAVE_XSHM="yes", HAVE_XSHM="no",
		   $X_LIBS)
      if test "x$HAVE_XSHM" = "xyes"; then
	XSHM_LIBS="-lXext -lXextSam"
      fi
    fi
  fi
], , [
  AC_SUBST(HAVE_XSHM)
  AC_SUBST(XSHM_LIBS)
])

dnl PKG_CHECK_MODULES(UDEVMGR, unified-dev-mgr)
dnl AC_SUBST(UDEVMGR_CFLAGS)
dnl AC_SUBST(UDEVMGR_LIBS)

dnl use encodebin --------------------------------------------------------------------------
AC_ARG_ENABLE(encodebin, AC_HELP_STRING([--enable-encodebin], [using encodebin]),
  [
    case "${enableval}" in
      yes) GST_TIZEN_USE_ENCODEBIN=yes ;;
      no)  GST_TIZEN_USE_ENCODEBIN=no ;;
      *)   AC_MSG_ERROR(bad value ${enableval} for --enable-encodebin) ;;
    esac
  ],
  [GST_TIZEN_USE_ENCODEBIN=yes])
AM_CONDITIONAL(GST_TIZEN_USE_ENCODEBIN, test "x$GST_TIZEN_USE_ENCODEBIN" = "xyes")

dnl use evasimagesink --------------------------------------------------------------------------
AC_ARG_ENABLE(evasimagesink, AC_HELP_STRING([--enable-evasimagesink], [using evasimagesink]),
  [
    case "${enableval}" in
      yes) GST_TIZEN_USE_EVASIMAGESINK=yes ;;
      no)  GST_TIZEN_USE_EVASIMAGESINK=no ;;
      *)   AC_MSG_ERROR(bad value ${enableval} for --enable-evasimagesink) ;;
    esac
  ],
  [GST_TIZEN_USE_EVASIMAGESINK=yes])
AM_CONDITIONAL(GST_TIZEN_USE_EVASIMAGESINK, test "x$GST_TIZEN_USE_EVASIMAGESINK" = "xyes")

dnl use evaspixmapsink ---------------------------------------------------------------------------
AC_ARG_ENABLE(evaspixmapsink, AC_HELP_STRING([--enable-evaspixmapsink], [using evaspixmapsink]),
  [
    case "${enableval}" in
      yes) GST_TIZEN_USE_EVASPIXMAPSINK=yes ;;
      no)  GST_TIZEN_USE_EVASPIXMAPSINK=no ;;
      *)   AC_MSG_ERROR(bad value ${enableval} for --enable-evaspixmapsink) ;;
    esac
  ],
  [GST_TIZEN_USE_EVASPIXMAPSINK=yes])
AM_CONDITIONAL(GST_TIZEN_USE_EVASPIXMAPSINK, test "x$GST_TIZEN_USE_EVASPIXMAPSINK" = "xyes")

dnl use xvimagesrc--------------------------------------------------------------------------
AC_ARG_ENABLE(xvimagesrc, AC_HELP_STRING([--enable-xvimagesrc], [using xvimagesrc]),
  [
    case "${enableval}" in
      yes) GST_TIZEN_USE_XVIMAGESRC=yes ;;
      no)  GST_TIZEN_USE_XVIMAGESRC=no ;;
      *)   AC_MSG_ERROR(bad value ${enableval} for --enable-xvimagesrc) ;;
    esac
  ],
  [GST_TIZEN_USE_XVIMAGESRC=yes])
AM_CONDITIONAL(GST_TIZEN_USE_XVIMAGESRC, test "x$GST_TIZEN_USE_XVIMAGESRC" = "xyes")

dnl use drmsrc --------------------------------------------------------------------------
AC_ARG_ENABLE(drmsrc, AC_HELP_STRING([--enable-drmsrc], [using drmsrc]),
  [
    case "${enableval}" in
      yes) GST_TIZEN_USE_DRMSRC=yes ;;
      no)  GST_TIZEN_USE_DRMSRC=no ;;
      *)   AC_MSG_ERROR(bad value ${enableval} for --enable-drmsrc) ;;
    esac
  ],
  [GST_TIZEN_USE_DRMSRC=yes])
AM_CONDITIONAL(GST_TIZEN_USE_DRMSRC, test "x$GST_TIZEN_USE_DRMSRC" = "xyes")

dnl use toggle --------------------------------------------------------------------------
AC_ARG_ENABLE(toggle, AC_HELP_STRING([--enable-toggle], [using toggle]),
  [
    case "${enableval}" in
      yes) GST_TIZEN_USE_TOGGLE=yes ;;
      no)  GST_TIZEN_USE_TOGGLE=no ;;
      *)   AC_MSG_ERROR(bad value ${enableval} for --enable-toggle) ;;
    esac
  ],
  [GST_TIZEN_USE_TOGGLE=yes])
AM_CONDITIONAL(GST_TIZEN_USE_TOGGLE, test "x$GST_TIZEN_USE_TOGGLE" = "xyes")

dnl for i386 --------------------------------------------------------------------------
AC_ARG_ENABLE(i386, AC_HELP_STRING([--enable-i386], [i386 build]),
	[
	 case "${enableval}" in
	  yes) IS_I386=yes ;;
	  no)  IS_I386=no ;;
	  *)   AC_MSG_ERROR(bad value ${enableval} for --enable-i386) ;;
	 esac
	],
	[IS_I386=no])
AM_CONDITIONAL([IS_I386], [test "x$IS_I386" = "xyes"])

dnl use pdpushsrc --------------------------------------------------------------------------
AC_ARG_ENABLE(pdpushsrc, AC_HELP_STRING([--enable-pdpushsrc], [using pdpushsrc]),
[
 case "${enableval}" in
	 yes) GST_TIZEN_USE_PD_PUSHSRC=yes ;;
	 no)  GST_TIZEN_USE_PD_PUSHSRC=no ;;
	 *)   AC_MSG_ERROR(bad value ${enableval} for --enable-pdpushsrc) ;;
 esac
 ],
 [GST_TIZEN_USE_PD_PUSHSRC=yes])
AM_CONDITIONAL(GST_TIZEN_USE_PD_PUSHSRC, test "x$GST_TIZEN_USE_PD_PUSHSRC" = "xyes")

dnl use audiotp --------------------------------------------------------------------------
AC_ARG_ENABLE(audiotp, AC_HELP_STRING([--enable-audiotp], [using audiotp]),
[
 case "${enableval}" in
	 yes) GST_TIZEN_USE_AUDIOTP=yes ;;
	 no)  GST_TIZEN_USE_AUDIOTP=no ;;
	 *)   AC_MSG_ERROR(bad value ${enableval} for --enable-audiotp) ;;
 esac
 ],
 [GST_TIZEN_USE_AUDIOTP=yes])
AM_CONDITIONAL(GST_TIZEN_USE_AUDIOTP, test "x$GST_TIZEN_USE_AUDIOTP" = "xyes")

dnl use audioeq --------------------------------------------------------------------------
AC_ARG_ENABLE(audioeq, AC_HELP_STRING([--enable-audioeq], [using audioeq]),
[
 case "${enableval}" in
         yes) GST_TIZEN_USE_AUDIOEQ=yes ;;
         no)  GST_TIZEN_USE_AUDIOEQ=no ;;
         *)   AC_MSG_ERROR(bad value ${enableval} for --enable-audioeq) ;;
 esac
 ],
 [GST_TIZEN_USE_AUDIOEQ=yes])
AM_CONDITIONAL(GST_TIZEN_USE_AUDIOEQ, test "x$GST_TIZEN_USE_AUDIOEQ" = "xyes")

dnl use fimcconvert ----------------------------------------------------------------------
AC_ARG_ENABLE(fimcconvert, AC_HELP_STRING([--enable-fimcconvert], [using fimcconvert]),
[
 case "${enableval}" in
         yes) GST_TIZEN_USE_FIMCCONVERT=yes ;;
         no)  GST_TIZEN_USE_FIMCCONVERT=no ;;
         *)   AC_MSG_ERROR(bad value ${enableval} for --enable-fimcconvert) ;;
 esac
 ],
 [GST_TIZEN_USE_FIMCCONVERT=yes])
AM_CONDITIONAL(GST_TIZEN_USE_FIMCCONVERT, test "x$GST_TIZEN_USE_FIMCCONVERT" = "xyes")

dnl use wfdrtspsrc --------------------------------------------------------------------------
AC_ARG_ENABLE(wfdrtspsrc, AC_HELP_STRING([--enable-wfdrtspsrc], [using wfdrtspsrc]),
[
 case "${enableval}" in
  yes) GST_TIZEN_USE_WFDRTSPSRC=yes ;;
  no)  GST_TIZEN_USE_WFDRTSPSRC=no ;;
  *)   AC_MSG_ERROR(bad value ${enableval} for --enable-wfdrtspsrc) ;;
 esac
 ],
 [GST_TIZEN_USE_WFDRTSPSRC=yes])
AM_CONDITIONAL(GST_TIZEN_USE_WFDRTSPSRC, test "x$GST_TIZEN_USE_WFDRTSPSRC" = "xyes")

if [test "x$GST_TIZEN_USE_WFDRTSPSRC" = "xyes"];
then
PKG_CHECK_MODULES(MM_WFD_COMMON, mm-scmirroring-common)
AC_SUBST(MM_WFD_COMMON_CFLAGS)
AC_SUBST(MM_WFD_COMMON_LIBS)
fi

AC_OUTPUT(
Makefile
common/Makefile
common/m4/Makefile
pdpushsrc/Makefile
pdpushsrc/src/Makefile
encodebin/Makefile
encodebin/src/Makefile
evasimagesink/Makefile
evasimagesink/src/Makefile
evaspixmapsink/Makefile
xvimagesrc/Makefile
xvimagesrc/src/Makefile
toggle/Makefile
toggle/src/Makefile
drmsrc/Makefile
drmsrc/src/Makefile
audiotp/Makefile
audiotp/src/Makefile
audioeq/Makefile
audioeq/src/Makefile
fimcconvert/Makefile
fimcconvert/src/Makefile
wfdrtspsrc/Makefile
wfdrtspsrc/src/Makefile
)

