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

/**
* SECTION:element-wfdbasesrc
*
* Makes a connection to an RTSP server and read the data.
* Device recognition is through wifi direct.
* wfdbasesrc strictly follows Wifi display specification.
*
* RTSP supports transport over TCP or UDP in unicast or multicast mode. By
* default wfdbasesrc will negotiate a connection in the following order:
* UDP unicast/UDP multicast/TCP. The order cannot be changed but the allowed
* protocols can be controlled with the #GstWFDBaseSrc:protocols property.
*
* wfdbasesrc currently understands WFD capability negotiation messages
*
* wfdbasesrc will internally instantiate an RTP session manager element
* that will handle the RTCP messages to and from the server, jitter removal,
* packet reordering along with providing a clock for the pipeline.
* This feature is implemented using the gstrtpbin element.
*
* wfdbasesrc acts like a live source and will therefore only generate data in the
* PLAYING state.
*
* <refsect2>
* <title>Example launch line</title>
* |[
* gst-launch wfdbasesrc location=rtsp://some.server/url ! fakesink
* ]| Establish a connection to an RTSP server and send the raw RTP packets to a
* fakesink.
* </refsect2>
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <stdio.h>
#include <stdarg.h>
#include <arpa/inet.h>
#include <dlfcn.h>

#include "gstwfdbasesrc.h"

#ifdef G_OS_WIN32
#include <winsock2.h>
#endif

GST_DEBUG_CATEGORY_STATIC (wfdbasesrc_debug);
#define GST_CAT_DEFAULT (wfdbasesrc_debug)

#define GST_WFD_BASE_TASK_GET_LOCK(wfd)    (GST_WFD_BASE_SRC_CAST(wfd)->priv->task_rec_lock)
#define GST_WFD_BASE_TASK_LOCK(wfd)        (g_rec_mutex_lock (&(GST_WFD_BASE_TASK_GET_LOCK(wfd))))
#define GST_WFD_BASE_TASK_UNLOCK(wfd)      (g_rec_mutex_unlock(&(GST_WFD_BASE_TASK_GET_LOCK(wfd))))

static GstStaticPadTemplate gst_wfd_base_src_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp, "
        "media = (string) \"video\", "
        "clock-rate = (int) 90000, "
        "payload = (int) 33"));

/* signals and args */
enum
{
  SIGNAL_UPDATE_MEDIA_INFO,
  SIGNAL_AV_FORMAT_CHANGE,
  SIGNAL_PAUSE,
  SIGNAL_RESUME,
  SIGNAL_CLOSE,
  SIGNAL_SET_STANDBY,
  LAST_SIGNAL
};

/* properties default values */
#define DEFAULT_LOCATION      NULL
#define DEFAULT_DEBUG            FALSE
#define DEFAULT_RETRY            50
#define DEFAULT_TCP_TIMEOUT      20000000
#define DEFAULT_PROXY            NULL
#define DEFAULT_RTP_BLOCKSIZE    0
#define DEFAULT_USER_ID          NULL
#define DEFAULT_USER_PW          NULL
#define DEFAULT_PORT_RANGE       NULL
#define DEFAULT_USER_AGENT       "TIZEN-WFD-SINK"

enum
{
  PROP_0,
  PROP_LOCATION,
  PROP_DEBUG,
  PROP_RETRY,
  PROP_TCP_TIMEOUT,
  PROP_RTP_BLOCKSIZE,
  PROP_USER_AGENT,
  PROP_AUDIO_PARAM,
  PROP_VIDEO_PARAM,
  PROP_HDCP_PARAM,
  PROP_ENABLE_PAD_PROBE,
  PROP_LAST
};

/* commands we send to out loop to notify it of events */
#define WFD_CMD_OPEN	(1 << 0)
#define WFD_CMD_PLAY	(1 << 1)
#define WFD_CMD_PAUSE	(1 << 2)
#define WFD_CMD_CLOSE	(1 << 3)
#define WFD_CMD_WAIT	(1 << 4)
#define WFD_CMD_REQUEST	(1 << 6)
#define WFD_CMD_LOOP	(1 << 7)
#define WFD_CMD_ALL         ((WFD_CMD_LOOP << 1) - 1)

