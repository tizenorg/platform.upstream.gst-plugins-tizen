Name:       gst-plugins-tizen1.0
Version:    0.1.0
Summary:    GStreamer tizen plugins (common)
Release:    2
Group:      Multimedia/Framework
Url:        http://gstreamer.freedesktop.org/
License:    LGPL-2.0+
Source0:    %{name}-%{version}.tar.gz
%define gst_branch 1.0

#BuildRequires:  pkgconfig(camsrcjpegenc)
BuildRequires:  pkgconfig(ecore)
BuildRequires:  pkgconfig(ecore-x)
BuildRequires:  pkgconfig(evas)
BuildRequires:  pkgconfig(gstreamer-audio-1.0)
BuildRequires:  pkgconfig(gstreamer-video-1.0)
BuildRequires:  pkgconfig(gstreamer-plugins-base-1.0)
BuildRequires:  pkgconfig(gstreamer-1.0)  
BuildRequires:  pkgconfig(libexif)
BuildRequires:	pkgconfig(libdri2)
BuildRequires:	pkgconfig(x11)
BuildRequires:	pkgconfig(xext)
BuildRequires:	pkgconfig(xv)
BuildRequires:	pkgconfig(xdamage)
BuildRequires:	pkgconfig(libdrm)
BuildRequires:  pkgconfig(libtbm)
BuildRequires:	libdrm-devel
BuildRequires:	pkgconfig(dri2proto)
BuildRequires:	pkgconfig(xfixes)
BuildRequires:  pkgconfig(vconf)
#BuildRequires:  pkgconfig(mm-scmirroring-common)

%description
GStreamer tizen plugins (common)

%prep
%setup -q


%build
export CFLAGS+=" -DGST_EXT_TIME_ANALYSIS -DGST_EXT_XV_ENHANCEMENT -DEXPORT_API=\"__attribute__((visibility(\\\"default\\\")))\" "

./autogen.sh --disable-static
%configure --disable-static

make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install


%files
%manifest gst-plugins-tizen1.0.manifest
%defattr(-,root,root,-)  
%{_libdir}/gstreamer-%{gst_branch}/*.so
