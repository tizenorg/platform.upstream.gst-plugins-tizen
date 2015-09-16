/*
 * wfdbasesrc
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



#ifndef __GST_WFD_BASE_SRC_H__
#define __GST_WFD_BASE_SRC_H__

#include <gst/gst.h>

G_BEGIN_DECLS

#include <gst/rtsp/rtsp.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>

#include "gstwfdsinkmessage.h"

#define ENABLE_WFD_MESSAGE

#define GST_TYPE_WFD_BASE_SRC (gst_wfd_base_src_get_type())
#define GST_WFD_BASE_SRC(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_WFD_BASE_SRC,GstWFDBaseSrc))
#define GST_WFD_BASE_SRC_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_WFD_BASE_SRC,GstWFDBaseSrcClass))
#define GST_WFD_BASE_SRC_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_WFD_BASE_SRC, GstWFDBaseSrcClass))
#define GST_IS_WFD_BASE_SRC(obj)  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_WFD_BASE_SRC))
#define GST_IS_WFD_BASE_SRC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_WFD_BASE_SRC))
#define GST_WFD_BASE_SRC_CAST(obj)   ((GstWFDBaseSrc *)(obj))

typedef struct _GstWFDBaseSrc GstWFDBaseSrc;
typedef struct _GstWFDBaseSrcClass GstWFDBaseSrcClass;
typedef struct _GstWFDBaseSrcPrivate GstWFDBaseSrcPrivate;

#define GST_WFD_BASE_STATE_GET_LOCK(wfd)   ((GST_WFD_BASE_SRC_CAST(wfd)->state_rec_lock))
#define GST_WFD_BASE_STATE_LOCK(wfd)       (g_rec_mutex_lock (&(GST_WFD_BASE_STATE_GET_LOCK(wfd))))
#define GST_WFD_BASE_STATE_UNLOCK(wfd)     (g_rec_mutex_unlock (&(GST_WFD_BASE_STATE_GET_LOCK(wfd))))

typedef enum
{
  WFD_PARAM_NONE,
  WFD_ROUTE,
  WFD_CONNECTOR_TYPE,
  WFD_STANDBY,
  WFD_IDR_REQUEST,
  WFD_SINK_STATUS
} GstWFDParam;

typedef struct {
  GstWFDParam type;

  union {
    struct {
      GstWFDSinkType type;
    }route_setting;
    struct {
      GstWFDConnector type;
    }connector_setting;
  };
}GstWFDRequestParam;

struct _GstWFDBaseSrc {
  GstBin           parent;

  /*< protected >*/
  GstPad       *srcpad;
  GstCaps      *caps;
  gboolean      is_ipv6;

  GstWFDRequestParam request_param;
  gboolean          enable_pad_probe;

  /* mutex for protecting state changes */
  GRecMutex        state_rec_lock;

  GstWFDBaseSrcPrivate *priv;
};

struct _GstWFDBaseSrcClass {
  GstBinClass parent_class;

  /*< public >*/
  /* virtual methods for subclasses */
  GstRTSPResult     (*handle_set_parameter)       (GstWFDBaseSrc *src, GstRTSPMessage * request, GstRTSPMessage * response);
  GstRTSPResult      (*handle_get_parameter)     (GstWFDBaseSrc *src, GstRTSPMessage * request, GstRTSPMessage * response);
  GstRTSPResult      (*prepare_transport)     (GstWFDBaseSrc *src, gint rtpport, gint rtcpport);
  GstRTSPResult      (*configure_transport)    (GstWFDBaseSrc *src, GstRTSPTransport * transport);
  gboolean      (*push_event)     (GstWFDBaseSrc *src, GstEvent *event);
  void      (*set_state)     (GstWFDBaseSrc *src, GstState state);
  void      (*cleanup)     (GstWFDBaseSrc *src);

  /* signals */
  void     (*update_media_info)       (GstWFDBaseSrc *src, GstStructure * str);
  void     (*change_av_format)       (GstWFDBaseSrc *src, gpointer *need_to_flush);

  /* actions */
  void     (*pause)   (GstWFDBaseSrc *src);
  void     (*resume)   (GstWFDBaseSrc *src);
  void     (*close)   (GstWFDBaseSrc *src);
  void     (*set_standby)   (GstWFDBaseSrc *src);
};

GType gst_wfd_base_src_get_type(void);

gboolean gst_wfd_base_src_set_target (GstWFDBaseSrc * src, GstPad *target);
gboolean gst_wfd_base_src_activate (GstWFDBaseSrc *src);
void gst_wfd_base_src_get_transport_info (GstWFDBaseSrc *src,
    GstRTSPTransport * transport, const gchar ** destination, gint * min, gint * max);
GstRTSPTransport gst_wfd_base_src_get_transport (GstWFDBaseSrc *src);

G_END_DECLS

#endif /* __GST_WFD_BASE_SRC_H__ */