#define GST_ELEMENT_PROGRESS(el, type, code, text)      \
G_STMT_START {                                          \
  gchar *__txt = _gst_element_error_printf text;        \
  gst_element_post_message (GST_ELEMENT_CAST (el),      \
      gst_message_new_progress (GST_OBJECT_CAST (el),   \
          GST_PROGRESS_TYPE_ ##type, code, __txt));     \
  g_free (__txt);                                       \
} G_STMT_END


typedef struct _GstWFDConnInfo {
  gchar              *location;
  GstRTSPUrl         *url;
  gchar              *url_str;
  GstRTSPConnection  *connection;
  gboolean            connected;
}GstWFDConnInfo;

#define GST_WFD_BASE_SRC_GET_PRIVATE(obj)  \
   (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GST_TYPE_WFD_BASE_SRC, GstWFDBaseSrcPrivate))

struct _GstWFDBaseSrcPrivate
{
  /* state */
  GstRTSPState       state;

  /* supported RTSP methods */
  gint               methods;

  /* task and mutex */
  GstTask         *task;
  GRecMutex task_rec_lock;
  gint             pending_cmd;
  gint             busy_cmd;
  gboolean do_stop;

  /* properties */
  gboolean          debug;
  guint             retry;
  GTimeVal          tcp_timeout;
  GTimeVal         *ptcp_timeout;
  guint             rtp_blocksize;
  GstStructure *audio_param;
  GstStructure *video_param;
  GstStructure *hdcp_param;
  gchar *user_agent;

  /* Set RTP port */
  gint primary_rtpport;

  GstWFDConnInfo  conninfo;
  GstRTSPLowerTrans protocol;

  /* stream info */
  guint video_height;
  guint video_width;
  guint video_framerate;
  gchar * audio_format;
  guint audio_channels;
  guint audio_bitwidth;
  guint audio_frequency;
};

/* object */
static void gst_wfd_base_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_wfd_base_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

/* element */
static GstStateChangeReturn gst_wfd_base_src_change_state (GstElement * element,
    GstStateChange transition);
static gboolean gst_wfd_base_src_send_event (GstElement * element, GstEvent * event);

/* bin */
static void gst_wfd_base_src_handle_message (GstBin * bin, GstMessage * message);

/* pad */
static gboolean gst_wfd_base_src_handle_src_event (GstPad * pad, GstObject *parent, GstEvent * event);
static gboolean gst_wfd_base_src_handle_src_query(GstPad * pad, GstObject *parent, GstQuery * query);

/* fundemental functions */
static GstRTSPResult gst_wfd_base_src_open (GstWFDBaseSrc * src);
static gboolean gst_wfd_base_src_setup (GstWFDBaseSrc * src);
static GstRTSPResult gst_wfd_base_src_play (GstWFDBaseSrc * src);
static GstRTSPResult gst_wfd_base_src_pause (GstWFDBaseSrc * src);
static GstRTSPResult gst_wfd_base_src_close (GstWFDBaseSrc * src, gboolean only_close);
static void gst_wfd_base_src_send_pause_cmd (GstWFDBaseSrc * src);
static void gst_wfd_base_src_send_play_cmd (GstWFDBaseSrc * src);
static void gst_wfd_base_src_send_close_cmd (GstWFDBaseSrc * src);
static void gst_wfd_base_src_set_standby (GstWFDBaseSrc * src);

/* URI interface */
static void gst_wfd_base_src_uri_handler_init (gpointer g_iface,
    gpointer iface_data);
static gboolean gst_wfd_base_src_uri_set_uri (GstURIHandler * handler,
    const gchar * uri, GError **error);

/* task */
static GstRTSPResult gst_wfd_base_src_loop (GstWFDBaseSrc * src);
static gboolean gst_wfd_base_src_loop_send_cmd (GstWFDBaseSrc * src, gint cmd,
    gint mask);

static gboolean gst_wfd_base_src_push_event (GstWFDBaseSrc * src, GstEvent * event);
static void gst_wfd_base_src_set_tcp_timeout (GstWFDBaseSrc * src, guint64 timeout);


static GstRTSPResult
gst_wfd_base_src_send (GstWFDBaseSrc * src, GstRTSPMessage * request, GstRTSPMessage * response,
    GstRTSPStatusCode * code);

static GstRTSPResult gst_wfd_base_src_get_video_parameter(GstWFDBaseSrc * src, GstWFDMessage *msg);
static GstRTSPResult gst_wfd_base_src_get_audio_parameter(GstWFDBaseSrc * src, GstWFDMessage *msg);

/* util */
static GstRTSPResult _rtsp_message_dump (GstRTSPMessage * msg);
static const char *_cmd_to_string (guint cmd);

static guint gst_wfd_base_src_signals[LAST_SIGNAL] = { 0 };

static GstBinClass *parent_class = NULL;

static void gst_wfd_base_src_class_init (GstWFDBaseSrcClass * klass);
static void gst_wfd_base_src_init (GstWFDBaseSrc * src, gpointer g_class);
static void gst_wfd_base_src_finalize (GObject * object);

GType
gst_wfd_base_src_get_type (void)
{
  static volatile gsize wfd_base_src_type = 0;

  static const GInterfaceInfo urihandler_info = {
    gst_wfd_base_src_uri_handler_init,
    NULL,
    NULL
  };

  if (g_once_init_enter (&wfd_base_src_type)) {
    GType object_type;
    static const GTypeInfo wfd_base_src_info = {
      sizeof (GstWFDBaseSrcClass),
      NULL,
      NULL,
      (GClassInitFunc) gst_wfd_base_src_class_init,
      NULL,
      NULL,
      sizeof (GstWFDBaseSrc),
      0,
      (GInstanceInitFunc) gst_wfd_base_src_init,
    };

    object_type = g_type_register_static (GST_TYPE_BIN,
        "GstWFDBaseSrc", &wfd_base_src_info, G_TYPE_FLAG_ABSTRACT);

    g_type_add_interface_static (object_type, GST_TYPE_URI_HANDLER,
        &urihandler_info);

    g_once_init_leave (&wfd_base_src_type, object_type);
  }
  return wfd_base_src_type;
}


static void
gst_wfd_base_src_class_init (GstWFDBaseSrcClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBinClass *gstbin_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbin_class = (GstBinClass *) klass;

  GST_DEBUG_CATEGORY_INIT (wfdbasesrc_debug, "wfdbasesrc", 0, "Wi-Fi Display Sink Base Source");

  g_type_class_add_private (klass, sizeof (GstWFDBaseSrcPrivate));

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->set_property = gst_wfd_base_src_set_property;
  gobject_class->get_property = gst_wfd_base_src_get_property;
  gobject_class->finalize = gst_wfd_base_src_finalize;

  g_object_class_install_property (gobject_class, PROP_LOCATION,
      g_param_spec_string ("location", "RTSP Location",
          "Location of the RTSP url to read",
          DEFAULT_LOCATION, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_DEBUG,
      g_param_spec_boolean ("debug", "Debug",
          "Dump request and response messages to stdout",
          DEFAULT_DEBUG, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_RETRY,
      g_param_spec_uint ("retry", "Retry",
          "Max number of retries when connecting Wi-Fi Display source",
          0, G_MAXUINT16, DEFAULT_RETRY,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_TCP_TIMEOUT,
      g_param_spec_uint64 ("tcp-timeout", "TCP Timeout",
          "Fail after timeout microseconds on TCP connections (0 = disabled)",
          0, G_MAXUINT64, DEFAULT_TCP_TIMEOUT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_RTP_BLOCKSIZE,
      g_param_spec_uint ("rtp-blocksize", "RTP Blocksize",
          "RTP package size to suggest to server (0 = disabled)",
          0, 65536, DEFAULT_RTP_BLOCKSIZE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

   g_object_class_install_property (gobject_class, PROP_USER_AGENT,
      g_param_spec_string ("user-agent", "User Agent",
          "User agent specified string", DEFAULT_USER_AGENT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_AUDIO_PARAM,
      g_param_spec_boxed ("audio-param", "audio parameters",
          "A GstStructure specifies the mapping for audio parameters",
          GST_TYPE_STRUCTURE, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_VIDEO_PARAM,
      g_param_spec_boxed ("video-param", "video parameters",
          "A GstStructure specifies the mapping for video parameters",
          GST_TYPE_STRUCTURE, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_HDCP_PARAM,
      g_param_spec_boxed ("hdcp-param", "HDCP parameters",
          "A GstStructure specifies the mapping for HDCP parameters",
          GST_TYPE_STRUCTURE, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_ENABLE_PAD_PROBE,
          g_param_spec_boolean ("enable-pad-probe", "Enable Pad Probe",
              "Enable pad probe for debugging",
              FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_wfd_base_src_signals[SIGNAL_UPDATE_MEDIA_INFO] =
      g_signal_new ("update-media-info", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (GstWFDBaseSrcClass, update_media_info),
      NULL, NULL, g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE, 1, GST_TYPE_STRUCTURE);

  gst_wfd_base_src_signals[SIGNAL_AV_FORMAT_CHANGE] =
      g_signal_new ("change-av-format", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (GstWFDBaseSrcClass, change_av_format),
      NULL, NULL, g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1, G_TYPE_POINTER);

  gst_wfd_base_src_signals[SIGNAL_PAUSE] =
      g_signal_new ("pause", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_STRUCT_OFFSET (GstWFDBaseSrcClass, pause), NULL, NULL,
      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0, G_TYPE_NONE);

  gst_wfd_base_src_signals[SIGNAL_RESUME] =
      g_signal_new ("resume", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_STRUCT_OFFSET (GstWFDBaseSrcClass, resume), NULL, NULL,
      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0, G_TYPE_NONE);

  gst_wfd_base_src_signals[SIGNAL_CLOSE] =
      g_signal_new ("close", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_STRUCT_OFFSET (GstWFDBaseSrcClass, close), NULL, NULL,
      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0, G_TYPE_NONE);

  gst_wfd_base_src_signals[SIGNAL_SET_STANDBY] =
      g_signal_new ("set-standby", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_STRUCT_OFFSET (GstWFDBaseSrcClass, set_standby), NULL, NULL,
      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0, G_TYPE_NONE);

  gstelement_class->send_event =
      GST_DEBUG_FUNCPTR(gst_wfd_base_src_send_event);
  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR(gst_wfd_base_src_change_state);

  gstbin_class->handle_message =
      GST_DEBUG_FUNCPTR(gst_wfd_base_src_handle_message);

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_wfd_base_src_src_template));

  klass->pause = GST_DEBUG_FUNCPTR (gst_wfd_base_src_send_pause_cmd);
  klass->resume = GST_DEBUG_FUNCPTR (gst_wfd_base_src_send_play_cmd);
  klass->close = GST_DEBUG_FUNCPTR (gst_wfd_base_src_send_close_cmd);
  klass->set_standby = GST_DEBUG_FUNCPTR (gst_wfd_base_src_set_standby);

  klass->push_event = GST_DEBUG_FUNCPTR (gst_wfd_base_src_push_event);
}

static GstStructure *
gst_wfd_rtsp_set_default_audio_param ()
{
  GstStructure *param = NULL;
  param = gst_structure_new ("audio_param",
        "audio_codec", G_TYPE_UINT, 0x3,
        "audio_latency", G_TYPE_UINT, 0x0,
        "audio_channels", G_TYPE_UINT, 0x3,
        "audio_sampling_frequency", G_TYPE_UINT, 0x1,
        NULL);

  return param;
}

static GstStructure *
gst_wfd_rtsp_set_default_video_param ()
{
  GstStructure *param = NULL;
  param = gst_structure_new ("video_param",
        "video_codec", G_TYPE_UINT, 0x1,
        "video_native_resolution", G_TYPE_UINT, 0x20,
        "video_cea_support", G_TYPE_UINT, 0x194ab,
        "video_vesa_support", G_TYPE_UINT, 0x15555555,
        "video_hh_support", G_TYPE_UINT, 0x555,
        "video_profile", G_TYPE_UINT, 0x1,
        "video_level", G_TYPE_UINT, 0x2,
        "video_latency", G_TYPE_UINT, 0x0,
        "video_vertical_resolution", G_TYPE_INT, 1200,
        "video_horizontal_resolution", G_TYPE_INT, 1920,
        "video_minimum_slicing", G_TYPE_INT, 0,
        "video_slice_enc_param", G_TYPE_INT, 200,
        "video_framerate_control_support", G_TYPE_INT, 11,
        NULL);

  return param;
}

static void
gst_wfd_base_src_init (GstWFDBaseSrc * src, gpointer g_class)
{
  GstPadTemplate *template = NULL;

#ifdef G_OS_WIN32
  WSADATA wsa_data;

  if (WSAStartup (MAKEWORD (2, 2), &wsa_data) != 0) {
    GST_ERROR_OBJECT (src, "WSAStartup failed: 0x%08x", WSAGetLastError ());
  }
#endif
  src->priv = GST_WFD_BASE_SRC_GET_PRIVATE (src);

  src->is_ipv6 = FALSE;
  src->srcpad = NULL;

  src->priv->conninfo.location = g_strdup (DEFAULT_LOCATION);
  src->priv->conninfo.url_str = NULL;
  src->priv->protocol = GST_RTSP_LOWER_TRANS_UNKNOWN;
  src->priv->debug = DEFAULT_DEBUG;
  src->priv->retry = DEFAULT_RETRY;
  gst_wfd_base_src_set_tcp_timeout (src, DEFAULT_TCP_TIMEOUT);
  src->priv->rtp_blocksize = DEFAULT_RTP_BLOCKSIZE;
  src->priv->user_agent = g_strdup (DEFAULT_USER_AGENT);
  src->priv->hdcp_param = NULL;
  src->priv->audio_param = gst_wfd_rtsp_set_default_audio_param ();
  src->priv->video_param = gst_wfd_rtsp_set_default_video_param ();

  src->enable_pad_probe = FALSE;
  src->request_param.type = WFD_PARAM_NONE;

  g_rec_mutex_init (&(src->state_rec_lock));
  g_rec_mutex_init (&(src->priv->task_rec_lock));

  /* create ghost pad for using src pad */
  template = gst_static_pad_template_get (&gst_wfd_base_src_src_template);
  src->srcpad = gst_ghost_pad_new_no_target_from_template ("src", template);
  gst_element_add_pad (GST_ELEMENT_CAST (src), src->srcpad);
  gst_object_unref (template);

  gst_pad_set_event_function (src->srcpad,
      GST_DEBUG_FUNCPTR (gst_wfd_base_src_handle_src_event));
  gst_pad_set_query_function (src->srcpad,
      GST_DEBUG_FUNCPTR (gst_wfd_base_src_handle_src_query));

  src->caps = gst_static_pad_template_get_caps (&gst_wfd_base_src_src_template);
  gst_pad_set_caps (src->srcpad, src->caps);

  src->priv->do_stop = FALSE;
  src->priv->state = GST_RTSP_STATE_INVALID;

  GST_OBJECT_FLAG_SET (src, GST_ELEMENT_FLAG_SOURCE);
}

static void
gst_wfd_base_src_finalize (GObject * object)
{
  GstWFDBaseSrc *src;

  src = GST_WFD_BASE_SRC (object);

  gst_rtsp_url_free (src->priv->conninfo.url);
  if (src->priv->conninfo.location)
    g_free (src->priv->conninfo.location);
  src->priv->conninfo.location = NULL;
  if (src->priv->conninfo.url_str)
    g_free (src->priv->conninfo.url_str);
  src->priv->conninfo.url_str = NULL;
  if (src->priv->conninfo.location)
    g_free (src->priv->conninfo.location);
  src->priv->conninfo.location = NULL;
  if (src->priv->conninfo.url_str)
    g_free (src->priv->conninfo.url_str);
  src->priv->conninfo.url_str = NULL;
  if (src->priv->user_agent)
    g_free (src->priv->user_agent);
  src->priv->user_agent = NULL;
  if(src->priv->audio_param)
    gst_structure_free(src->priv->audio_param);
  src->priv->audio_param = NULL;
  if(src->priv->video_param)
    gst_structure_free(src->priv->video_param);
  src->priv->video_param = NULL;
  if(src->priv->hdcp_param)
    gst_structure_free(src->priv->hdcp_param);
  src->priv->hdcp_param = NULL;

  /* free locks */
  g_rec_mutex_clear (&(src->state_rec_lock));
  g_rec_mutex_clear (&(src->priv->task_rec_lock));

#ifdef G_OS_WIN32
  WSACleanup ();
#endif

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_wfd_base_src_set_tcp_timeout (GstWFDBaseSrc * src, guint64 timeout)
{
  GstWFDBaseSrcPrivate *priv = src->priv;

  priv->tcp_timeout.tv_sec = timeout / G_USEC_PER_SEC;
  priv->tcp_timeout.tv_usec = timeout % G_USEC_PER_SEC;

  if (timeout != 0)
    priv->ptcp_timeout = &priv->tcp_timeout;
  else
    priv->ptcp_timeout = NULL;
}

static void
gst_wfd_base_src_set_property (GObject * object, guint prop_id, const GValue * value,
    GParamSpec * pspec)
{
  GstWFDBaseSrc *src = GST_WFD_BASE_SRC (object);
  GError *err = NULL;

  switch (prop_id) {
    case PROP_LOCATION:
      gst_wfd_base_src_uri_set_uri (GST_URI_HANDLER (src),
          g_value_get_string (value), &err);
      break;
    case PROP_DEBUG:
      src->priv->debug = g_value_get_boolean (value);
      break;
    case PROP_RETRY:
      src->priv->retry = g_value_get_uint (value);
      break;
    case PROP_TCP_TIMEOUT:
      gst_wfd_base_src_set_tcp_timeout (src, g_value_get_uint64 (value));
      break;
    case PROP_RTP_BLOCKSIZE:
      src->priv->rtp_blocksize = g_value_get_uint (value);
      break;
    case PROP_USER_AGENT:
      if (src->priv->user_agent)
        g_free(src->priv->user_agent);
      src->priv->user_agent = g_value_dup_string (value);
      break;
    case PROP_AUDIO_PARAM:
    {
      const GstStructure *s = gst_value_get_structure (value);
      if (src->priv->audio_param)
        gst_structure_free (src->priv->audio_param);
      if (s)
        src->priv->audio_param = gst_structure_copy (s);
      else
        src->priv->audio_param = NULL;
      break;
    }
    case PROP_VIDEO_PARAM:
    {
      const GstStructure *s = gst_value_get_structure (value);
      if (src->priv->video_param)
        gst_structure_free (src->priv->video_param);
      if (s)
        src->priv->video_param = gst_structure_copy (s);
      else
        src->priv->video_param = NULL;
      break;
    }
    case PROP_HDCP_PARAM:
    {
      const GstStructure *s = gst_value_get_structure (value);
      if (src->priv->hdcp_param)
        gst_structure_free (src->priv->hdcp_param);
      if (s)
        src->priv->hdcp_param = gst_structure_copy (s);
      else
        src->priv->hdcp_param = NULL;
      break;
    }
    case PROP_ENABLE_PAD_PROBE:
      src->enable_pad_probe = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_wfd_base_src_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstWFDBaseSrc *src = GST_WFD_BASE_SRC (object);

  switch (prop_id) {
    case PROP_LOCATION:
      g_value_set_string (value, src->priv->conninfo.location);
      break;
    case PROP_DEBUG:
      g_value_set_boolean (value, src->priv->debug);
      break;
    case PROP_RETRY:
      g_value_set_uint (value, src->priv->retry);
      break;
    case PROP_TCP_TIMEOUT:
    {
      guint64 timeout;

      timeout = src->priv->tcp_timeout.tv_sec * (guint64)G_USEC_PER_SEC +
          src->priv->tcp_timeout.tv_usec;
      g_value_set_uint64 (value, timeout);
      break;
    }
    case PROP_RTP_BLOCKSIZE:
      g_value_set_uint (value, src->priv->rtp_blocksize);
      break;
    case PROP_USER_AGENT:
      g_value_set_string (value, src->priv->user_agent);
      break;
    case PROP_AUDIO_PARAM:
      gst_value_set_structure (value, src->priv->audio_param);
      break;
    case PROP_VIDEO_PARAM:
      gst_value_set_structure (value, src->priv->video_param);
      break;
    case PROP_HDCP_PARAM:
      gst_value_set_structure (value, src->priv->hdcp_param);
      break;
    case PROP_ENABLE_PAD_PROBE:
      g_value_set_boolean (value, src->enable_pad_probe);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_wfd_base_src_cleanup (GstWFDBaseSrc * src)
{
  GstWFDBaseSrcClass *klass = NULL;

  klass = GST_WFD_BASE_SRC_GET_CLASS (src);

  if (src->caps)
    gst_caps_unref (src->caps);
  src->caps = NULL;

  if (klass->cleanup)
  	klass->cleanup(src);
}

static void
gst_wfd_base_src_flush (GstWFDBaseSrc * src, gboolean flush)
{
  GstWFDBaseSrcClass *klass;
  GstEvent *event;
  GstState state;

  klass = GST_WFD_BASE_SRC_GET_CLASS (src);

  if (flush) {
    event = gst_event_new_flush_start ();
    GST_DEBUG_OBJECT (src, "start flush");
    state = GST_STATE_PAUSED;
  } else {
    event = gst_event_new_flush_stop (FALSE);
    GST_DEBUG_OBJECT (src, "stop flush");
    state = GST_STATE_PLAYING;
  }

  if (klass->push_event)
    klass->push_event(src, event);

  if (klass->set_state)
    klass->set_state (src, state);
}

static GstRTSPResult
gst_wfd_base_src_connection_send (GstWFDBaseSrc * src,
    GstRTSPMessage * message, GTimeVal * timeout)
{
  GstRTSPResult ret = GST_RTSP_OK;

  if (src->priv->debug)
    _rtsp_message_dump (message);

  if (src->priv->conninfo.connection)
    ret = gst_rtsp_connection_send (src->priv->conninfo.connection, message, timeout);
  else
    ret = GST_RTSP_ERROR;

  return ret;
}

static GstRTSPResult
gst_wfd_base_src_connection_receive (GstWFDBaseSrc * src,
    GstRTSPMessage * message, GTimeVal * timeout)
{
  GstRTSPResult ret;

  if (src->priv->conninfo.connection)
    ret = gst_rtsp_connection_receive (src->priv->conninfo.connection, message, timeout);
  else
    ret = GST_RTSP_ERROR;

  if (src->priv->debug)
    _rtsp_message_dump (message);

  return ret;
}

static GstRTSPResult
gst_wfd_base_src_send_request (GstWFDBaseSrc * src)
{
  GstWFDRequestParam request_param = {0};
  GstRTSPMessage request = { 0 };
  GstRTSPMessage response = { 0 };
  GstRTSPResult res = GST_RTSP_OK;
  GstWFDResult wfd_res = GST_WFD_OK;
  GstWFDMessage *wfd_msg= NULL;
  gchar *rtsp_body = NULL;
  guint rtsp_body_length = 0;
  GString *rtsp_body_length_str = NULL;

  GST_OBJECT_LOCK(src);
  if (src->request_param.type==WFD_PARAM_NONE) {
    GST_DEBUG_OBJECT(src, "nothing to request...");
    GST_OBJECT_UNLOCK(src);
    return GST_RTSP_EINVAL;
  }
  request_param = src->request_param;
  memset (&src->request_param, 0, sizeof(GstWFDRequestParam));
  GST_OBJECT_UNLOCK(src);

  GST_DEBUG_OBJECT (src, "need to send request message with %d parameter", request_param.type);

  res = gst_rtsp_message_init_request (&request, GST_RTSP_SET_PARAMETER,
    src->priv->conninfo.url_str);
  if (res < 0)
    goto error;

  /* Create set_parameter body to be sent in the request */
  wfd_res = gst_wfd_message_new (&wfd_msg);
  if(wfd_res != GST_WFD_OK)
    goto error;
  wfd_res = gst_wfd_message_init (wfd_msg);
  if(wfd_res != GST_WFD_OK)
    goto error;

  switch(request_param.type) {
    case WFD_ROUTE:
     /* Note : RTSP M10  :
      *   Send RTSP SET_PARAMETER with wfd-route to change the WFD sink at which audio is rendered.
      *   Applies only when both a primary and secondary sinks are in WFD session with a WFD source.
      */
      wfd_res = gst_wfd_message_set_audio_sink_type (wfd_msg, request_param.route_setting.type);
      if(wfd_res != GST_WFD_OK)
        goto error;
      break;

    case WFD_CONNECTOR_TYPE:
     /* Note : RTSP M11  :
      *   Send RTSP SET_PARAMETER with wfd-connector-type to indicate change of active connector type,
      *     when the WFD source and WFD sink support content protection.
      */
      wfd_res = gst_wfd_message_set_connector_type (wfd_msg, request_param.connector_setting.type);
      if(wfd_res != GST_WFD_OK)
        goto error;
      break;

    case WFD_STANDBY:
     /* Note : RTSP M12  :
      *   Send RTSP SET_PARAMETER with wfd-standby to indicate that the sender is entering WFD standby mode.
      */
      wfd_res = gst_wfd_message_set_standby (wfd_msg, TRUE);
      if(wfd_res != GST_WFD_OK)
        goto error;
      break;

    case WFD_IDR_REQUEST:
     /* Note : RTSP M13  :
      *   Send RTSP SET_PARAMETER with wfd-idr-request to request IDR refresh.
      */
      wfd_res = gst_wfd_message_set_idr_request (wfd_msg);
      if(wfd_res != GST_WFD_OK)
        goto error;
      break;

    default:
      GST_ERROR_OBJECT (src, "Unhandled WFD message type...");
      goto error;
      break;
  }

  //gst_wfd_message_dump (wfd_msg);
  rtsp_body = gst_wfd_message_as_text (wfd_msg);
  if (rtsp_body == NULL) {
    GST_ERROR ("gst_wfd_message_as_text is failed");
    goto error;
  }

  rtsp_body_length = strlen(rtsp_body);
  rtsp_body_length_str = g_string_new ("");
  g_string_append_printf (rtsp_body_length_str,"%d",rtsp_body_length);

  GST_DEBUG ("WFD message body: %s", rtsp_body);

  /* add content-length type */
  gst_rtsp_message_add_header (&request, GST_RTSP_HDR_CONTENT_LENGTH, g_string_free (rtsp_body_length_str, FALSE));

  /* adding wfdrtsp data to request */
  res = gst_rtsp_message_set_body (&request,(guint8 *)rtsp_body, rtsp_body_length);
  if (res != GST_RTSP_OK) {
    GST_ERROR_OBJECT (src, "Failed to set body to rtsp request...");
    goto error;
  }

  gst_wfd_message_free (wfd_msg);

  /* send request message  */
  GST_DEBUG_OBJECT (src, "send reuest...");
  if ((res = gst_wfd_base_src_send (src, &request, &response,
      NULL)) < 0)
    goto error;

  return res;

/* ERRORS */
error:
 {
    if(wfd_msg)
      gst_wfd_message_free(wfd_msg);
    gst_rtsp_message_unset (&request);
    gst_rtsp_message_unset (&response);
    if(wfd_res != GST_WFD_OK) {
      GST_ERROR_OBJECT(src, "Message config error : %d", wfd_res);
      return GST_RTSP_ERROR;
    }
    return res;
 }
}

static gboolean
gst_wfd_base_src_handle_src_event (GstPad * pad, GstObject *parent, GstEvent * event)
{
  GstWFDBaseSrc *src;
  gboolean res = TRUE;
  gboolean forward = FALSE;
  const GstStructure *s;

  src = GST_WFD_BASE_SRC_CAST (parent);
  if(src == NULL)  {
    GST_ERROR_OBJECT (src, "src is NULL.");
    return FALSE;
  }

  //GST_DEBUG_OBJECT (src, "pad %s:%s received event %s",
  //    GST_DEBUG_PAD_NAME (pad), GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_QOS:
    case GST_EVENT_NAVIGATION:
    case GST_EVENT_LATENCY:
      break;
    case GST_EVENT_CUSTOM_UPSTREAM:
      s = gst_event_get_structure (event);
      if (gst_structure_has_name (s, "GstWFDIDRRequest")) {
        /* Send IDR request */
        GST_OBJECT_LOCK(src);
        src->request_param.type = WFD_IDR_REQUEST;
        GST_OBJECT_UNLOCK(src);
	 gst_wfd_base_src_loop_send_cmd(src, WFD_CMD_REQUEST, WFD_CMD_LOOP);
      }
      break;
    default:
      forward = TRUE;
      break;
  }

  if (forward) {
    GstPad *target;

    if ((target = gst_ghost_pad_get_target (GST_GHOST_PAD_CAST (pad)))) {
      res = gst_pad_send_event (target, event);
      gst_object_unref (target);
    } else {
      gst_event_unref (event);
    }
  } else {
    gst_event_unref (event);
  }

  return res;
}

/* this query is executed on the ghost source pad exposed on manager. */
static gboolean
gst_wfd_base_src_handle_src_query (GstPad * pad, GstObject *parent, GstQuery * query)
{
  GstWFDBaseSrc *src = NULL;
  gboolean res = FALSE;

  src = GST_WFD_BASE_SRC_CAST (parent);
  if(src == NULL)
  {
    GST_ERROR_OBJECT (src, "src is NULL.");
    return res;
  }

  //GST_DEBUG_OBJECT (src, "pad %s:%s received query %s",
  //    GST_DEBUG_PAD_NAME (pad), GST_QUERY_TYPE_NAME (query));

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_URI:
    {
      if (src->priv->conninfo.location == NULL) {
        res = FALSE;
      } else {
        gst_query_set_uri( query, src->priv->conninfo.location);
        res = TRUE;
      }
      break;
    }
    default:
    {
      GstPad *target = gst_ghost_pad_get_target (GST_GHOST_PAD_CAST (pad));

      /* forward the query to the proxy target pad */
      if (target) {
        res = gst_pad_query (target, query);
        gst_object_unref (target);
      }
      break;
    }
  }

  return res;
}

static void
gst_wfd_base_src_configure_caps (GstWFDBaseSrc * src)
{
  GstWFDBaseSrcPrivate *priv = src->priv;
  GstCaps *caps;
  GstStructure *structure;

  GST_DEBUG_OBJECT (src, "configuring caps");

  if ((caps = src->caps)) {
    caps = gst_caps_make_writable (caps);
    structure = gst_caps_get_structure (caps, 0);
    /* update caps */
    gst_structure_set (structure, "height", G_TYPE_INT, priv->video_height, NULL);
    gst_structure_set (structure, "width", G_TYPE_INT, priv->video_width, NULL);
    gst_structure_set (structure, "video-framerate", G_TYPE_INT, priv->video_framerate, NULL);

    src->caps = caps;
  }

  if (src->srcpad) {
    GST_DEBUG_OBJECT (src, "set caps for srcpad");
    gst_pad_set_caps(src->srcpad, src->caps);
  }
}

static gboolean
gst_wfd_base_src_push_event (GstWFDBaseSrc * src, GstEvent * event)
{
  gboolean res = TRUE;

  g_return_val_if_fail(GST_IS_EVENT(event), FALSE);

  if (src ->srcpad) {
    gst_event_ref (event);
    res = gst_pad_push_event (src ->srcpad, event);
  }

  gst_event_unref (event);

  return res;
}

static GstRTSPResult
gst_wfd_base_src_conninfo_connect (GstWFDBaseSrc * src, GstWFDConnInfo * info)
{
  GstWFDBaseSrcPrivate *priv = src->priv;
  GstRTSPResult res;

  if (info->connection == NULL) {
    if (info->url == NULL) {
      GST_DEBUG_OBJECT (src, "parsing uri (%s)...", info->location);
      if ((res = gst_rtsp_url_parse (info->location, &info->url)) < 0)
        goto parse_error;
    }

    /* create connection */
    GST_DEBUG_OBJECT (src, "creating connection (%s)...", info->location);
    if ((res = gst_rtsp_connection_create (info->url, &info->connection)) < 0)
      goto could_not_create;

    if (info->url_str)
      g_free (info->url_str);
    info->url_str = gst_rtsp_url_get_request_uri (info->url);

    GST_DEBUG_OBJECT (src, "sanitized uri %s", info->url_str);
  }

  if (!info->connected) {
    /* connect */
    GST_DEBUG_OBJECT (src, "connecting (%s)...", info->location);
    int retry = 0;
connect_retry:
    if (priv->do_stop) {
      GST_ERROR_OBJECT (src, "stop connecting....");
      return GST_RTSP_EINTR;
    }

    if ((res =
            gst_rtsp_connection_connect (info->connection,
                priv->ptcp_timeout)) < 0) {
      if (retry < priv->retry) {
        GST_ERROR_OBJECT(src, "Connection failed... Try again...");
        usleep(100000);
        retry++;
        goto connect_retry;
      } else {
        goto could_not_connect;
      }
    } else {
      GST_INFO_OBJECT(src, "Connection success");
    }

    info->connected = TRUE;
  }
  return GST_RTSP_OK;

  /* ERRORS */
parse_error:
  {
    GST_ERROR_OBJECT (src, "No valid RTSP URL was provided");
    return res;
  }
could_not_create:
  {
    gchar *str = gst_rtsp_strresult (res);
    GST_ERROR_OBJECT (src, "Could not create connection. (%s)", str);
    g_free (str);
    return res;
  }
could_not_connect:
  {
    gchar *str = gst_rtsp_strresult (res);
    GST_ERROR_OBJECT (src, "Could not connect to server. (%s)", str);
    g_free (str);
    return res;
  }
}

static GstRTSPResult
gst_wfd_base_src_conninfo_close (GstWFDBaseSrc * src, GstWFDConnInfo * info,
    gboolean free)
{
  g_return_val_if_fail (info, GST_RTSP_EINVAL);

  GST_WFD_BASE_STATE_LOCK (src);
  if (info->connected) {
    GST_DEBUG_OBJECT (src, "closing connection...");
    gst_rtsp_connection_close (info->connection);
    info->connected = FALSE;
  }
  if (free && info->connection) {
    /* free connection */
    GST_DEBUG_OBJECT (src, "freeing connection...");
    gst_rtsp_connection_free (info->connection);
    info->connection = NULL;
  }
  GST_WFD_BASE_STATE_UNLOCK (src);
  return GST_RTSP_OK;
}

static void
gst_wfd_base_src_connection_flush (GstWFDBaseSrc * src, gboolean flush)
{
  GST_DEBUG_OBJECT (src, "set flushing %d", flush);
  GST_WFD_BASE_STATE_LOCK (src);
  if (src->priv->conninfo.connection) {
    GST_DEBUG_OBJECT (src, "connection flush %d", flush);
    gst_rtsp_connection_flush (src->priv->conninfo.connection, flush);
  }
  GST_WFD_BASE_STATE_UNLOCK (src);
}

static GstRTSPResult
gst_wfd_base_src_handle_request (GstWFDBaseSrc * src, GstRTSPMessage * request)
{
  GstWFDBaseSrcPrivate *priv = src->priv;
  GstWFDBaseSrcClass *klass = NULL;
  GstRTSPMethod method = GST_RTSP_INVALID;
  GstRTSPVersion version = GST_RTSP_VERSION_INVALID;
  GstRTSPMessage response = { 0 };
  GstRTSPResult res = GST_RTSP_OK;
  GstWFDResult wfd_res = GST_WFD_OK;
  const gchar *uristr;
  guint8 *data = NULL;
  guint size = 0;
  GstWFDMessage *wfd_msg = NULL;

  klass = GST_WFD_BASE_SRC_GET_CLASS (src);

  res = gst_rtsp_message_parse_request (request, &method, &uristr, &version);
  if (res < 0)
    goto send_error;

  if (version != GST_RTSP_VERSION_1_0) {
    /* we can only handle 1.0 requests */
    res = GST_RTSP_ENOTIMPL;
    goto send_error;
  }

  GST_DEBUG_OBJECT (src, "got %s request", gst_rtsp_method_as_text(method));

  switch(method) {
    case GST_RTSP_OPTIONS:
    {
      /* Note : RTSP M1 :
       *   A WFD source shall send an RTSP M1 request to a WFD sink to begin the RTSP procedures and a WFD Capability Negotiation.
       *   A WFD sink shall respond with an RTSP M1 response message which contains an RTSP OPTIONS.
       */
      gchar *options_str = NULL;
      GstRTSPMethod options = 0;
      gchar *str = NULL;

      options = GST_RTSP_GET_PARAMETER | GST_RTSP_SET_PARAMETER | GST_RTSP_TEARDOWN | GST_RTSP_OPTIONS
      | GST_RTSP_PLAY | GST_RTSP_PAUSE | GST_RTSP_SETUP;

      str = gst_rtsp_options_as_text (options);
      if (!str) {
        GST_ERROR ("Failed to make options string, str is NULL.");
        res = GST_RTSP_ENOMEM;
        goto send_error;
      }

      options_str = g_strconcat ((const gchar*)str, ", org.wfa.wfd1.0", NULL);
      if (!options_str) {
       GST_ERROR ("Failed to make options string.");
       res = GST_RTSP_ENOMEM;
       g_free(str);
       goto send_error;
      }
      g_free(str);

      res = gst_rtsp_message_init_response (&response, GST_RTSP_STS_OK,
         gst_rtsp_status_as_text (GST_RTSP_STS_OK), request);
      if(res < 0) {
       g_free (options_str);
       goto send_error;
      }
      gst_rtsp_message_add_header (&response, GST_RTSP_HDR_PUBLIC, options_str);
      gst_rtsp_message_add_header (&response, GST_RTSP_HDR_USER_AGENT, (const gchar*)priv->user_agent);
      g_free (options_str);

      break;
    }

    case GST_RTSP_GET_PARAMETER:
    {
      gchar *rtsp_body = NULL;
      guint rtsp_body_length = 0;
      GString *rtsp_body_length_str = NULL;

      res = gst_rtsp_message_init_response (&response, GST_RTSP_STS_OK, gst_rtsp_status_as_text (GST_RTSP_STS_OK), request);
      if (res < 0)
        goto send_error;

      gst_rtsp_message_add_header (&response, GST_RTSP_HDR_CONTENT_TYPE, "text/parameters");

      res = gst_rtsp_message_get_body (request, &data, &size);
      if (res < 0)
        goto send_error;

      if (size==0) {
        /* Note : RTSP M16 : WFD keep-alive :
         *   The WFD keep-alive function is used to periodically ensure the status of WFD sesion.
         *   A WFD source indicates the timeout value via the "Session:" line in the RTSP M6 response.
         *   A WFD sink shall respond with an RTSP M16 request message upon successful receiving the RTSP M16 request message.
         */
        res = gst_rtsp_connection_reset_timeout (priv->conninfo.connection);
        if (res < 0)
          goto send_error;
        break;
      }
      wfd_res = gst_wfd_message_new (&wfd_msg);
      if (wfd_res != GST_WFD_OK) {
        GST_ERROR ("gst_wfd_message_new is failed");
        goto message_config_error;
      }

      wfd_res = gst_wfd_message_init (wfd_msg);
      if (wfd_res != GST_WFD_OK) {
        GST_ERROR ("gst_wfd_message_init is failed");
        goto message_config_error;
      }
      wfd_res = gst_wfd_message_parse_buffer (data, size, wfd_msg);
      if (wfd_res != GST_WFD_OK) {
        GST_ERROR ("gst_wfd_message_parse_buffer is failed");
        goto message_config_error;
      }
      //gst_wfd_message_dump (wfd_msg);

      if (!wfd_msg)
        goto message_config_error;

      /* Note : RTSP M3 :
       *   The WFD source shall send RTSP M3 request to the WFD sink to query the WFD sink's attributes and capabilities.
       *   A WFD sink shall respond with an RTSP M3 response message which contains the values of the requested parameters.
       *   The WFD source may query all parameters at once with a single RTSP M3 request message or may send separate RTSP M3 request message.
       *   The WFD sink shall only response with formats and settings that it can accept in the following RTSP M4 message exchnage.
       */
      /* Note : wfd-audio-codecs :
       *    The wfd-audio-codecs parameter specifies the audio formats supported in the WFD session.
       *    Valid audio codecs are LPCM, AAC, AC3.
       *    Primary sink should support one of audio codecs.
       */
      if(wfd_msg->audio_codecs) {
        guint audio_codec = 0;
        guint audio_sampling_frequency = 0;
        guint audio_channels = 0;
        guint audio_latency = 0;

        if(priv->audio_param != NULL) {
          GstStructure *audio_param = priv->audio_param;
          if (gst_structure_has_field (audio_param, "audio_codec"))
            gst_structure_get_uint (audio_param, "audio_codec", &audio_codec);
          if (gst_structure_has_field (audio_param, "audio_latency"))
            gst_structure_get_uint (audio_param, "audio_latency", &audio_latency);
          if (gst_structure_has_field (audio_param, "audio_channels"))
            gst_structure_get_uint (audio_param, "audio_channels", &audio_channels);
          if (gst_structure_has_field (audio_param, "audio_sampling_frequency"))
            gst_structure_get_uint (audio_param, "audio_sampling_frequency", &audio_sampling_frequency);
        }

        wfd_res = gst_wfd_message_set_supported_audio_format (wfd_msg,
          audio_codec,
          audio_sampling_frequency,
          audio_channels,
          16,
          audio_latency);
        if (wfd_res != GST_WFD_OK) {
          GST_ERROR ("gst_wfd_message_set_supported_audio_format is failed");
          goto message_config_error;
        }
      }

      /* Note : wfd-video-formats :
       *    The wfd-video-formats parameter specifies the supported video resolutions,
       *    H.644 codec profile, level, decoder latency,  minimum slice size, slice encoding parameters
       *      and support for video frame rate control (including explicit frame rate change and implicit video frame skipping.
       */
      if(wfd_msg->video_formats) {
        guint video_codec = 0;
        guint video_native_resolution = 0;
        guint video_cea_support = 0;
        guint video_vesa_support = 0;
        guint video_hh_support = 0;
        guint video_profile = 0;
        guint video_level = 0;
        guint video_latency = 0;
        gint video_vertical_resolution = 0;
        gint video_horizontal_resolution = 0;
        gint video_minimum_slicing = 0;
        gint video_slice_enc_param = 0;
        gint video_framerate_control_support = 0;

        if (priv->video_param != NULL) {
          GstStructure *video_param = priv->video_param;

          if (gst_structure_has_field (video_param, "video_codec"))
            gst_structure_get_uint (video_param, "video_codec", &video_codec);
          if (gst_structure_has_field (video_param, "video_native_resolution"))
            gst_structure_get_uint (video_param, "video_native_resolution", &video_native_resolution);
          if (gst_structure_has_field (video_param, "video_cea_support"))
            gst_structure_get_uint (video_param, "video_cea_support", &video_cea_support);
          if (gst_structure_has_field (video_param, "video_vesa_support"))
            gst_structure_get_uint (video_param, "video_vesa_support", &video_vesa_support);
          if (gst_structure_has_field (video_param, "video_hh_support"))
            gst_structure_get_uint (video_param, "video_hh_support", &video_hh_support);
          if (gst_structure_has_field (video_param, "video_profile"))
            gst_structure_get_uint (video_param, "video_profile", &video_profile);
          if (gst_structure_has_field (video_param, "video_level"))
            gst_structure_get_uint (video_param, "video_level", &video_level);
          if (gst_structure_has_field (video_param, "video_latency"))
            gst_structure_get_uint (video_param, "video_latency", &video_latency);
          if (gst_structure_has_field (video_param, "video_vertical_resolution"))
            gst_structure_get_int (video_param, "video_vertical_resolution", &video_vertical_resolution);
          if (gst_structure_has_field (video_param, "video_horizontal_resolution"))
            gst_structure_get_int (video_param, "video_horizontal_resolution", &video_horizontal_resolution);
          if (gst_structure_has_field (video_param, "video_minimum_slicing"))
            gst_structure_get_int (video_param, "video_minimum_slicing", &video_minimum_slicing);
          if (gst_structure_has_field (video_param, "video_slice_enc_param"))
            gst_structure_get_int (video_param, "video_slice_enc_param", &video_slice_enc_param);
          if (gst_structure_has_field (video_param, "video_framerate_control_support"))
            gst_structure_get_int (video_param, "video_framerate_control_support", &video_framerate_control_support);
        }

        wfd_res = gst_wfd_message_set_supported_video_format (wfd_msg,
            video_codec,
            GST_WFD_VIDEO_CEA_RESOLUTION,
            video_native_resolution,
            video_cea_support,
            video_vesa_support,
            video_hh_support,
            video_profile,
            video_level,
            video_latency,
            video_vertical_resolution,
            video_horizontal_resolution,
            video_minimum_slicing,
            video_slice_enc_param,
            video_framerate_control_support,
            GST_WFD_PREFERRED_DISPLAY_MODE_NOT_SUPPORTED);
        if (wfd_res != GST_WFD_OK) {
          GST_ERROR ("gst_wfd_message_set_supported_video_format is failed");
          goto message_config_error;
        }
      }

      /* Note : wfd-3d-formats :
       *    The wfd-3d-formats parameter specifies the support for stereoscopic video capabilities.
       */
      if(wfd_msg->video_3d_formats) {
        /* TODO : Set preferred video_3d_formats */
        wfd_res = GST_WFD_OK;
      }

      /* Note : wfd-content-protection :
       *   The wfd-content-protection parameter specifies whether the WFD sink supports the HDCP system 2.0/2.1 for content protection.
       */
     if(wfd_msg->content_protection) {
        gint hdcp_version = 0;
        gint hdcp_port_no = 0;

        if (priv->hdcp_param != NULL) {
          GstStructure *hdcp_param = priv->hdcp_param;

          if (gst_structure_has_field (hdcp_param, "hdcp_version"))
            gst_structure_get_int (hdcp_param, "hdcp_version", &hdcp_version);
          if (gst_structure_has_field (hdcp_param, "hdcp_port_no"))
            gst_structure_get_int (hdcp_param, "hdcp_port_no", &hdcp_port_no);
        }

        wfd_res = gst_wfd_message_set_contentprotection_type (wfd_msg,
          hdcp_version,
          (guint32)hdcp_port_no);
        if (wfd_res != GST_WFD_OK) {
          GST_ERROR ("gst_wfd_message_set_contentprotection_type is failed");
          goto message_config_error;
        }
      }

      /* Note : wfd-display-edid :
       *   The wfd-display-edid parameter specifies the EDID of the display on which the content will be rendered.
       *   EDID data comes in multiples of 128-byte blocks of EDID data depending on the EDID structure that it supports.
       *   If a WFD sink reports wfd-connector-type as HDMI or DP or UDI, the WFD sink should return the EDID of the display that renders the streamed video.
       *   The WFD sink dongle without an integrated display or with an integrated display that is not being used to render streamed video
       *     shall not set the edid filed of the wfd-display-edid paramter to "none" regardless of whether an external display devices is attached or not.
       */
      if(wfd_msg->display_edid) {
        /* TODO: Set preferred display_edid */
        wfd_res = gst_wfd_message_set_display_EDID (wfd_msg,
		      FALSE,
		      0,
		      NULL);
        if (wfd_res != GST_WFD_OK) {
          GST_ERROR ("gst_wfd_message_set_display_EDID is failed");
          goto message_config_error;
        }
      }

      /* Note : wfd-coupled-sink :
       *   The wfd-coupled-sink parameter is used by a WFD sink to convey its coupled status
       *     and if coupled to another WFD sink, the coupled WFD sink's MAC address
       */
      if(wfd_msg->coupled_sink) {
        /* To test with dummy coupled sink address */
        wfd_res = gst_wfd_message_set_coupled_sink (wfd_msg,
          GST_WFD_SINK_COUPLED,
          (gchar *)"1.0.0.1:435");
        if (wfd_res != GST_WFD_OK) {
          GST_ERROR ("gst_wfd_message_set_coupled_sink is failed");
          goto message_config_error;
        }
      }

      /* Note : wfd-client-rtp-ports :
       *   The wfd-coupled-sink parameter is used by a WFD sink to convey the RTP port(s) that the WFD sink is listening on
       *     and by the a WFD source to indicate how audio, video or both audio and video payload will be encapsulated in the MPEG2-TS stream
       *     transmitted from the WFD source to the WFD sink.
       */
      if(wfd_msg->client_rtp_ports) {
        /* Hardcoded as of now. This is to comply with dongle port settings.
        This should be derived from gst_wfd_base_src_alloc_udp_ports */
        priv->primary_rtpport = 19000;
        wfd_res = gst_wfd_message_set_prefered_RTP_ports (wfd_msg,
          GST_WFD_RTSP_TRANS_RTP,
          GST_WFD_RTSP_PROFILE_AVP,
          GST_WFD_RTSP_LOWER_TRANS_UDP,
          priv->primary_rtpport,
          0);
        if (wfd_res != GST_WFD_OK) {
          GST_ERROR ("gst_wfd_message_set_prefered_RTP_ports is failed");
          goto message_config_error;
        }
      }

      /* Note : wfd-I2C :
       *   The wfd-I2C parameter is used by a WFD source to inquire whether a WFD sink supports remote I2C read/write function or not.
       *   If the WFD sink supports remote I2C read/write function, it shall set the value of this parameter to the TCP port number
       *     to be used by the WFD source to exchange remote I2C read/write messaging transactions with the WFD sink.
       */
      if(wfd_msg->I2C) {
        /* TODO */
        wfd_res = GST_WFD_OK;
      }

      /* Note : wfd-connector-type :
       *   The WFD source may send wfd-connector-type parameter to inquire about the connector type that is currently active in the WFD sink.
       *   The WFD sink shall not send wfd-connector-type parameter unless the WFD source support this parameter.
       *   The WFD sink dongle that is not connected to an external display and it is not acting as a WFD sink with embedded display
       *     (to render streamed content) shall return a value of "none". Otherwise, the WFD sink shall choose a non-reserved value.
       */
      if(wfd_msg->connector_type) {
        /* TODO */
        wfd_res = GST_WFD_OK;
      }

      /* Note : wfd-standby-resume-capability :
       *   The wfd-standby-resume-capability parameter describes support of both standby control using
       *     a wfd-standby parameter and resume control using PLAY and using triggered-method setting PLAY.
       */
      if(wfd_msg->standby_resume_capability) {
        /* TODO */
        wfd_res = GST_WFD_OK;
      }

      rtsp_body = gst_wfd_message_as_text (wfd_msg);
      if (rtsp_body == NULL) {
        GST_ERROR ("gst_wfd_message_as_text is failed");
        goto message_config_error;
      }
      rtsp_body_length = strlen(rtsp_body);
      rtsp_body_length_str = g_string_new ("");
      g_string_append_printf (rtsp_body_length_str,"%d", rtsp_body_length);

      gst_rtsp_message_add_header (&response, GST_RTSP_HDR_CONTENT_LENGTH, g_string_free (rtsp_body_length_str, FALSE));

      res = gst_rtsp_message_set_body (&response, (guint8*)rtsp_body, rtsp_body_length);
      if (res < 0)
        goto send_error;

      if (klass->handle_get_parameter) {
        GST_DEBUG_OBJECT(src, "try to handle more GET_PARAMETER parameter");
        res = klass->handle_get_parameter(src, request, &response);
        if (res < 0)
          goto send_error;
      }
      break;
    }

    case GST_RTSP_SET_PARAMETER:
    {
      res = gst_rtsp_message_init_response (&response, GST_RTSP_STS_OK, gst_rtsp_status_as_text (GST_RTSP_STS_OK), request);
      if (res < 0)
        goto send_error;

      res = gst_rtsp_message_get_body (request, &data, &size);
      if (res < 0)
        goto send_error;

      wfd_res = gst_wfd_message_new (&wfd_msg);
      if (wfd_res != GST_WFD_OK)
        goto message_config_error;    
      wfd_res = gst_wfd_message_init (wfd_msg);
      if (wfd_res != GST_WFD_OK)
        goto message_config_error; 
      wfd_res = gst_wfd_message_parse_buffer (data, size, wfd_msg);
      if (wfd_res != GST_WFD_OK)
        goto message_config_error;
      //gst_wfd_message_dump (wfd_msg);

      if (!wfd_msg)
        goto message_config_error;

     /* Note : RTSP M4 :
       */
      /* Note : wfd-trigger-method :
       *   The wfd-trigger-method parameter is used by a WFD source to trigger the WFD sink to initiate an operation with the WFD source.
       */
      if (wfd_msg->trigger_method) {
        GstWFDTrigger trigger = GST_WFD_TRIGGER_UNKNOWN;

        wfd_res = gst_wfd_message_get_trigger_type (wfd_msg, &trigger);
        if (wfd_res != GST_WFD_OK)
          goto message_config_error;

        res = gst_wfd_base_src_connection_send (src, &response, NULL);
        if (res < 0)
          goto send_error;

        GST_DEBUG_OBJECT (src, "got trigger method for %s", GST_STR_NULL(wfd_msg->trigger_method->wfd_trigger_method));
        switch(trigger) {
          case GST_WFD_TRIGGER_PAUSE:
            gst_wfd_base_src_loop_send_cmd (src, WFD_CMD_PAUSE, WFD_CMD_LOOP);
            break;
          case GST_WFD_TRIGGER_PLAY:
            gst_wfd_base_src_loop_send_cmd (src, WFD_CMD_PLAY, WFD_CMD_LOOP);
            break;
          case GST_WFD_TRIGGER_TEARDOWN:
            gst_wfd_base_src_loop_send_cmd (src, WFD_CMD_CLOSE, WFD_CMD_ALL);
            break;
          case GST_WFD_TRIGGER_SETUP:
            if (!gst_wfd_base_src_setup (src))
              goto setup_failed;
            break;
          default:
            break;
        }
        goto done;
      }

      if (wfd_msg->audio_codecs || wfd_msg->video_formats || wfd_msg->video_3d_formats) {
        GstStructure *stream_info = gst_structure_new ("WFDStreamInfo", NULL, NULL);

        if(wfd_msg->audio_codecs) {
          res = gst_wfd_base_src_get_audio_parameter(src, wfd_msg);
          if(res != GST_RTSP_OK) {
            goto message_config_error;
          }

          gst_structure_set (stream_info,
              "audio_format", G_TYPE_STRING, priv->audio_format,
              "audio_channels", G_TYPE_INT, priv->audio_channels,
              "audio_rate", G_TYPE_INT, priv->audio_frequency,
              "audio_bitwidth", G_TYPE_INT, priv->audio_bitwidth/16,
              NULL);
        }

        if(wfd_msg->video_formats) {
          res = gst_wfd_base_src_get_video_parameter(src, wfd_msg);
          if(res != GST_RTSP_OK) {
            goto message_config_error;
          }

          gst_structure_set (stream_info,
              "video_format", G_TYPE_STRING, "H264",
              "video_width", G_TYPE_INT, priv->video_width,
              "video_height", G_TYPE_INT, priv->video_height,
              "video_framerate", G_TYPE_INT, priv->video_framerate,
              NULL);
        }

        if(wfd_msg->video_3d_formats) {
        /* TO DO */
        }
        g_signal_emit (src, gst_wfd_base_src_signals[SIGNAL_UPDATE_MEDIA_INFO], 0, stream_info);
      }

      /* Note : wfd-presentation-url :
       *   The wfd-presentation-url parameter describes the Universial Resource Identified (URI)
       *     to be used in the RTSP Setup (RTSP M6) request message in order to setup the WFD session from the WFD sink to the WFD source.
       */
      if(wfd_msg->presentation_url) {
        gchar *url0 = NULL, *url1 = NULL;

        wfd_res = gst_wfd_message_get_presentation_url (wfd_msg, &url0, &url1);
        if(wfd_res != GST_WFD_OK) {
          goto message_config_error;
        }

        g_free (priv->conninfo.location);
        priv->conninfo.location = g_strdup (url0);
        /* url1 is ignored as of now */
      }

      /* Note : wfd-client-rtp-ports :
       *   The wfd-coupled-sink parameter is used by a WFD sink to convey the RTP port(s) that the WFD sink is listening on
       *     and by the a WFD source to indicate how audio, video or both audio and video payload will be encapsulated in the MPEG2-TS stream
       *     transmitted from the WFD source to the WFD sink.
       */
      if(wfd_msg->client_rtp_ports) {
        GstWFDRTSPTransMode trans = GST_WFD_RTSP_TRANS_UNKNOWN;
        GstWFDRTSPProfile profile = GST_WFD_RTSP_PROFILE_UNKNOWN;
        GstWFDRTSPLowerTrans lowertrans = GST_WFD_RTSP_LOWER_TRANS_UNKNOWN;
        guint32 rtp_port0 =0, rtp_port1 =0;

        wfd_res = gst_wfd_message_get_prefered_RTP_ports (wfd_msg, &trans, &profile, &lowertrans, &rtp_port0, &rtp_port1);
        if(wfd_res != GST_WFD_OK) {
          goto message_config_error;
        }
      }

      /* Note : wfd-preferred-display-mode :
       *   The wfd-preferred-display-mode-supported field in a wfd-video-formats and/or in a wfd-3d-formats parameter in an RTSP M3 response message
       *      indicates whether a WFD sink supports the prefered display mod operation or not.
       */
      if(wfd_msg->preferred_display_mode) {
      }

      /* Note : wfd-av-format-change-timing :
       *   The wfd-av-format-change-timing parameter is used to signal the actual AV format change timing of the streaming data to the WFD sink.
       *   It shall be included in an RTSP M4 request message for WFD capability re-nogotiation after a WFD session has been established.
       */
      if (wfd_msg->av_format_change_timing) {
        guint64 pts=0LL, dts=0LL;
        gboolean need_to_flush = FALSE;

        wfd_res = gst_wfd_message_get_av_format_change_timing (wfd_msg, &pts, &dts);
        if(wfd_res != GST_WFD_OK) {
          goto message_config_error;
        }

        if (priv->state == GST_RTSP_STATE_PLAYING) {
          GST_DEBUG_OBJECT(src, "change format with PTS[%lld] and DTS[%lld]", pts, dts);

          g_signal_emit (src, gst_wfd_base_src_signals[SIGNAL_AV_FORMAT_CHANGE], 0, (gpointer)&need_to_flush);

          if (need_to_flush) {
            gst_wfd_base_src_flush(src,TRUE);
            gst_wfd_base_src_flush(src, FALSE);
          }
        }
      }

      /* Note : RTSP M10 :
       */
      /* Note : wfd-route :
       *   The wfd-route parameter provides a mechanism to specify the destination to which the audio stream is to be routed.
       */
      if(wfd_msg->route) {
        /* TO DO*/
      }

      /* Note : RTSP M12 :
       */
      /* Note : wfd-standby :
       *   The wfd-standby parameter is used to indicate that the sender is entering WFD standby mode.
       */
      if(wfd_msg->standby) {
        gboolean standby_enable = FALSE;

        wfd_res = gst_wfd_message_get_standby (wfd_msg, &standby_enable);
        if(wfd_res != GST_WFD_OK) {
          goto message_config_error;
        }

        GST_DEBUG_OBJECT (src, "wfd source is entering standby mode");
      }

      if (klass->handle_set_parameter) {
        GST_DEBUG_OBJECT(src, "try to handle more SET_PARAMETER parameter");
        res = klass->handle_set_parameter(src, request, &response);
        if (res < 0)
          goto send_error;
      }
      break;
    }

    default:
    {
      res = gst_rtsp_message_init_response (&response, GST_RTSP_STS_OK, gst_rtsp_status_as_text (GST_RTSP_STS_OK), request);
      if (res < 0)
        goto send_error;

      break;
    }
  }

  res = gst_wfd_base_src_connection_send (src, &response, priv->ptcp_timeout);
  if (res < 0)
    goto send_error;

done:
  gst_rtsp_message_unset (request);
  gst_rtsp_message_unset (&response);
  gst_wfd_message_free (wfd_msg);

  return GST_RTSP_OK;

  /* ERRORS */
setup_failed:
  {
    GST_ERROR_OBJECT(src, "Could not setup(error)");
    gst_rtsp_message_unset (request);
    gst_rtsp_message_unset (&response);
    gst_wfd_message_free (wfd_msg);
    return GST_RTSP_ERROR;
  }
message_config_error:
  {
    GST_ERROR_OBJECT(src, "Message config error (%d)", wfd_res);
    gst_rtsp_message_unset (request);
    gst_rtsp_message_unset (&response);
    gst_wfd_message_free (wfd_msg);
    return GST_RTSP_ERROR;
  }
send_error:
  {
    GST_ERROR_OBJECT(src, "Could not send message");
    gst_rtsp_message_unset (request);
    gst_rtsp_message_unset (&response);
    gst_wfd_message_free (wfd_msg);
    return res;
  }
}

static void
gst_wfd_base_src_loop_start_cmd (GstWFDBaseSrc * src, gint cmd)
{
  GST_DEBUG_OBJECT (src, "start cmd %s", _cmd_to_string(cmd));

  switch (cmd) {
    case WFD_CMD_OPEN:
      GST_ELEMENT_PROGRESS (src, START, "open", ("Opening Stream"));
      break;
    case WFD_CMD_PLAY:
      GST_ELEMENT_PROGRESS (src, START, "play", ("Sending PLAY request"));
      break;
    case WFD_CMD_PAUSE:
      GST_ELEMENT_PROGRESS (src, START, "pause", ("Sending PAUSE request"));
      break;
    case WFD_CMD_CLOSE:
      GST_ELEMENT_PROGRESS (src, START, "close", ("Closing Stream"));
      break;
    default:
      break;
  }
}

static void
gst_wfd_base_src_loop_complete_cmd (GstWFDBaseSrc * src, gint cmd)
{
  GST_DEBUG_OBJECT (src, "complete cmd %s", _cmd_to_string(cmd));

  switch (cmd) {
    case WFD_CMD_OPEN:
      GST_ELEMENT_PROGRESS (src, COMPLETE, "open", ("Opened Stream"));
      break;
    case WFD_CMD_PLAY:
      GST_ELEMENT_PROGRESS (src, COMPLETE, "play", ("Sent PLAY request"));
      break;
    case WFD_CMD_PAUSE:
      GST_ELEMENT_PROGRESS (src, COMPLETE, "pause", ("Sent PAUSE request"));
      break;
    case WFD_CMD_CLOSE:
      GST_ELEMENT_PROGRESS (src, COMPLETE, "close", ("Closed Stream"));
      break;
    default:
      break;
  }
}

static void
gst_wfd_base_src_loop_cancel_cmd (GstWFDBaseSrc * src, gint cmd)
{
  GST_DEBUG_OBJECT (src, "cancel cmd %s", _cmd_to_string(cmd));

  switch (cmd) {
    case WFD_CMD_OPEN:
      GST_ELEMENT_PROGRESS (src, CANCELED, "open", ("Open canceled"));
      break;
    case WFD_CMD_PLAY:
      GST_ELEMENT_PROGRESS (src, CANCELED, "play", ("PLAY canceled"));
      break;
    case WFD_CMD_PAUSE:
      GST_ELEMENT_PROGRESS (src, CANCELED, "pause", ("PAUSE canceled"));
      break;
    case WFD_CMD_CLOSE:
      GST_ELEMENT_PROGRESS (src, CANCELED, "close", ("Close canceled"));
      break;
    default:
      break;
  }
}

static void
gst_wfd_base_src_loop_error_cmd (GstWFDBaseSrc * src, gint cmd)
{
  GST_DEBUG_OBJECT (src, "error cmd %s", _cmd_to_string(cmd));

  switch (cmd) {
    case WFD_CMD_OPEN:
      GST_ELEMENT_PROGRESS (src, ERROR, "open", ("Open failed"));
      break;
    case WFD_CMD_PLAY:
      GST_ELEMENT_PROGRESS (src, ERROR, "play", ("PLAY failed"));
      break;
    case WFD_CMD_PAUSE:
      GST_ELEMENT_PROGRESS (src, ERROR, "pause", ("PAUSE failed"));
      break;
    case WFD_CMD_CLOSE:
      GST_ELEMENT_PROGRESS (src, ERROR, "close", ("Close failed"));
      break;
    default:
      break;
  }
}

static void
gst_wfd_base_src_loop_end_cmd (GstWFDBaseSrc * src, gint cmd, GstRTSPResult ret)
{
  GST_DEBUG_OBJECT (src, "end cmd %s", _cmd_to_string(cmd));

  if (ret == GST_RTSP_OK)
    gst_wfd_base_src_loop_complete_cmd (src, cmd);
  else if (ret == GST_RTSP_EINTR)
    gst_wfd_base_src_loop_cancel_cmd (src, cmd);
  else
    gst_wfd_base_src_loop_error_cmd (src, cmd);
}

static gboolean
gst_wfd_base_src_loop_send_cmd (GstWFDBaseSrc * src, gint cmd, gint mask)
{
  GstWFDBaseSrcPrivate *priv = src->priv;
  gboolean flushed = FALSE;
  gint old;

  /* start new request */
  gst_wfd_base_src_loop_start_cmd (src, cmd);

  GST_DEBUG_OBJECT (src, "sending cmd %s", _cmd_to_string(cmd));

  GST_OBJECT_LOCK (src);
  old = priv->pending_cmd;
  if (old != WFD_CMD_WAIT) {
    priv->pending_cmd = WFD_CMD_WAIT;
    GST_OBJECT_UNLOCK (src);
    /* cancel previous request */
    GST_DEBUG_OBJECT (src, "cancel previous request %s", _cmd_to_string (old));
    gst_wfd_base_src_loop_cancel_cmd (src, old);
    GST_OBJECT_LOCK (src);
  }
  priv->pending_cmd = cmd;
  /* interrupt if allowed */
  if (priv->busy_cmd & mask) {
    GST_DEBUG_OBJECT (src, "connection flush busy %s",
        _cmd_to_string (priv->busy_cmd));
    gst_wfd_base_src_connection_flush (src, TRUE);
    flushed = TRUE;
  } else {
    GST_DEBUG_OBJECT (src, "not interrupting busy cmd %s",
        _cmd_to_string (priv->busy_cmd));
  }

  if (priv->task)
    gst_task_start (priv->task);
  GST_OBJECT_UNLOCK (src);

  return flushed;
}

static GstRTSPResult
gst_wfd_base_src_loop (GstWFDBaseSrc * src)
{
  GstWFDBaseSrcPrivate *priv = src->priv;
  GstRTSPResult res = GST_RTSP_OK;
  GstRTSPMessage message = { 0 };

  if (!priv->conninfo.connection || !priv->conninfo.connected)
    goto no_connection;

  while (TRUE) {
    GTimeVal tv_timeout;

    /* get the next timeout interval */
    gst_rtsp_connection_next_timeout (priv->conninfo.connection, &tv_timeout);

    GST_DEBUG_OBJECT (src, "doing receive with timeout %d seconds",
        (gint) tv_timeout.tv_sec);

    gst_rtsp_message_unset (&message);

    /* we should continue reading the TCP socket because the server might
     * send us requests. */
    res = gst_wfd_base_src_connection_receive (src, &message, &tv_timeout);

    switch (res) {
      case GST_RTSP_OK:
        GST_DEBUG_OBJECT (src, "we received a server message");
        break;
      case GST_RTSP_EINTR:
        /* we got interrupted, see what we have to do */
        goto interrupt;
      case GST_RTSP_ETIMEOUT:
        /* timeout */
        break;
      case GST_RTSP_EEOF:
        /* server closed the connection.*/
        GST_ELEMENT_WARNING (src, RESOURCE, READ, (NULL),
            ("The server closed the connection."));
        goto connect_error;
      default:
        goto receive_error;
    }

    switch (message.type) {
      case GST_RTSP_MESSAGE_REQUEST:
        /* server sends us a request message, handle it */
        res = gst_wfd_base_src_handle_request(src, &message);
        if (res == GST_RTSP_EEOF)
          goto server_eof;
        else if (res < 0)
          goto handle_request_failed;
        break;
      case GST_RTSP_MESSAGE_RESPONSE:
        /* we ignore response and data messages */
        GST_DEBUG_OBJECT (src, "ignoring response message");
        break;
      case GST_RTSP_MESSAGE_DATA:
        /* we ignore response and data messages */
        GST_DEBUG_OBJECT (src, "ignoring data message");
        break;
      default:
        GST_WARNING_OBJECT (src, "ignoring unknown message type %d",
            message.type);
        break;
    }
  }
  g_assert_not_reached ();

  return res;

no_connection:
  {
    res = GST_RTSP_ERROR;
    GST_ERROR_OBJECT (src, "we are not connected");
    goto pause;
  }
interrupt:
  {
    /* we get here when the connection got interrupted */
    gst_rtsp_message_unset (&message);
    GST_DEBUG_OBJECT (src, "got interrupted");
    goto pause;
  }
connect_error:
  {
    gchar *str = gst_rtsp_strresult (res);

    priv->conninfo.connected = FALSE;
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ_WRITE, (NULL),
        ("Could not connect to server. (%s)", str));
    g_free (str);

    goto pause;
  }
receive_error:
  {
    gchar *str = gst_rtsp_strresult (res);

    GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL),
        ("Could not receive message. (%s)", str));
    g_free (str);

    goto pause;
  }
handle_request_failed:
  {
    gchar *str = gst_rtsp_strresult (res);

    gst_rtsp_message_unset (&message);
    if (res != GST_RTSP_EINTR)
      GST_ELEMENT_ERROR (src, RESOURCE, WRITE, (NULL),
          ("Could not handle server message. (%s)", str));
    g_free (str);

    goto pause;
  }
server_eof:
  {
    GST_DEBUG_OBJECT (src, "we got an eof from the server");
    GST_ELEMENT_WARNING (src, RESOURCE, READ, (NULL),
        ("The server closed the connection."));
    priv->conninfo.connected = FALSE;
    gst_rtsp_message_unset (&message);

    goto pause;
  }
pause:
  {
    gchar *str = gst_rtsp_strresult (res);
    GstWFDBaseSrcClass *klass = GST_WFD_BASE_SRC_GET_CLASS (src);

    if (res == GST_RTSP_EEOF) {
      /* perform EOS logic */
      if (klass->push_event)
        klass->push_event(src, gst_event_new_eos ());
    } else if (res == GST_RTSP_EINTR) {
      GST_DEBUG_OBJECT (src, "interupted");
    } else if (res < GST_RTSP_OK) {
      /* for fatal errors we post an error message, post the error before the
       * EOS so the app knows about the error first. */
      GST_ELEMENT_ERROR (src, STREAM, FAILED,
          ("Internal data flow error."),
          ("streaming task paused, reason %s (%d)", str, res));
      if (klass->push_event)
        klass->push_event(src, gst_event_new_eos ());
    }

    if (res != GST_RTSP_EINTR) {
      GST_DEBUG_OBJECT (src, "pausing task, reason %s", str);
      gst_wfd_base_src_loop_send_cmd (src, WFD_CMD_WAIT, WFD_CMD_LOOP);
    }

    g_free (str);

    return res;
  }
}

static GstRTSPResult
gst_wfd_base_src_try_send (GstWFDBaseSrc * src, GstRTSPMessage * request,
    GstRTSPMessage * response, GstRTSPStatusCode * code)
{
  GstWFDBaseSrcPrivate *priv = src->priv;
  GstRTSPResult res = GST_RTSP_OK;
  GstRTSPStatusCode thecode = GST_RTSP_STS_OK;

  res = gst_wfd_base_src_connection_send (src, request, priv->ptcp_timeout);
  if (res < 0)
    goto send_error;

  gst_rtsp_connection_reset_timeout (priv->conninfo.connection);

next:
  res = gst_wfd_base_src_connection_receive (src, response, priv->ptcp_timeout);
  if (res < 0)
    goto receive_error;

  switch (response->type) {
    case GST_RTSP_MESSAGE_REQUEST:
      res = gst_wfd_base_src_handle_request(src, response);
      if (res == GST_RTSP_EEOF)
        goto server_eof;
      else if (res < 0)
        goto handle_request_failed;
      goto next;
    case GST_RTSP_MESSAGE_RESPONSE:
      /* ok, a response is good */
      GST_DEBUG_OBJECT (src, "received response message");
      break;
    default:
    case GST_RTSP_MESSAGE_DATA:
      /* get next response */
      GST_DEBUG_OBJECT (src, "ignoring data response message");
      goto next;
  }

  thecode = response->type_data.response.code;

  GST_DEBUG_OBJECT (src, "got response message %d", thecode);

  /* if the caller wanted the result code, we store it. */
  if (code)
    *code = thecode;

  /* If the request didn't succeed, bail out before doing any more */
  if (thecode != GST_RTSP_STS_OK)
    return GST_RTSP_OK;

  return GST_RTSP_OK;

  /* ERRORS */
send_error:
  {
    gchar *str = gst_rtsp_strresult (res);

    GST_ELEMENT_ERROR (src, RESOURCE, WRITE, (NULL),
        ("Could not send message. (%s)", str));
    g_free (str);
    return res;
  }
receive_error:
  {
    gchar *str = gst_rtsp_strresult (res);

    GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL),
        ("Could not receive message. (%s)", str));
    g_free (str);
    return res;
  }
handle_request_failed:
  {
    /* ERROR was posted */
    gst_rtsp_message_unset (response);
    return res;
  }
server_eof:
  {
    GST_DEBUG_OBJECT (src, "we got an eof from the server");
    GST_ELEMENT_WARNING (src, RESOURCE, READ, (NULL),
        ("The server closed the connection."));
    gst_rtsp_message_unset (response);
    return res;
  }
}

/**
 * gst_wfd_base_src_send:
 * @src: the rtsp source
 * @conn: the connection to send on
 * @request: must point to a valid request
 * @response: must point to an empty #GstRTSPMessage
 * @code: an optional code result
 *
 * send @request and retrieve the response in @response. optionally @code can be
 * non-NULL in which case it will contain the status code of the response.
 *
 * If This function returns #GST_RTSP_OK, @response will contain a valid response
 * message that should be cleaned with gst_rtsp_message_unset() after usage.
 *
 * If @code is NULL, this function will return #GST_RTSP_ERROR (with an invalid
 * @response message) if the response code was not 200 (OK).
 *
 * Returns: #GST_RTSP_OK if the processing was successful.
 */

static GstRTSPResult
gst_wfd_base_src_send (GstWFDBaseSrc * src, GstRTSPMessage * request,
    GstRTSPMessage * response, GstRTSPStatusCode * code)
{
  GstRTSPStatusCode int_code = GST_RTSP_STS_OK;
  GstRTSPResult res = GST_RTSP_ERROR;
  GstRTSPMethod method = GST_RTSP_INVALID;

  /* save method so we can disable it when the server complains */
  method = request->type_data.request.method;

  if ((res =
          gst_wfd_base_src_try_send (src, request, response, &int_code)) < 0)
    goto error;

  /* If the user requested the code, let them handle errors, otherwise
   * post an error below */
  if (code != NULL)
    *code = int_code;
  else if (int_code != GST_RTSP_STS_OK)
    goto error_response;

  return res;

  /* ERRORS */
error:
  {
    GST_DEBUG_OBJECT (src, "got error %d", res);
    return res;
  }
error_response:
  {
    res = GST_RTSP_ERROR;

    switch (response->type_data.response.code) {
      case GST_RTSP_STS_NOT_FOUND:
        GST_ELEMENT_ERROR (src, RESOURCE, NOT_FOUND, (NULL), ("%s",
                response->type_data.response.reason));
        break;
      case GST_RTSP_STS_NOT_ACCEPTABLE:
      case GST_RTSP_STS_NOT_IMPLEMENTED:
      case GST_RTSP_STS_METHOD_NOT_ALLOWED:
        GST_ERROR_OBJECT (src, "got NOT IMPLEMENTED, disable method %s",
            gst_rtsp_method_as_text (method));
        src->priv->methods &= ~method;
        res = GST_RTSP_OK;
        break;
      default:
        GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL),
            ("Got error response: %d (%s).", response->type_data.response.code,
                response->type_data.response.reason));
        break;
    }
    /* if we return ERROR we should unset the response ourselves */
    if (res == GST_RTSP_ERROR)
      gst_rtsp_message_unset (response);

    return res;
  }
}


/* parse the response and collect all the supported methods. We need this
 * information so that we don't try to send an unsupported request to the
 * server.
 */
static gboolean
gst_wfd_base_src_parse_methods (GstWFDBaseSrc * src, GstRTSPMessage * response)
{
  GstWFDBaseSrcPrivate *priv = src->priv;
  GstRTSPHeaderField field;
  gchar *respoptions;
  gchar **options;
  gint indx = 0;
  gint i;

  /* reset supported methods */
  priv->methods = 0;

  /* Try Allow Header first */
  field = GST_RTSP_HDR_ALLOW;
  while (TRUE) {
    respoptions = NULL;
    gst_rtsp_message_get_header (response, field, &respoptions, indx);
    if (indx == 0 && !respoptions) {
      /* if no Allow header was found then try the Public header... */
      field = GST_RTSP_HDR_PUBLIC;
      gst_rtsp_message_get_header (response, field, &respoptions, indx);
    }
    if (!respoptions)
      break;

    /* If we get here, the server gave a list of supported methods, parse
     * them here. The string is like:
     *
     * OPTIONS, DESCRIBE, ANNOUNCE, PLAY, SETUP, ...
     */
    options = g_strsplit (respoptions, ",", 0);

    for (i = 0; options[i]; i++) {
      gchar *stripped;
      gint method;

      stripped = g_strstrip (options[i]);
      method = gst_rtsp_find_method (stripped);

      /* keep bitfield of supported methods */
      if (method != GST_RTSP_INVALID)
        priv->methods |= method;
    }
    g_strfreev (options);

    indx++;
  }

  if (priv->methods == 0) {
    /* neither Allow nor Public are required, assume the server supports
     * at least DESCRIBE, SETUP, we always assume it supports PLAY as
     * well. */
    GST_DEBUG_OBJECT (src, "could not get OPTIONS");
    priv->methods = GST_RTSP_SETUP;
  }
  /* always assume PLAY, FIXME, extensions should be able to override
   * this */
  priv->methods |= GST_RTSP_PLAY;

  if (!(priv->methods & GST_RTSP_SETUP))
    goto no_setup;

  return TRUE;

no_setup:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL),
        ("Server does not support SETUP."));
    return FALSE;
  }
}

static GstRTSPResult
gst_wfd_base_src_create_transports_string (GstWFDBaseSrc * src, gchar ** transports)
{
  GString *result;

  *transports = NULL;

  GST_DEBUG_OBJECT (src, "got transports %s", GST_STR_NULL (*transports));

  /* extension listed transports, use those */
  if (*transports != NULL)
    return GST_RTSP_OK;

  /* the default RTSP transports */
  result = g_string_new ("");
  GST_DEBUG_OBJECT (src, "adding UDP unicast");

  g_string_append (result, "RTP/AVP");
  g_string_append (result, "/UDP");
  g_string_append (result, ";unicast;client_port=");
  g_string_append_printf (result, "%d", src->priv->primary_rtpport);
  g_string_append (result, "-");
  g_string_append_printf (result, "%d", src->priv->primary_rtpport+1);

  *transports = g_string_free (result, FALSE);

  GST_DEBUG_OBJECT (src, "prepared transports %s", GST_STR_NULL (*transports));

  return GST_RTSP_OK;
}

gboolean
gst_wfd_base_src_activate (GstWFDBaseSrc *src)
{
  GST_DEBUG_OBJECT (src, "activating streams");

  if (src->srcpad) {
    GST_DEBUG_OBJECT (src, "setting caps");
    gst_pad_set_caps (src->srcpad, src->caps);

    GST_DEBUG_OBJECT (src, "activating srcpad");
    gst_pad_set_active (src->srcpad, TRUE);
  }

  return TRUE;
}

void
gst_wfd_base_src_get_transport_info (GstWFDBaseSrc *src,
    GstRTSPTransport * transport, const gchar ** destination, gint * min, gint * max)
{
  g_return_if_fail (transport);
  g_return_if_fail (transport->lower_transport == GST_RTSP_LOWER_TRANS_UDP);

  if (destination) {
    /* first take the source, then the endpoint to figure out where to send
     * the RTCP. */
    if (!(*destination = transport->source)) {
      if (src->priv->conninfo.connection)
        *destination =
            gst_rtsp_connection_get_ip (src->priv->conninfo.connection);
    }
  }
  if (min && max) {
    /* for unicast we only expect the ports here */
    *min = transport->server_port.min;
    *max = transport->server_port.max;
  }
}

/* Perform the SETUP request for all the streams.
 *
 * We ask the server for a specific transport, which initially includes all the
 * ones we can support (UDP/TCP/MULTICAST). For the UDP transport we allocate
 * two local UDP ports that we send to the server.
 *
 * Once the server replied with a transport, we configure the other streams
 * with the same transport.
 *
 * This function will also configure the manager for the selected transport,
 * which basically means creating the pipeline.
 */
static gboolean
gst_wfd_base_src_setup (GstWFDBaseSrc * src)
{
  GstWFDBaseSrcPrivate *priv = src->priv;
  GstWFDBaseSrcClass * klass;
  GstRTSPResult res = GST_RTSP_OK;
  GstRTSPMessage request = { 0 };
  GstRTSPMessage response = { 0 };
  GstRTSPLowerTrans protocols = GST_RTSP_LOWER_TRANS_UNKNOWN;
  GstRTSPStatusCode code = GST_RTSP_STS_OK;
  gchar *resptrans = NULL;
  GstRTSPTransport transport = { 0 };
  gchar *transports = NULL;
  GstRTSPUrl *url;
  gchar *hval;

  klass = GST_WFD_BASE_SRC_GET_CLASS (src);

  if (G_UNLIKELY (!klass->prepare_transport))
    goto no_function;
  if (G_UNLIKELY (!klass->configure_transport))
    goto no_function;

  if (!priv->conninfo.connection || !priv->conninfo.connected)
    goto no_connection;

  url = gst_rtsp_connection_get_url (priv->conninfo.connection);
  protocols = url->transports & GST_RTSP_LOWER_TRANS_UDP;
  if (protocols == 0)
    goto no_protocols;

  GST_DEBUG_OBJECT (src, "doing setup of with %s", GST_STR_NULL(priv->conninfo.location));

  GST_DEBUG_OBJECT (src, "protocols = 0x%x", protocols);
  /* create a string with first transport in line */
  res = gst_wfd_base_src_create_transports_string (src, &transports);
  if (res < 0 || transports == NULL)
    goto setup_transport_failed;

  if (strlen (transports) == 0) {
    g_free (transports);
    GST_DEBUG_OBJECT (src, "no transports found");
    goto setup_transport_failed;
  }

  /* now prepare with the selected transport */
  res = klass->prepare_transport(src, priv->primary_rtpport, priv->primary_rtpport+1);
  if (res < 0)
    goto setup_failed;

  /* create SETUP request */
  res =
      gst_rtsp_message_init_request (&request, GST_RTSP_SETUP, priv->conninfo.location);
  if (res < 0) {
    g_free (transports);
    goto create_request_failed;
  }

  gst_rtsp_message_add_header (&request, GST_RTSP_HDR_TRANSPORT, transports);
  g_free (transports);

  /* if the user wants a non default RTP packet size we add the blocksize
   * parameter */
  if (priv->rtp_blocksize > 0) {
    hval = g_strdup_printf ("%d", priv->rtp_blocksize);
    gst_rtsp_message_add_header (&request, GST_RTSP_HDR_BLOCKSIZE, hval);
    g_free (hval);
  }

  /* handle the code ourselves */
  if ((res = gst_wfd_base_src_send (src, &request, &response, &code) < 0))
    goto send_error;

  switch (code) {
    case GST_RTSP_STS_OK:
      break;
    case GST_RTSP_STS_UNSUPPORTED_TRANSPORT:
      gst_rtsp_message_unset (&request);
      gst_rtsp_message_unset (&response);
      goto no_protocols;
      break;
    default:
      goto response_error;
  }

  /* parse response transport */
  gst_rtsp_message_get_header (&response, GST_RTSP_HDR_TRANSPORT,
      &resptrans, 0);
  if (!resptrans)
    goto no_transport;

  /* parse transport, go to next manager on parse error */
  if (gst_rtsp_transport_parse (resptrans, &transport) != GST_RTSP_OK) {
    GST_ERROR_OBJECT (src, "failed to parse transport %s", resptrans);
    goto setup_failed;
  }

  /* now configure the manager with the selected transport */
  res = klass->configure_transport(src, &transport);
  if (res < 0)
    goto setup_failed;

  priv->protocol = transport.lower_transport;

  /* clean up our transport struct */
  gst_rtsp_transport_init (&transport);
  /* clean up used RTSP messages */
  gst_rtsp_message_unset (&request);
  gst_rtsp_message_unset (&response);

  priv->state = GST_RTSP_STATE_READY;

  return TRUE;

  /* ERRORS */
no_function:
  {
    GST_ELEMENT_ERROR (src, LIBRARY, INIT, (NULL),
        ("No prepare or configure function."));
    return FALSE;
  }
no_connection:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL),
        ("Could not connct to server, no connection."));
    return FALSE;
  }
no_protocols:
  {
    /* no transport possible, post an error and stop */
    GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL),
        ("Could not connect to server, no protocols left"));
    return FALSE;
  }
