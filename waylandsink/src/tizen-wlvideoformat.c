/* GStreamer Wayland video sink
 *
 * Copyright (C) 2011 Intel Corporation
 * Copyright (C) 2011 Sreerenj Balachandran <sreerenj.balachandran@intel.com>
 * Copyright (C) 2012 Wim Taymans <wim.taymans@gmail.com>
 * Copyright (C) 2014 Collabora Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "tizen-wlvideoformat.h"
#ifdef GST_WLSINK_ENHANCEMENT

GST_DEBUG_CATEGORY_EXTERN (gsttizenwl_debug);
#define GST_CAT_DEFAULT gsttizenwl_debug

typedef struct
{
  uint32_t wl_format;
  GstVideoFormat gst_format;
} wl_VideoFormat;

static const wl_VideoFormat formats[] = {
#if G_BYTE_ORDER == G_BIG_ENDIAN
  {TBM_FORMAT_XRGB8888, GST_VIDEO_FORMAT_xRGB},
  {TBM_FORMAT_XBGR8888, GST_VIDEO_FORMAT_xBGR},
  {TBM_FORMAT_RGBX8888, GST_VIDEO_FORMAT_RGBx},
  {TBM_FORMAT_BGRX8888, GST_VIDEO_FORMAT_BGRx},
  {TBM_FORMAT_ARGB8888, GST_VIDEO_FORMAT_ARGB},
  {TBM_FORMAT_ABGR8888, GST_VIDEO_FORMAT_RGBA},
  {TBM_FORMAT_RGBA8888, GST_VIDEO_FORMAT_RGBA},
  {TBM_FORMAT_BGRA8888, GST_VIDEO_FORMAT_BGRA},
#else
  {TBM_FORMAT_XRGB8888, GST_VIDEO_FORMAT_BGRx},
  {TBM_FORMAT_XBGR8888, GST_VIDEO_FORMAT_RGBx},
  {TBM_FORMAT_RGBX8888, GST_VIDEO_FORMAT_xBGR},
  {TBM_FORMAT_BGRX8888, GST_VIDEO_FORMAT_xRGB},
  {TBM_FORMAT_ARGB8888, GST_VIDEO_FORMAT_BGRA},
  {TBM_FORMAT_ABGR8888, GST_VIDEO_FORMAT_RGBA},
  {TBM_FORMAT_RGBA8888, GST_VIDEO_FORMAT_ABGR},
  {TBM_FORMAT_BGRA8888, GST_VIDEO_FORMAT_ARGB},
#endif
  {TBM_FORMAT_RGB565, GST_VIDEO_FORMAT_RGB16},
  {TBM_FORMAT_BGR565, GST_VIDEO_FORMAT_BGR16},
  {TBM_FORMAT_RGB888, GST_VIDEO_FORMAT_RGB},
  {TBM_FORMAT_BGR888, GST_VIDEO_FORMAT_BGR},
  {TBM_FORMAT_YUYV, GST_VIDEO_FORMAT_YUY2},
  {TBM_FORMAT_YVYU, GST_VIDEO_FORMAT_YVYU},
  {TBM_FORMAT_UYVY, GST_VIDEO_FORMAT_UYVY},
  {TBM_FORMAT_AYUV, GST_VIDEO_FORMAT_AYUV},
  {TBM_FORMAT_NV12, GST_VIDEO_FORMAT_NV12},
  {TBM_FORMAT_NV21, GST_VIDEO_FORMAT_NV21},
  {TBM_FORMAT_NV16, GST_VIDEO_FORMAT_NV16},
  {TBM_FORMAT_YUV410, GST_VIDEO_FORMAT_YUV9},
  {TBM_FORMAT_YVU410, GST_VIDEO_FORMAT_YVU9},
  {TBM_FORMAT_YUV411, GST_VIDEO_FORMAT_Y41B},
  {TBM_FORMAT_YUV420, GST_VIDEO_FORMAT_I420},
  {TBM_FORMAT_YVU420, GST_VIDEO_FORMAT_YV12},
  {TBM_FORMAT_YUV422, GST_VIDEO_FORMAT_Y42B},
  {TBM_FORMAT_YUV444, GST_VIDEO_FORMAT_v308},
  {TBM_FORMAT_NV12MT, GST_VIDEO_FORMAT_ST12},
  {TBM_FORMAT_NV12,   GST_VIDEO_FORMAT_SN12},
};

uint32_t
gst_video_format_to_wayland_format (GstVideoFormat format)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (formats); i++)
    if (formats[i].gst_format == format)
      return formats[i].wl_format;

  GST_WARNING ("wayland video format not found");
  return -1;
}

GstVideoFormat
gst_wayland_format_to_video_format (uint32_t wl_format)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (formats); i++)
    if (formats[i].wl_format == wl_format)
      return formats[i].gst_format;

  GST_WARNING ("gst video format not found");
  return GST_VIDEO_FORMAT_UNKNOWN;
}

const gchar *
gst_wayland_format_to_string (uint32_t wl_format)
{
  return gst_video_format_to_string
      (gst_wayland_format_to_video_format (wl_format));
}
#endif
