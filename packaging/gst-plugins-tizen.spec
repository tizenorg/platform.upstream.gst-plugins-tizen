%bcond_with x
%define gst_branch 1.0

Name:       gst-plugins-tizen
Version:    1.0.0
Summary:    GStreamer tizen plugins (common)
Release:    15
Group:      Multimedia/Framework
Url:        http://gstreamer.freedesktop.org/
License:    LGPL-2.1+ and Apache-2.0
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
%else
BuildRequires:	pkgconfig(wayland-client)
BuildRequires:	pkgconfig(wayland-tbm-client)
BuildRequires:	pkgconfig(tizen-extension-client)
%endif
BuildRequires:	pkgconfig(libdrm)
BuildRequires:	pkgconfig(libdrm_exynos)
BuildRequires:  pkgconfig(libtbm)
BuildRequires:	libdrm-devel
BuildRequires:  pkgconfig(vconf)
BuildRequires:  pkgconfig(mm-common)

%description
GStreamer tizen plugins (common)

%prep
%setup -q


%build
export CFLAGS+=" -DGST_EXT_TIME_ANALYSIS -DGST_EXT_XV_ENHANCEMENT -DEXPORT_API=\"__attribute__((visibility(\\\"default\\\")))\" "

./autogen.sh --disable-static
%configure \
%if %{with x}
	--disable-waylandsrc\
%else
	--disable-xvimagesrc\
%endif
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
<<<<<<< HEAD
%{_libdir}/gstreamer-%{gst_branch}/*.so
=======
%{_libdir}/libgstwfdbase.so*
%{_libdir}/gstreamer-1.0/*.so
>>>>>>> ef7394e... [wfdmanager] Change the structure of Wi-Fi display sink's source element for making possible to be inherited
%{_datadir}/license/%{name}