create_request_failed:
  {
    gchar *str = gst_rtsp_strresult (res);

    GST_ELEMENT_ERROR (src, LIBRARY, INIT, (NULL),
        ("Could not create request. (%s)", str));
    g_free (str);
    goto cleanup_error;
  }
setup_transport_failed:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, SETTINGS, (NULL),
        ("Could not setup transport."));
    goto cleanup_error;
  }
response_error:
  {
    const gchar *str = gst_rtsp_status_as_text (code);

    GST_ELEMENT_ERROR (src, RESOURCE, WRITE, (NULL),
        ("Error (%d): %s", code, GST_STR_NULL (str)));
    goto cleanup_error;
  }
send_error:
  {
    gchar *str = gst_rtsp_strresult (res);

    GST_ELEMENT_ERROR (src, RESOURCE, WRITE, (NULL),
        ("Could not send message. (%s)", str));
    g_free (str);
    goto cleanup_error;
  }
no_transport:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, SETTINGS, (NULL),
        ("Server did not select transport."));
    goto cleanup_error;
  }
setup_failed:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, SETTINGS, (NULL),
        ("Could not setup."));
    goto cleanup_error;
  }
cleanup_error:
  {
    gst_rtsp_message_unset (&request);
    gst_rtsp_message_unset (&response);
    return FALSE;
  }
}

