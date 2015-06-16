/*
 * wfdrtspsrc
 *
 * Copyright (c) 2000 - 2014 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: JongHyuk Choi <jhchoi.choi@samsung.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */



#ifndef __GST_WFDRTSPSRC_H__
#define __GST_WFDRTSPSRC_H__

#include <gst/gst.h>

G_BEGIN_DECLS

#include <gst/rtsp/gstrtspconnection.h>
#include <gst/rtsp/gstrtspmessage.h>
#include <gst/rtsp/gstrtspurl.h>
#include <gst/rtsp/gstrtsprange.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include "wfdrtspconfigmessage.h"
#define ENABLE_WFD_MESSAGE

#include "wfdrtspmanager.h"

#define GST_TYPE_WFDRTSPSRC \
  (gst_wfdrtspsrc_get_type())
#define GST_WFDRTSPSRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_WFDRTSPSRC,GstWFDRTSPSrc))
#define GST_WFDRTSPSRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_WFDRTSPSRC,GstWFDRTSPSrcClass))
#define GST_IS_WFDRTSPSRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_WFDRTSPSRC))
#define GST_IS_WFDRTSPSRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_WFDRTSPSRC))
#define GST_WFDRTSPSRC_CAST(obj) \
  ((GstWFDRTSPSrc *)(obj))

typedef struct _GstWFDRTSPSrc GstWFDRTSPSrc;
typedef struct _GstWFDRTSPSrcClass GstWFDRTSPSrcClass;

#define GST_WFD_RTSP_TASK_GET_LOCK(wfd)   ((GST_WFDRTSPSRC_CAST(wfd)->task_rec_lock))
#define GST_WFD_RTSP_TASK_LOCK(wfd)       (g_rec_mutex_lock (&(GST_WFD_RTSP_TASK_GET_LOCK(wfd))))
#define GST_WFD_RTSP_TASK_UNLOCK(wfd)     (g_rec_mutex_unlock (&(GST_WFD_RTSP_TASK_GET_LOCK(wfd))))

typedef enum
{
  WFD_PARAM_NONE,
  WFD_ROUTE,
  WFD_CONNECTOR_TYPE,
  WFD_STANDBY,
  WFD_IDR_REQUEST,
  WFD_UIBC_CAPABILITY,
  WFD_UIBC_SETTING,
  WFD_SINK_STATUS
} GstWFDParam;

struct _GstWFDRTSPSrc {
  GstBin           parent;

  WFDRTSPManager           *manager;

  /* task and mutex */
  GstTask         *task;
  GRecMutex task_rec_lock;
  gint             loop_cmd;
  gboolean         waiting;
  gboolean do_stop;
  GstWFDParam wfd_param;

  /* properties */
  GstRTSPLowerTrans protocols;
  gboolean          debug;
  guint             retry;
  GTimeVal          tcp_timeout;
  GTimeVal         *ptcp_timeout;
  gchar            *proxy_host;
  guint             proxy_port;
  gchar            *proxy_user;
  gchar            *proxy_passwd;
  guint             rtp_blocksize;
  gchar            *user_id;
  gchar            *user_pw;
  GstStructure *audio_param;
  GstStructure *video_param;
  GstStructure *hdcp_param;
  /* Set User-agent */
  gchar *user_agent;
  /* Set RTP port */
  gint primary_rtpport;
  gint secondary_rtpport;

  /* state */
  GstRTSPState       state;

  /* supported methods */
  gint               methods;

  GstWFDRTSPConnInfo  conninfo;

  guint video_height;
  guint video_width;
  guint video_framerate;
  gchar * audio_format;
  guint audio_channels;
  guint audio_bitwidth;
  guint audio_frequency;

  gboolean is_paused;
#ifdef ENABLE_WFD_MESSAGE
  void *message_handle;
  gboolean extended_wfd_message_support;
#endif
};

struct _GstWFDRTSPSrcClass {
  GstBinClass parent_class;

  /* signals */
  void     (*update_media_info)       (GstWFDRTSPSrc *src, GstStructure * str);
  void     (*change_av_format)       (GstWFDRTSPSrc *src, gpointer *need_to_flush);

  /* actions */
  void     (*pause)   (GstWFDRTSPSrc *src);
  void     (*resume)   (GstWFDRTSPSrc *src);
  void     (*close)   (GstWFDRTSPSrc *src);
};

GType gst_wfdrtspsrc_get_type(void);

G_END_DECLS

#endif /* __GST_WFDRTSPSRC_H__ */
