%bcond_with wayland
%bcond_with x
%define gst_branch 1.0

Name:       gst-plugins-tizen
Version:    1.0.0
Summary:    GStreamer tizen plugins (common)
Release:    24
Group:      Multimedia/Framework
Url:        http://gstreamer.freedesktop.org/
License:    LGPL-2.1+
Source0:    %{name}-%{version}.tar.gz

#BuildRequires:  pkgconfig(camsrcjpegenc)
BuildRequires:  pkgconfig(ecore)
BuildRequires:  pkgconfig(evas)
BuildRequires:  pkgconfig(gstreamer-audio-1.0)
BuildRequires:  pkgconfig(gstreamer-video-1.0)
BuildRequires:  pkgconfig(gstreamer-plugins-base-1.0)
BuildRequires:  pkgconfig(gstreamer-1.0)
BuildRequires:  pkgconfig(libexif)
%if %{with x}
BuildRequires:  pkgconfig(ecore-x)
BuildRequires:	pkgconfig(libdri2)
BuildRequires:	pkgconfig(x11)
BuildRequires:	pkgconfig(xext)
BuildRequires:	pkgconfig(xv)
BuildRequires:	pkgconfig(xdamage)
BuildRequires:	pkgconfig(xfixes)
BuildRequires:	pkgconfig(dri2proto)
%endif
BuildRequires:	pkgconfig(libdrm)
BuildRequires:	pkgconfig(libdrm_exynos)
BuildRequires:  pkgconfig(libtbm)
BuildRequires:	libdrm-devel
BuildRequires:  pkgconfig(vconf)
BuildRequires:  pkgconfig(mm-common)
%if %{with wayland}
BuildRequires:  pkgconfig(wayland-client) >= 1.0.0
BuildRequires:  pkgconfig(wayland-tbm-client)
BuildRequires:  pkgconfig(tizen-extension-client)
BuildRequires:  pkgconfig(gstreamer-wayland-1.0)
%endif

%description
GStreamer tizen plugins (common)

%prep
%setup -q


%build
export CFLAGS+=" -DGST_EXT_TIME_ANALYSIS -DGST_EXT_XV_ENHANCEMENT -DGST_WLSINK_ENHANCEMENT -DEXPORT_API=\"__attribute__((visibility(\\\"default\\\")))\" "

./autogen.sh --disable-static
%configure \
	--disable-drmdecryptor\
	--disable-static

make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install
mkdir -p %{buildroot}/%{_datadir}/license
cp -rf %{_builddir}/%{name}-%{version}/COPYING %{buildroot}%{_datadir}/license/%{name}


%files
%manifest gst-plugins-tizen1.0.manifest
%defattr(-,root,root,-)
%{_libdir}/gstreamer-%{gst_branch}/*.so
%{_libdir}/libgstwfdbase.so*
%{_datadir}/license/%{name}