/* Note : RTSP M1~M6 :
 *   WFD session capability negotiation
 */
static GstRTSPResult
gst_wfd_base_src_open (GstWFDBaseSrc * src)
{
  GstWFDBaseSrcPrivate *priv = src->priv;
  GstRTSPResult res = GST_RTSP_OK;
  GstRTSPMessage request = { 0 };
  GstRTSPMessage response = { 0 };
  GstRTSPMessage message = { 0 };
  GstRTSPMethod method = { 0 };
  gchar *str = NULL;

  /* can't continue without a valid url */
  if (G_UNLIKELY (priv->conninfo.url == NULL))
    goto no_url;

  if ((res = gst_wfd_base_src_conninfo_connect (src, &priv->conninfo)) < 0)
    goto connect_failed;

  res = gst_wfd_base_src_connection_receive (src, &message, priv->ptcp_timeout);
  if(res != GST_RTSP_OK)
    goto connect_failed;

  if(message.type == GST_RTSP_MESSAGE_REQUEST) {
    method = message.type_data.request.method;
    if(method == GST_RTSP_OPTIONS) {
      res = gst_wfd_base_src_handle_request(src, &message);
      if (res < GST_RTSP_OK)
        goto connect_failed;
    } else
      goto methods_error;
  } else {
    GST_ERROR_OBJECT (src, "failed to receive options...");
    goto methods_error;
  }

  /* create OPTIONS */
  GST_DEBUG_OBJECT (src, "create options...");
  res = gst_rtsp_message_init_request (&request, GST_RTSP_OPTIONS,"*");
  if (res < 0)
    goto create_request_failed;

  gst_rtsp_message_add_header (&request, GST_RTSP_HDR_REQUIRE, "org.wfa.wfd1.0");

  /* send OPTIONS */
  GST_DEBUG_OBJECT (src, "send options...");
  if ((res = gst_wfd_base_src_send (src, &request, &response,
          NULL)) < 0)
    goto send_error;

  /* parse OPTIONS */
  if (!gst_wfd_base_src_parse_methods (src, &response))
    goto methods_error;

 /* Receive request message from source */
receive_request_message:
  res = gst_wfd_base_src_connection_receive (src, &message,
    priv->ptcp_timeout);
  if(res != GST_RTSP_OK)
    goto connect_failed;

  if(message.type == GST_RTSP_MESSAGE_REQUEST) {
    method = message.type_data.request.method;
    if(method == GST_RTSP_GET_PARAMETER ||method == GST_RTSP_SET_PARAMETER) {
      res = gst_wfd_base_src_handle_request(src, &message);
      if (res < GST_RTSP_OK)
        goto handle_request_failed;
    } else
      goto methods_error;

    if (priv->state != GST_RTSP_STATE_READY)
      goto receive_request_message;
  }

  /* clean up any messages */
  gst_rtsp_message_unset (&request);
  gst_rtsp_message_unset (&response);

  return res;

  /* ERRORS */
no_url:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, NOT_FOUND, (NULL),
        ("No valid RTSP URL was provided"));
    goto cleanup_error;
  }
connect_failed:
  {
    str = NULL;
    str = gst_rtsp_strresult (res);

    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ_WRITE, (NULL),
        ("Failed to connect. (%s)", str));
    g_free (str);
    goto cleanup_error;
  }
create_request_failed:
  {
    str = NULL;
    str = gst_rtsp_strresult (res);

    GST_ELEMENT_ERROR (src, LIBRARY, INIT, (NULL),
        ("Could not create request. (%s)", str));
    g_free (str);
    goto cleanup_error;
  }
send_error:
  {
    GST_ERROR_OBJECT (src, "send errors");
    /* Don't post a message - the rtsp_send method will have
     * taken care of it because we passed NULL for the response code */
    goto cleanup_error;
  }
methods_error:
  {
    GST_ERROR_OBJECT (src, "method errors");
    /* error was posted */
    goto cleanup_error;
  }
handle_request_failed:
  {
    GST_ERROR_OBJECT (src, "failed to handle request");
    /* error was posted */
    goto cleanup_error;
  }
cleanup_error:
  {
    GST_ERROR_OBJECT (src, "failed to open");

    if (priv->conninfo.connection) {
      GST_ERROR_OBJECT (src, "free connection");
      gst_wfd_base_src_conninfo_close (src, &priv->conninfo, TRUE);
    }
    gst_rtsp_message_unset (&request);
    gst_rtsp_message_unset (&response);
    return res;
  }
}

static void
gst_wfd_base_src_send_close_cmd (GstWFDBaseSrc * src)
{
  gst_wfd_base_src_loop_send_cmd (src, WFD_CMD_CLOSE, WFD_CMD_ALL);
}

/* Note : RTSP M8 :
 *   Send TEARDOWN request to WFD source.
 */
static GstRTSPResult
gst_wfd_base_src_close (GstWFDBaseSrc * src, gboolean only_close)
{
  GstWFDBaseSrcPrivate *priv = src->priv;
  GstWFDBaseSrcClass *klass;
  GstRTSPMessage request = { 0 };
  GstRTSPMessage response = { 0 };
  GstRTSPResult res = GST_RTSP_OK;

  GST_DEBUG_OBJECT (src, "TEARDOWN...");

  klass = GST_WFD_BASE_SRC_GET_CLASS (src);

  if (klass->set_state)
    klass->set_state (src, GST_STATE_READY);

  if (priv->state < GST_RTSP_STATE_READY) {
    GST_DEBUG_OBJECT (src, "not ready, doing cleanup");
    goto close;
  }

  if (only_close)
    goto close;

  if (!priv->conninfo.connection || !priv->conninfo.connected)
    goto close;

  if (!(priv->methods & (GST_RTSP_PLAY | GST_RTSP_TEARDOWN)))
    goto not_supported;

  /* do TEARDOWN */
  res =
      gst_rtsp_message_init_request (&request, GST_RTSP_TEARDOWN, priv->conninfo.url_str);
  if (res < 0)
    goto create_request_failed;

  if ((res =
          gst_wfd_base_src_send (src, &request, &response,
              NULL)) < 0)
    goto send_error;

  /* FIXME, parse result? */
  gst_rtsp_message_unset (&request);
  gst_rtsp_message_unset (&response);

close:
  /* close connections */
  GST_DEBUG_OBJECT (src, "closing connection...");
  gst_wfd_base_src_conninfo_close (src, &priv->conninfo, TRUE);

  priv->state = GST_RTSP_STATE_INVALID;

  return res;

  /* ERRORS */
create_request_failed:
  {
    gchar *str = gst_rtsp_strresult (res);

    GST_ELEMENT_ERROR (src, LIBRARY, INIT, (NULL),
        ("Could not create request. (%s)", str));
    g_free (str);
    goto close;
  }
send_error:
  {
    gchar *str = gst_rtsp_strresult (res);

    gst_rtsp_message_unset (&request);
    if (res != GST_RTSP_EINTR) {
      GST_ELEMENT_ERROR (src, RESOURCE, WRITE, (NULL),
          ("Could not send message. (%s)", str));
    } else {
      GST_ERROR_OBJECT (src, "TEARDOWN interrupted");
    }
    g_free (str);
    goto close;
  }
not_supported:
  {
    GST_ERROR_OBJECT (src,
        "TEARDOWN and PLAY not supported, can't do TEARDOWN");
    goto close;
  }
}

/* RTP-Info is of the format:
 *
 * url=<URL>;[seq=<seqbase>;rtptime=<timebase>] [, url=...]
 *
 * rtptime corresponds to the timestamp for the NPT time given in the header
 * seqbase corresponds to the next sequence number we received. This number
 * indicates the first seqnum after the seek and should be used to discard
 * packets that are from before the seek.
 */
static gboolean
gst_wfd_base_src_parse_rtpinfo (GstWFDBaseSrc * src, gchar * rtpinfo)
{
  gchar **infos;
  gint i, j;

  GST_DEBUG_OBJECT (src, "parsing RTP-Info %s", rtpinfo);

  infos = g_strsplit (rtpinfo, ",", 0);
  for (i = 0; infos[i]; i++) {
    gchar **fields;
    gint64 seqbase;
    gint64 timebase;

    GST_DEBUG_OBJECT (src, "parsing info %s", infos[i]);

    /* init values, types of seqbase and timebase are bigger than needed so we
     * can store -1 as uninitialized values */
    seqbase = GST_CLOCK_TIME_NONE;
    timebase = GST_CLOCK_TIME_NONE;

    /* parse url, find manager for url.
     * parse seq and rtptime. The seq number should be configured in the rtp
     * depayloader or session manager to detect gaps. Same for the rtptime, it
     * should be used to create an initial time newsegment. */
    fields = g_strsplit (infos[i], ";", 0);
    for (j = 0; fields[j]; j++) {
      GST_DEBUG_OBJECT (src, "parsing field %s", fields[j]);
      /* remove leading whitespace */
      fields[j] = g_strchug (fields[j]);
      if (g_str_has_prefix (fields[j], "url=")) {
        /* get the url and the manager */
      } else if (g_str_has_prefix (fields[j], "seq=")) {
        seqbase = atoi (fields[j] + 4);
      } else if (g_str_has_prefix (fields[j], "rtptime=")) {
        timebase = g_ascii_strtoll (fields[j] + 8, NULL, 10);
      }
    }
    g_strfreev (fields);
    /* now we need to store the values for the caps of the manager */
    GST_DEBUG_OBJECT (src,
        "setting: seqbase %"G_GINT64_FORMAT", timebase %" G_GINT64_FORMAT,
         seqbase, timebase);
  }
  g_strfreev (infos);

  return TRUE;
}

static void
gst_wfd_base_src_send_play_cmd (GstWFDBaseSrc * src)
{
  gst_wfd_base_src_loop_send_cmd (src, WFD_CMD_PLAY, WFD_CMD_LOOP);
}

/* Note : RTSP M7 :
 *   Send PLAY request to WFD source. WFD source begins audio and/or video streaming.
 */
static GstRTSPResult
gst_wfd_base_src_play (GstWFDBaseSrc * src)
{
  GstWFDBaseSrcPrivate *priv = src->priv;
  GstWFDBaseSrcClass *klass;
  GstRTSPMessage request = { 0 };
  GstRTSPMessage response = { 0 };
  GstRTSPResult res = GST_RTSP_OK;
  gchar *hval;
  gint hval_idx;

  GST_DEBUG_OBJECT (src, "PLAY...");

  klass = GST_WFD_BASE_SRC_GET_CLASS (src);

  if (!priv->conninfo.connection || !priv->conninfo.connected)
    goto done;

  if (!(priv->methods & GST_RTSP_PLAY))
    goto not_supported;

  if (priv->state == GST_RTSP_STATE_PLAYING)
    goto was_playing;

  /* do play */
  res = gst_rtsp_message_init_request (&request, GST_RTSP_PLAY, priv->conninfo.url_str);
  if (res < 0)
    goto create_request_failed;

  if ((res = gst_wfd_base_src_send (src, &request, &response, NULL)) < 0)
    goto send_error;

  gst_rtsp_message_unset (&request);

  /* parse the RTP-Info header field (if ANY) to get the base seqnum and timestamp
   * for the RTP packets. If this is not present, we assume all starts from 0...
   * This is info for the RTP session manager that we pass to it in caps. */
  hval_idx = 0;
  while (gst_rtsp_message_get_header (&response, GST_RTSP_HDR_RTP_INFO,
          &hval, hval_idx++) == GST_RTSP_OK)
    gst_wfd_base_src_parse_rtpinfo (src, hval);

  gst_rtsp_message_unset (&response);

  /* configure the caps of the streams after we parsed all headers. */
  gst_wfd_base_src_configure_caps (src);

  /* set to PLAYING after we have configured the caps, otherwise we
   * might end up calling request_key (with SRTP) while caps are still
   * being configured. */
  if (klass->set_state)
    klass->set_state (src, GST_STATE_PLAYING);

  priv->state = GST_RTSP_STATE_PLAYING;

done:
  return res;

  /* ERRORS */
not_supported:
  {
    GST_ERROR_OBJECT (src, "PLAY is not supported");
    goto done;
  }
was_playing:
  {
    GST_DEBUG_OBJECT (src, "we were already PLAYING");
    goto done;
  }
create_request_failed:
  {
    GST_ELEMENT_ERROR (src, LIBRARY, INIT, (NULL),
        ("Could not create request."));
    return FALSE;
  }
send_error:
  {
    gst_rtsp_message_unset (&request);
    GST_ELEMENT_ERROR (src, RESOURCE, WRITE, (NULL),
        ("Could not send message."));
    return FALSE;
  }
}

static void
gst_wfd_base_src_send_pause_cmd (GstWFDBaseSrc * src)
{
  gst_wfd_base_src_loop_send_cmd (src, WFD_CMD_PAUSE, WFD_CMD_LOOP);
}

/* Note : RTSP M9  :
 *   Send PAUSE request to WFD source. WFD source pauses the audio video stream(s).
 */
static GstRTSPResult
gst_wfd_base_src_pause (GstWFDBaseSrc * src)
{
  GstWFDBaseSrcPrivate *priv = src->priv;
  GstRTSPResult res = GST_RTSP_OK;
  GstRTSPMessage request = { 0 };
  GstRTSPMessage response = { 0 };
  GstWFDBaseSrcClass * klass;

  GST_DEBUG_OBJECT (src, "PAUSE...");

  klass = GST_WFD_BASE_SRC_GET_CLASS (src);

  if (!priv->conninfo.connection || !priv->conninfo.connected)
    goto no_connection;

  if (!(priv->methods & GST_RTSP_PAUSE))
    goto not_supported;

  if (priv->state == GST_RTSP_STATE_READY)
    goto was_paused;

  if (gst_rtsp_message_init_request (&request, GST_RTSP_PAUSE, priv->conninfo.url_str) < 0)
    goto create_request_failed;

  if ((res = gst_wfd_base_src_send (src, &request, &response, NULL)) < 0)
    goto send_error;

  gst_rtsp_message_unset (&request);
  gst_rtsp_message_unset (&response);

  if (klass->set_state)
    klass->set_state (src, GST_STATE_PAUSED);

no_connection:
  priv->state = GST_RTSP_STATE_READY;

done:
  return res;

  /* ERRORS */
not_supported:
  {
    GST_ERROR_OBJECT (src, "PAUSE is not supported");
    goto done;
  }
was_paused:
  {
    GST_DEBUG_OBJECT (src, "we were already PAUSED");
    goto done;
  }
create_request_failed:
  {
    GST_ELEMENT_ERROR (src, LIBRARY, INIT, (NULL),
        ("Could not create request."));
    goto done;
  }
send_error:
  {
    gst_rtsp_message_unset (&request);
    GST_ELEMENT_ERROR (src, RESOURCE, WRITE, (NULL),
        ("Could not send message."));
    goto done;
  }
}

static void
gst_wfd_base_src_handle_message (GstBin * bin, GstMessage * message)
{
  GstWFDBaseSrc *src = GST_WFD_BASE_SRC (bin);

  GST_DEBUG_OBJECT (src, "got %s message from %s",
      GST_MESSAGE_TYPE_NAME (message), GST_MESSAGE_SRC_NAME (message));

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_EOS:
      gst_message_unref (message);
      break;
    case GST_MESSAGE_ELEMENT:
    {
      const GstStructure *s = gst_message_get_structure (message);
      if (gst_structure_has_name (s, "GstUDPSrcTimeout"))
        GST_DEBUG_OBJECT (bin, "timeout on UDP port");

      GST_BIN_CLASS (parent_class)->handle_message (bin, message);
      break;
    }
    case GST_MESSAGE_ERROR:
    {
      gchar* debug = NULL;
      GError* error = NULL;

      gst_message_parse_error (message, &error, &debug);
      GST_ERROR_OBJECT (src, "error : %s", error->message);
      GST_ERROR_OBJECT (src, "debug : %s", debug);
      g_free (debug);
      g_error_free (error);

      /* fatal but not our message, forward */
      GST_BIN_CLASS (parent_class)->handle_message (bin, message);
      break;
    }
    default:
    {
      GST_BIN_CLASS (parent_class)->handle_message (bin, message);
      break;
    }
  }
}

/* the thread where everything happens */
static void
gst_wfd_base_src_thread (GstWFDBaseSrc * src)
{
  GstWFDBaseSrcPrivate *priv = src->priv;
  GstRTSPResult res = GST_RTSP_OK;
  gint cmd;

  GST_OBJECT_LOCK (src);
  cmd = priv->pending_cmd;
  if (cmd == WFD_CMD_PLAY || cmd == WFD_CMD_PAUSE || cmd == WFD_CMD_REQUEST
      || cmd == WFD_CMD_LOOP || cmd == WFD_CMD_OPEN)
    priv->pending_cmd = WFD_CMD_LOOP;
  else
    priv->pending_cmd = WFD_CMD_WAIT;

  GST_DEBUG_OBJECT (src, "got command %s", _cmd_to_string (cmd));

  /* we got the message command, so ensure communication is possible again */
  gst_wfd_base_src_connection_flush (src, FALSE);

  priv->busy_cmd = cmd;
  GST_OBJECT_UNLOCK (src);

  switch (cmd) {
    case WFD_CMD_OPEN:
      res = gst_wfd_base_src_open (src);
      break;
    case WFD_CMD_PLAY:
      res = gst_wfd_base_src_play (src);
      break;
    case WFD_CMD_PAUSE:
      res = gst_wfd_base_src_pause (src);
      break;
    case WFD_CMD_CLOSE:
      res = gst_wfd_base_src_close (src, FALSE);
      break;
    case WFD_CMD_LOOP:
      res = gst_wfd_base_src_loop (src);
      break;
    case WFD_CMD_REQUEST:
      res = gst_wfd_base_src_send_request (src);
      break;
    default:
      break;
  }

  GST_OBJECT_LOCK (src);
  /* and go back to sleep */
  if (priv->pending_cmd == WFD_CMD_WAIT) {
    if (priv->task)
      gst_task_pause (priv->task);
  }
  /* reset waiting */
  priv->busy_cmd = WFD_CMD_WAIT;
  GST_OBJECT_UNLOCK (src);

  if (cmd == WFD_CMD_PLAY || cmd == WFD_CMD_PAUSE
      || cmd == WFD_CMD_CLOSE || cmd == WFD_CMD_OPEN)
    gst_wfd_base_src_loop_end_cmd (src, cmd, res);
}

static gboolean
gst_wfd_base_src_start (GstWFDBaseSrc * src)
{
  GstWFDBaseSrcPrivate *priv = src->priv;

  GST_DEBUG_OBJECT (src, "starting");

  GST_OBJECT_LOCK (src);

  priv->state = GST_RTSP_STATE_INIT;

  priv->pending_cmd = WFD_CMD_WAIT;

  if (priv->task == NULL) {
    priv->task = gst_task_new ((GstTaskFunction) gst_wfd_base_src_thread, src, NULL);
    if (priv->task == NULL)
      goto task_error;

    gst_task_set_lock (priv->task, &(GST_WFD_BASE_TASK_GET_LOCK (src)));
  }
  GST_OBJECT_UNLOCK (src);

  return TRUE;

  /* ERRORS */
task_error:
  {
    GST_OBJECT_UNLOCK (src);
    GST_ERROR_OBJECT (src, "failed to create task");
    return FALSE;
  }
}

static gboolean
gst_wfd_base_src_stop (GstWFDBaseSrc * src)
{
  GstWFDBaseSrcPrivate *priv = src->priv;
  GstTask *task;

  GST_DEBUG_OBJECT (src, "stopping");

  GST_OBJECT_LOCK (src);
  priv->do_stop = TRUE;
  GST_OBJECT_UNLOCK (src);

  /* also cancels pending task */
  gst_wfd_base_src_loop_send_cmd (src, WFD_CMD_WAIT, WFD_CMD_ALL);

  GST_OBJECT_LOCK (src);
  if ((task = priv->task)) {
    priv->task = NULL;
    GST_OBJECT_UNLOCK (src);

    gst_task_stop (task);

    /* make sure it is not running */
    GST_WFD_BASE_TASK_LOCK (src);
    GST_WFD_BASE_TASK_UNLOCK (src);

    /* now wait for the task to finish */
    gst_task_join (task);

    /* and free the task */
    gst_object_unref (GST_OBJECT (task));
    GST_OBJECT_LOCK (src);
  }
  GST_OBJECT_UNLOCK (src);

  /* ensure synchronously all is closed */
  gst_wfd_base_src_close (src, TRUE);

  /* cleanup */
  gst_wfd_base_src_cleanup (src);

  return TRUE;
}


static GstStateChangeReturn
gst_wfd_base_src_change_state (GstElement * element, GstStateChange transition)
{
  GstWFDBaseSrc *src;
  GstStateChangeReturn ret;

  src = GST_WFD_BASE_SRC (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      GST_DEBUG_OBJECT (src, "NULL->READY");
      if (!gst_wfd_base_src_start (src))
        goto start_failed;
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      GST_DEBUG_OBJECT (src, "READY->PAUSED");
      gst_wfd_base_src_loop_send_cmd (src, WFD_CMD_OPEN, 0);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      GST_DEBUG_OBJECT(src, "PAUSED->PLAYING");
      break;
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      GST_DEBUG_OBJECT(src, "PLAYING->PAUSED");
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      GST_DEBUG_OBJECT(src, "PAUSED->READY");
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      GST_DEBUG_OBJECT (src, "READY->NULL");
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    goto done;

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      gst_wfd_base_src_loop_send_cmd (src, WFD_CMD_PLAY, WFD_CMD_LOOP);
      break;
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      ret = GST_STATE_CHANGE_NO_PREROLL;
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      gst_wfd_base_src_stop (src);
      break;
    default:
      break;
  }

done:
  return ret;

start_failed:
  {
    GST_ERROR_OBJECT (src, "start failed");
    return GST_STATE_CHANGE_FAILURE;
  }
}

static gboolean
gst_wfd_base_src_send_event (GstElement * element, GstEvent * event)
{
  gboolean res = FALSE;
  GstWFDBaseSrc *src;
  GstWFDBaseSrcClass *klass;

  src = GST_WFD_BASE_SRC (element);
  klass = GST_WFD_BASE_SRC_GET_CLASS (src);

  if (GST_EVENT_IS_DOWNSTREAM (event)) {
    if (klass->push_event)
      res = klass->push_event(src, event);
  } else {
    res = GST_ELEMENT_CLASS (parent_class)->send_event (element, event);
  }

  return res;
}

static void
gst_wfd_base_src_set_standby (GstWFDBaseSrc * src)
{
  GST_OBJECT_LOCK(src);
  memset (&src->request_param, 0, sizeof(GstWFDRequestParam));
  src->request_param.type = WFD_STANDBY;
  GST_OBJECT_UNLOCK(src);

  gst_wfd_base_src_loop_send_cmd(src, WFD_CMD_REQUEST, WFD_CMD_LOOP);
}

gboolean
gst_wfd_base_src_set_target (GstWFDBaseSrc * src, GstPad *target)
{
  gboolean ret;

  ret = gst_ghost_pad_set_target (GST_GHOST_PAD (src->srcpad), target);

  return ret;
}

/*** GSTURIHANDLER INTERFACE *************************************************/
static GstURIType
gst_wfd_base_src_uri_get_type (GType type)
{
  return GST_URI_SRC;
}

static const gchar * const *
gst_wfd_base_src_uri_get_protocols (GType type)
{
  static const gchar *protocols[] =
      { "rtsp", NULL };

  return protocols;
}

static gchar *
gst_wfd_base_src_uri_get_uri (GstURIHandler * handler)
{
  GstWFDBaseSrc *src = GST_WFD_BASE_SRC (handler);

  /* should not dup */
  return src->priv->conninfo.location;
}

static gboolean
gst_wfd_base_src_uri_set_uri (GstURIHandler * handler, const gchar * uri, GError **error)
{
  GstWFDBaseSrc *src;
  GstRTSPResult res;
  GstRTSPUrl *newurl = NULL;

  src = GST_WFD_BASE_SRC (handler);

  /* same URI, we're fine */
  if (src->priv->conninfo.location && uri && !strcmp (uri, src->priv->conninfo.location))
    goto was_ok;

    /* try to parse */
    GST_DEBUG_OBJECT (src, "parsing URI");
    if ((res = gst_rtsp_url_parse (uri, &newurl)) < 0)
      goto parse_error;


  /* if worked, free previous and store new url object along with the original
   * location. */
  GST_DEBUG_OBJECT (src, "configuring URI");
  g_free (src->priv->conninfo.location);
  src->priv->conninfo.location = g_strdup (uri);
  gst_rtsp_url_free (src->priv->conninfo.url);
  src->priv->conninfo.url = newurl;
  g_free (src->priv->conninfo.url_str);
  if (newurl)
    src->priv->conninfo.url_str = gst_rtsp_url_get_request_uri (src->priv->conninfo.url);
  else
    src->priv->conninfo.url_str = NULL;

  GST_DEBUG_OBJECT (src, "set uri: %s", GST_STR_NULL (uri));
  GST_DEBUG_OBJECT (src, "request uri is: %s",
      GST_STR_NULL (src->priv->conninfo.url_str));

  return TRUE;

  /* Special cases */
was_ok:
  {
    GST_DEBUG_OBJECT (src, "URI was ok: '%s'", GST_STR_NULL (uri));
    return TRUE;
  }
parse_error:
  {
    GST_ERROR_OBJECT (src, "Not a valid RTSP url '%s' (%d)",
        GST_STR_NULL (uri), res);
    return FALSE;
  }
}

static void
gst_wfd_base_src_uri_handler_init (gpointer g_iface, gpointer iface_data)
{
  GstURIHandlerInterface *iface = (GstURIHandlerInterface *) g_iface;

  iface->get_type = gst_wfd_base_src_uri_get_type;
  iface->get_protocols = gst_wfd_base_src_uri_get_protocols;
  iface->get_uri = gst_wfd_base_src_uri_get_uri;
  iface->set_uri = gst_wfd_base_src_uri_set_uri;
}

static GstRTSPResult _get_cea_resolution_and_set_to_src(GstWFDBaseSrc *src, GstWFDVideoCEAResolution Resolution)
{
  GstWFDBaseSrcPrivate *priv = src->priv;
  GstWFDVideoCEAResolution CEARes = Resolution;

  switch(CEARes)
  {
    case GST_WFD_CEA_UNKNOWN:
      break;
    case GST_WFD_CEA_640x480P60:
      priv->video_width=640;
      priv->video_height=480;
      priv->video_framerate=60;
      break;
    case GST_WFD_CEA_720x480P60:
      priv->video_width=720;
      priv->video_height=480;
      priv->video_framerate=60;
      break;
    case GST_WFD_CEA_720x480I60:
      priv->video_width=720;
      priv->video_height=480;
      priv->video_framerate=60;
      break;
    case GST_WFD_CEA_720x576P50:
      priv->video_width=720;
      priv->video_height=576;
      priv->video_framerate=50;
      break;
    case GST_WFD_CEA_720x576I50:
       priv->video_width=720;
      priv->video_height=576;
      priv->video_framerate=50;
      break;
    case GST_WFD_CEA_1280x720P30:
      priv->video_width=1280;
      priv->video_height=720;
      priv->video_framerate=30;
      break;
    case GST_WFD_CEA_1280x720P60:
      priv->video_width=1280;
      priv->video_height=720;
      priv->video_framerate=60;
      break;
    case GST_WFD_CEA_1920x1080P30:
      priv->video_width=1920;
      priv->video_height=1080;
      priv->video_framerate=30;
      break;
    case GST_WFD_CEA_1920x1080P60:
      priv->video_width=1920;
      priv->video_height=1080;
      priv->video_framerate=60;
      break;
    case GST_WFD_CEA_1920x1080I60:
      priv->video_width=1920;
      priv->video_height=1080;
      priv->video_framerate=60;
      break;
    case GST_WFD_CEA_1280x720P25:
      priv->video_width=1280;
      priv->video_height=720;
      priv->video_framerate=25;
      break;
    case GST_WFD_CEA_1280x720P50:
      priv->video_width=1280;
      priv->video_height=720;
      priv->video_framerate=50;
      break;
    case GST_WFD_CEA_1920x1080P25:
      priv->video_width=1920;
      priv->video_height=1080;
      priv->video_framerate=25;
      break;
    case GST_WFD_CEA_1920x1080P50:
      priv->video_width=1920;
      priv->video_height=1080;
      priv->video_framerate=50;
      break;
    case GST_WFD_CEA_1920x1080I50:
      priv->video_width=1920;
      priv->video_height=1080;
      priv->video_framerate=50;
      break;
    case GST_WFD_CEA_1280x720P24:
      priv->video_width=1280;
      priv->video_height=720;
      priv->video_framerate=24;
      break;
    case GST_WFD_CEA_1920x1080P24:
      priv->video_width=1920;
      priv->video_height=1080;
      priv->video_framerate=24;
      break;
    default:
      break;
  }
  return GST_RTSP_OK;
}

static GstRTSPResult _get_vesa_resolution_and_set_to_src(GstWFDBaseSrc *src, GstWFDVideoVESAResolution Resolution)
{
  GstWFDBaseSrcPrivate *priv = src->priv;
  GstWFDVideoVESAResolution VESARes = Resolution;

  switch(VESARes)
  {
    case GST_WFD_VESA_UNKNOWN:
      break;
    case GST_WFD_VESA_800x600P30:
      priv->video_width=800;
      priv->video_height=600;
      priv->video_framerate=30;
      break;
    case GST_WFD_VESA_800x600P60:
      priv->video_width=800;
      priv->video_height=600;
      priv->video_framerate=60;
      break;
    case GST_WFD_VESA_1024x768P30:
      priv->video_width=1024;
      priv->video_height=768;
      priv->video_framerate=30;
      break;
    case GST_WFD_VESA_1024x768P60:
      priv->video_width=1024;
      priv->video_height=768;
      priv->video_framerate=60;
      break;
    case GST_WFD_VESA_1152x864P30:
      priv->video_width=1152;
      priv->video_height=864;
      priv->video_framerate=30;
      break;
    case GST_WFD_VESA_1152x864P60:
      priv->video_width=1152;
      priv->video_height=864;
      priv->video_framerate=60;
      break;
    case GST_WFD_VESA_1280x768P30:
      priv->video_width=1280;
      priv->video_height=768;
      priv->video_framerate=30;
      break;
    case GST_WFD_VESA_1280x768P60:
      priv->video_width=1280;
      priv->video_height=768;
      priv->video_framerate=60;
      break;
    case GST_WFD_VESA_1280x800P30:
      priv->video_width=1280;
      priv->video_height=800;
      priv->video_framerate=30;
      break;
    case GST_WFD_VESA_1280x800P60:
      priv->video_width=1280;
      priv->video_height=800;
      priv->video_framerate=60;
      break;
    case GST_WFD_VESA_1360x768P30:
      priv->video_width=1360;
      priv->video_height=768;
      priv->video_framerate=30;
      break;
    case GST_WFD_VESA_1360x768P60:
      priv->video_width=1360;
      priv->video_height=768;
      priv->video_framerate=60;
      break;
    case GST_WFD_VESA_1366x768P30:
      priv->video_width=1366;
      priv->video_height=768;
      priv->video_framerate=30;
      break;
    case GST_WFD_VESA_1366x768P60:
      priv->video_width=1366;
      priv->video_height=768;
      priv->video_framerate=60;
      break;
    case GST_WFD_VESA_1280x1024P30:
      priv->video_width=1280;
      priv->video_height=1024;
      priv->video_framerate=30;
      break;
    case GST_WFD_VESA_1280x1024P60:
      priv->video_width=1280;
      priv->video_height=1024;
      priv->video_framerate=60;
      break;
    case GST_WFD_VESA_1400x1050P30:
      priv->video_width=1400;
      priv->video_height=1050;
      priv->video_framerate=30;
      break;
    case GST_WFD_VESA_1400x1050P60:
      priv->video_width=1400;
      priv->video_height=1050;
      priv->video_framerate=60;
      break;
    case GST_WFD_VESA_1440x900P30:
      priv->video_width=1440;
      priv->video_height=900;
      priv->video_framerate=30;
      break;
    case GST_WFD_VESA_1440x900P60:
      priv->video_width=1440;
      priv->video_height=900;
      priv->video_framerate=60;
      break;
    case GST_WFD_VESA_1600x900P30:
      priv->video_width=1600;
      priv->video_height=900;
      priv->video_framerate=30;
      break;
    case GST_WFD_VESA_1600x900P60:
      priv->video_width=1600;
      priv->video_height=900;
      priv->video_framerate=60;
      break;
    case GST_WFD_VESA_1600x1200P30:
      priv->video_width=1600;
      priv->video_height=1200;
      priv->video_framerate=30;
      break;
    case GST_WFD_VESA_1600x1200P60:
      priv->video_width=1600;
      priv->video_height=1200;
      priv->video_framerate=60;
      break;
    case GST_WFD_VESA_1680x1024P30:
      priv->video_width=1680;
      priv->video_height=1024;
      priv->video_framerate=30;
      break;
    case GST_WFD_VESA_1680x1024P60:
      priv->video_width=1680;
      priv->video_height=1024;
      priv->video_framerate=60;
      break;
    case GST_WFD_VESA_1680x1050P30:
      priv->video_width=1680;
      priv->video_height=1050;
      priv->video_framerate=30;
      break;
    case GST_WFD_VESA_1680x1050P60:
      priv->video_width=1680;
      priv->video_height=1050;
      priv->video_framerate=60;
      break;
    case GST_WFD_VESA_1920x1200P30:
      priv->video_width=1920;
      priv->video_height=1200;
      priv->video_framerate=30;
      break;
    case GST_WFD_VESA_1920x1200P60:
      priv->video_width=1920;
      priv->video_height=1200;
      priv->video_framerate=60;
      break;
    default:
      break;
  }
  return GST_RTSP_OK;
}

static GstRTSPResult _get_hh_resolution_and_set_to_src(GstWFDBaseSrc *src, GstWFDVideoHHResolution Resolution)
{
  GstWFDBaseSrcPrivate *priv = src->priv;
  GstWFDVideoHHResolution HHRes = Resolution;

  switch(HHRes)
  {
    case GST_WFD_HH_UNKNOWN:
      break;
    case GST_WFD_HH_800x480P30:
      priv->video_width=800;
      priv->video_height=480;
      priv->video_framerate=30;
      break;
    case GST_WFD_HH_800x480P60:
      priv->video_width=800;
      priv->video_height=480;
      priv->video_framerate=60;
      break;
    case GST_WFD_HH_854x480P30:
      priv->video_width=854;
      priv->video_height=480;
      priv->video_framerate=30;
      break;
    case GST_WFD_HH_854x480P60:
      priv->video_width=854;
      priv->video_height=480;
      priv->video_framerate=60;
      break;
    case GST_WFD_HH_864x480P30:
      priv->video_width=864;
      priv->video_height=480;
      priv->video_framerate=30;
      break;
    case GST_WFD_HH_864x480P60:
      priv->video_width=864;
      priv->video_height=480;
      priv->video_framerate=60;
      break;
    case GST_WFD_HH_640x360P30:
      priv->video_width=640;
      priv->video_height=360;
      priv->video_framerate=30;
      break;
    case GST_WFD_HH_640x360P60:
      priv->video_width=640;
      priv->video_height=360;
      priv->video_framerate=60;
      break;
    case GST_WFD_HH_960x540P30:
      priv->video_width=960;
      priv->video_height=540;
      priv->video_framerate=30;
      break;
    case GST_WFD_HH_960x540P60:
      priv->video_width=960;
      priv->video_height=540;
      priv->video_framerate=60;
      break;
    case GST_WFD_HH_848x480P30:
      priv->video_width=848;
      priv->video_height=480;
      priv->video_framerate=30;
      break;
    case GST_WFD_HH_848x480P60:
      priv->video_width=848;
      priv->video_height=480;
      priv->video_framerate=60;
      break;
    default:
      break;
  }
  return GST_RTSP_OK;
}

static GstRTSPResult
gst_wfd_base_src_get_audio_parameter(GstWFDBaseSrc * src, GstWFDMessage * msg)
{
  GstWFDBaseSrcPrivate *priv = src->priv;
  GstWFDAudioFormats audio_format = GST_WFD_AUDIO_UNKNOWN;
  GstWFDAudioChannels audio_channels = GST_WFD_CHANNEL_UNKNOWN;
  GstWFDAudioFreq audio_frequency = GST_WFD_FREQ_UNKNOWN;
  guint audio_bitwidth = 0;
  guint32 audio_latency = 0;
  GstWFDResult wfd_res = GST_WFD_OK;

  wfd_res = gst_wfd_message_get_prefered_audio_format (msg, &audio_format, &audio_frequency, &audio_channels, &audio_bitwidth, &audio_latency);
  if(wfd_res != GST_WFD_OK) {
    GST_ERROR("Failed to get prefered audio format.");
    return GST_RTSP_ERROR;
  }

  priv->audio_format = g_strdup(msg->audio_codecs->list->audio_format);
  if(audio_frequency == GST_WFD_FREQ_48000)
    audio_frequency = 48000;
  else if(audio_frequency == GST_WFD_FREQ_44100)
    audio_frequency = 44100;

  if(audio_channels == GST_WFD_CHANNEL_2)
    audio_channels = 2;
  else if(audio_channels == GST_WFD_CHANNEL_4)
    audio_channels = 4;
  else if(audio_channels == GST_WFD_CHANNEL_6)
    audio_channels = 6;
  else if(audio_channels == GST_WFD_CHANNEL_8)
    audio_channels = 8;

  priv->audio_channels = audio_channels;
  priv->audio_frequency = audio_frequency;
  priv->audio_bitwidth = audio_bitwidth;

  return GST_RTSP_OK;
}

static GstRTSPResult
gst_wfd_base_src_get_video_parameter(GstWFDBaseSrc * src, GstWFDMessage * msg)
{
  GstWFDVideoCodecs cvCodec = GST_WFD_VIDEO_UNKNOWN;
  GstWFDVideoNativeResolution cNative = GST_WFD_VIDEO_CEA_RESOLUTION;
  guint64 cNativeResolution = 0;
  GstWFDVideoCEAResolution cCEAResolution = GST_WFD_CEA_UNKNOWN;
  GstWFDVideoVESAResolution cVESAResolution = GST_WFD_VESA_UNKNOWN;
  GstWFDVideoHHResolution cHHResolution = GST_WFD_HH_UNKNOWN;
  GstWFDVideoH264Profile cProfile = GST_WFD_H264_UNKNOWN_PROFILE;
  GstWFDVideoH264Level cLevel = GST_WFD_H264_LEVEL_UNKNOWN;
  guint32 cMaxHeight = 0;
  guint32 cMaxWidth = 0;
  guint32 cmin_slice_size = 0;
  guint32 cslice_enc_params = 0;
  guint cframe_rate_control = 0;
  guint cvLatency = 0;
  GstWFDResult wfd_res = GST_WFD_OK;

  wfd_res = gst_wfd_message_get_prefered_video_format (msg, &cvCodec, &cNative, &cNativeResolution,
      &cCEAResolution, &cVESAResolution, &cHHResolution,
      &cProfile, &cLevel, &cvLatency, &cMaxHeight,
      &cMaxWidth, &cmin_slice_size, &cslice_enc_params, &cframe_rate_control);
  if(wfd_res != GST_WFD_OK) {
      GST_ERROR("Failed to get prefered video format.");
      return GST_RTSP_ERROR;
  }
#if 0
  switch(cNative)
  {
    case GST_WFD_VIDEO_CEA_RESOLUTION:
      _get_cea_resolution_and_set_to_src(src, cCEAResolution);
      break;
    case GST_WFD_VIDEO_VESA_RESOLUTION:
      _get_vesa_resolution_and_set_to_src(src, cVESAResolution);
      break;
    case GST_WFD_VIDEO_HH_RESOLUTION:
      _get_hh_resolution_and_set_to_src(src, cHHResolution);
      break;
    default:
      break;
  }
#endif

  if(cCEAResolution != GST_WFD_CEA_UNKNOWN) {
    _get_cea_resolution_and_set_to_src(src, cCEAResolution);
  }
  else if(cVESAResolution != GST_WFD_VESA_UNKNOWN) {
    _get_vesa_resolution_and_set_to_src(src, cVESAResolution);
  }
   else if(cHHResolution != GST_WFD_HH_UNKNOWN) {
    _get_hh_resolution_and_set_to_src(src, cHHResolution);
  }

  return GST_RTSP_OK;
}

/*rtsp dump code start*/
typedef struct _RTSPKeyValue
{
  GstRTSPHeaderField field;
  gchar *value;
  gchar *custom_key;            /* custom header string (field is INVALID then) */
} RTSPKeyValue;

static void
_key_value_foreach (GArray * array, GFunc func, gpointer user_data)
{
  guint i;

  g_return_if_fail (array != NULL);

  for (i = 0; i < array->len; i++) {
    (*func) (&g_array_index (array, RTSPKeyValue, i), user_data);
  }
}

static void
_dump_key_value (gpointer data, gpointer user_data G_GNUC_UNUSED)
{
  RTSPKeyValue *key_value = (RTSPKeyValue *) data;
  const gchar *key_string;

  if (key_value->custom_key != NULL)
    key_string = key_value->custom_key;
  else
    key_string = gst_rtsp_header_as_text (key_value->field);

  GST_ERROR ("   key: '%s', value: '%s'", key_string, key_value->value);
}

static const char *
_cmd_to_string (guint cmd)
{
  switch (cmd) {
    case WFD_CMD_OPEN:
      return "OPEN";
    case WFD_CMD_PLAY:
      return "PLAY";
    case WFD_CMD_PAUSE:
      return "PAUSE";
    case WFD_CMD_CLOSE:
      return "CLOSE";
    case WFD_CMD_WAIT:
      return "WAIT";
    case WFD_CMD_LOOP:
      return "LOOP";
    case WFD_CMD_REQUEST:
      return "REQUEST";
  }

  return "unknown";
}

static GstRTSPResult
_rtsp_message_dump (GstRTSPMessage * msg)
{
  guint8 *data;
  guint size;

  g_return_val_if_fail (msg != NULL, GST_RTSP_EINVAL);

  GST_ERROR("------------------------------------------------------");
  switch (msg->type) {
    case GST_RTSP_MESSAGE_REQUEST:
      GST_ERROR ("RTSP request message %p", msg);
      GST_ERROR (" request line:");
      GST_ERROR ("   method: '%s'",
          gst_rtsp_method_as_text (msg->type_data.request.method));
      GST_ERROR ("   uri:    '%s'", msg->type_data.request.uri);
      GST_ERROR ("   version: '%s'",
          gst_rtsp_version_as_text (msg->type_data.request.version));
      GST_ERROR (" headers:");
      _key_value_foreach (msg->hdr_fields, _dump_key_value, NULL);
      GST_ERROR (" body:");
      gst_rtsp_message_get_body (msg, &data, &size);
      //gst_util_dump_mem (data, size);
      if (size > 0) GST_ERROR ("%s(%d)", data, size);
      break;
    case GST_RTSP_MESSAGE_RESPONSE:
      GST_ERROR ("RTSP response message %p", msg);
      GST_ERROR (" status line:");
      GST_ERROR ("   code:   '%d'", msg->type_data.response.code);
      GST_ERROR ("   reason: '%s'", msg->type_data.response.reason);
      GST_ERROR ("   version: '%s'",
          gst_rtsp_version_as_text (msg->type_data.response.version));
      GST_ERROR (" headers:");
      _key_value_foreach (msg->hdr_fields, _dump_key_value, NULL);
      gst_rtsp_message_get_body (msg, &data, &size);
      GST_ERROR (" body: length %d", size);
      //gst_util_dump_mem (data, size);
      if (size > 0) GST_ERROR ("%s(%d)", data, size);
      break;
    case GST_RTSP_MESSAGE_HTTP_REQUEST:
      GST_ERROR ("HTTP request message %p", msg);
      GST_ERROR (" request line:");
      GST_ERROR ("   method:  '%s'",
          gst_rtsp_method_as_text (msg->type_data.request.method));
      GST_ERROR ("   uri:     '%s'", msg->type_data.request.uri);
      GST_ERROR ("   version: '%s'",
          gst_rtsp_version_as_text (msg->type_data.request.version));
      GST_ERROR (" headers:");
      _key_value_foreach (msg->hdr_fields, _dump_key_value, NULL);
      GST_ERROR (" body:");
      gst_rtsp_message_get_body (msg, &data, &size);
      //gst_util_dump_mem (data, size);
      if (size > 0) GST_ERROR ("%s(%d)", data, size);
      break;
    case GST_RTSP_MESSAGE_HTTP_RESPONSE:
      GST_ERROR ("HTTP response message %p", msg);
      GST_ERROR (" status line:");
      GST_ERROR ("   code:    '%d'", msg->type_data.response.code);
      GST_ERROR ("   reason:  '%s'", msg->type_data.response.reason);
      GST_ERROR ("   version: '%s'",
          gst_rtsp_version_as_text (msg->type_data.response.version));
      GST_ERROR (" headers:");
      _key_value_foreach (msg->hdr_fields, _dump_key_value, NULL);
      gst_rtsp_message_get_body (msg, &data, &size);
      GST_ERROR (" body: length %d", size);
      //gst_util_dump_mem (data, size);
      if (size > 0) GST_ERROR ("%s(%d)", data, size);
      break;
    case GST_RTSP_MESSAGE_DATA:
      GST_ERROR ("RTSP data message %p", msg);
      GST_ERROR (" channel: '%d'", msg->type_data.data.channel);
      GST_ERROR (" size:    '%d'", msg->body_size);
      gst_rtsp_message_get_body (msg, &data, &size);
      //gst_util_dump_mem (data, size);
      if (size > 0) GST_ERROR ("%s(%d)", data, size);
      break;
    default:
      GST_ERROR ("unsupported message type %d", msg->type);
      return GST_RTSP_EINVAL;
  }

  GST_ERROR("------------------------------------------------------");
  return GST_RTSP_OK;
}
/*rtsp dump code end*/
