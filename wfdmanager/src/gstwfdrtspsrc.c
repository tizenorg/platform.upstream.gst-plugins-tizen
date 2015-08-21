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

/**
* SECTION:element-wfdrtspsrc
*
* Makes a connection to an RTSP server and read the data.
* Device recognition is through wifi direct.
* wfdrtspsrc strictly follows Wifi display specification.
*
* RTSP supports transport over TCP or UDP in unicast or multicast mode. By
* default wfdrtspsrc will negotiate a connection in the following order:
* UDP unicast/UDP multicast/TCP. The order cannot be changed but the allowed
* protocols can be controlled with the #GstWFDRTSPSrc:protocols property.
*
* wfdrtspsrc currently understands WFD capability negotiation messages
*
* wfdrtspsrc will internally instantiate an RTP session manager element
* that will handle the RTCP messages to and from the server, jitter removal,
* packet reordering along with providing a clock for the pipeline.
* This feature is implemented using the gstrtpbin element.
*
* wfdrtspsrc acts like a live source and will therefore only generate data in the
* PLAYING state.
*
* <refsect2>
* <title>Example launch line</title>
* |[
* gst-launch wfdrtspsrc location=rtsp://some.server/url ! fakesink
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

#include "gstwfdrtspsrc.h"
#include "wfdrtspmacro.h"

#ifdef G_OS_WIN32
#include <winsock2.h>
#endif

GST_DEBUG_CATEGORY_STATIC (wfdrtspsrc_debug);
#define GST_CAT_DEFAULT (wfdrtspsrc_debug)

static GstStaticPadTemplate gst_wfdrtspsrc_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp, "
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

enum _GstWfdRtspSrcBufferMode
{
  BUFFER_MODE_NONE,
  BUFFER_MODE_SLAVE,
  BUFFER_MODE_BUFFER,
  BUFFER_MODE_AUTO
};

#define _SET_FRAMERATE_TO_CAPS 1

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
#define DEFAULT_DO_RTCP          TRUE
#define DEFAULT_LATENCY_MS       2000
#define DEFAULT_UDP_BUFFER_SIZE  0x80000
#define DEFAULT_UDP_TIMEOUT          10000000

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
  PROP_DO_RTCP,
  PROP_LATENCY,
  PROP_UDP_BUFFER_SIZE,
  PROP_UDP_TIMEOUT,
  PROP_ENABLE_PAD_PROBE,
  PROP_LAST
};

#define gst_wfdrtspsrc_parent_class parent_class

/* commands we send to out loop to notify it of events */
#define CMD_OPEN	0
#define CMD_PLAY	1
#define CMD_PAUSE	2
#define CMD_CLOSE	3
#define CMD_WAIT	4
#define CMD_LOOP	5
#define CMD_REQUEST	6

#define GST_ELEMENT_PROGRESS(el, type, code, text)      \
G_STMT_START {                                          \
  gchar *__txt = _gst_element_error_printf text;        \
  gst_element_post_message (GST_ELEMENT_CAST (el),      \
      gst_message_new_progress (GST_OBJECT_CAST (el),   \
          GST_PROGRESS_TYPE_ ##type, code, __txt));     \
  g_free (__txt);                                       \
} G_STMT_END

/* object */
static void gst_wfdrtspsrc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_wfdrtspsrc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_wfdrtspsrc_finalize (GObject * object);

/* element */
static GstStateChangeReturn gst_wfdrtspsrc_change_state (GstElement * element,
    GstStateChange transition);
static gboolean gst_wfdrtspsrc_send_event (GstElement * element, GstEvent * event);

/* bin */
static void gst_wfdrtspsrc_handle_message (GstBin * bin, GstMessage * message);

static void gst_wfdrtspsrc_loop_send_cmd (GstWFDRTSPSrc * src, gint cmd);

/* fundemental functions */
static GstRTSPResult gst_wfdrtspsrc_open (GstWFDRTSPSrc * src);
static gboolean gst_wfdrtspsrc_setup (GstWFDRTSPSrc * src);
static GstRTSPResult gst_wfdrtspsrc_play (GstWFDRTSPSrc * src);
static GstRTSPResult gst_wfdrtspsrc_pause (GstWFDRTSPSrc * src);
static GstRTSPResult gst_wfdrtspsrc_close (GstWFDRTSPSrc * src, gboolean only_close);

static void gst_wfdrtspsrc_set_tcp_timeout (GstWFDRTSPSrc * src, guint64 timeout);


static void gst_wfdrtspsrc_uri_handler_init (gpointer g_iface,
    gpointer iface_data);
static gboolean gst_wfdrtspsrc_uri_set_uri (GstURIHandler * handler,
    const gchar * uri, GError **error);

static gboolean gst_wfdrtspsrc_loop (GstWFDRTSPSrc * src);
static gboolean gst_wfdrtspsrc_push_event (GstWFDRTSPSrc * src, GstEvent * event);


static GstRTSPResult
gst_wfdrtspsrc_send (GstWFDRTSPSrc * src, GstRTSPConnection * conn,
    GstRTSPMessage * request, GstRTSPMessage * response,
    GstRTSPStatusCode * code);

static gboolean gst_wfdrtspsrc_parse_methods (GstWFDRTSPSrc * src, GstRTSPMessage * response);

static void gst_wfdrtspsrc_connection_flush (GstWFDRTSPSrc * src, gboolean flush);

static GstRTSPResult gst_wfdrtspsrc_get_video_parameter(GstWFDRTSPSrc * src, WFDMessage *msg);
static GstRTSPResult gst_wfdrtspsrc_get_audio_parameter(GstWFDRTSPSrc * src, WFDMessage *msg);

static gboolean
gst_wfdrtspsrc_handle_src_event (GstPad * pad, GstObject *parent, GstEvent * event);
static gboolean
gst_wfdrtspsrc_handle_src_query(GstPad * pad, GstObject *parent, GstQuery * query);

static void
gst_wfdrtspsrc_send_pause_cmd (GstWFDRTSPSrc * src);
static void
gst_wfdrtspsrc_send_play_cmd (GstWFDRTSPSrc * src);
static void
gst_wfdrtspsrc_send_close_cmd (GstWFDRTSPSrc * src);
static void
gst_wfdrtspsrc_set_standby (GstWFDRTSPSrc * src);

static GstRTSPResult
gst_wfdrtspsrc_message_dump (GstRTSPMessage * msg);

#ifdef ENABLE_WFD_MESSAGE
static gint
__wfd_config_message_init(GstWFDRTSPSrc * src)
{
  src->message_handle = dlopen(WFD_MESSAGE_FEATURES_PATH, RTLD_LAZY);
  if (src->message_handle == NULL) {
    GST_ERROR("failed to init __wfd_config_message_init");
    src->extended_wfd_message_support = FALSE;
    return FALSE;
  } else {
    src->extended_wfd_message_support = TRUE;
  }
  return TRUE;
}

static void *
__wfd_config_message_func(GstWFDRTSPSrc *src, const char *func)
{
  return dlsym(src->message_handle, func);
}

static void
__wfd_config_message_close(GstWFDRTSPSrc *src)
{
  GST_DEBUG("close wfd cofig message");

  dlclose(src->message_handle);
  src->message_handle = NULL;
  src->extended_wfd_message_support = FALSE;
}
#endif

static guint gst_wfdrtspsrc_signals[LAST_SIGNAL] = { 0 };

static void
_do_init (GType wfdrtspsrc_type)
{
  static const GInterfaceInfo urihandler_info = {
    gst_wfdrtspsrc_uri_handler_init,
    NULL,
    NULL
  };

  GST_DEBUG_CATEGORY_INIT (wfdrtspsrc_debug, "wfdrtspsrc", 0, "Wi-Fi Display Sink source");

  g_type_add_interface_static (wfdrtspsrc_type, GST_TYPE_URI_HANDLER,
      &urihandler_info);
}

G_DEFINE_TYPE_WITH_CODE (GstWFDRTSPSrc, gst_wfdrtspsrc, GST_TYPE_BIN, _do_init(g_define_type_id))

static void
gst_wfdrtspsrc_class_init (GstWFDRTSPSrcClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBinClass *gstbin_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbin_class = (GstBinClass *) klass;

  gobject_class->set_property = gst_wfdrtspsrc_set_property;
  gobject_class->get_property = gst_wfdrtspsrc_get_property;
  gobject_class->finalize = gst_wfdrtspsrc_finalize;

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

  g_object_class_install_property (gobject_class, PROP_DO_RTCP,
      g_param_spec_boolean ("do-rtcp", "Do RTCP",
          "Send RTCP packets, disable for old incompatible server.",
          DEFAULT_DO_RTCP, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_LATENCY,
      g_param_spec_uint ("latency", "Buffer latency in ms",
          "Amount of ms to buffer", 0, G_MAXUINT, DEFAULT_LATENCY_MS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_UDP_BUFFER_SIZE,
      g_param_spec_int ("udp-buffer-size", "UDP Buffer Size",
          "Size of the kernel UDP receive buffer in bytes, 0=default",
          0, G_MAXINT, DEFAULT_UDP_BUFFER_SIZE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_UDP_TIMEOUT,
      g_param_spec_uint64 ("timeout", "Timeout",
          "Fail after timeout microseconds on UDP connections (0 = disabled)",
          0, G_MAXUINT64, DEFAULT_UDP_TIMEOUT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_ENABLE_PAD_PROBE,
          g_param_spec_boolean ("enable-pad-probe", "Enable Pad Probe",
              "Enable pad probe for debugging",
              FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_wfdrtspsrc_signals[SIGNAL_UPDATE_MEDIA_INFO] =
      g_signal_new ("update-media-info", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (GstWFDRTSPSrcClass, update_media_info),
      NULL, NULL, g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE, 1, GST_TYPE_STRUCTURE);

  gst_wfdrtspsrc_signals[SIGNAL_AV_FORMAT_CHANGE] =
      g_signal_new ("change-av-format", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (GstWFDRTSPSrcClass, change_av_format),
      NULL, NULL, g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1, G_TYPE_POINTER);

  gst_wfdrtspsrc_signals[SIGNAL_PAUSE] =
      g_signal_new ("pause", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_STRUCT_OFFSET (GstWFDRTSPSrcClass, pause), NULL, NULL,
      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0, G_TYPE_NONE);

  gst_wfdrtspsrc_signals[SIGNAL_RESUME] =
      g_signal_new ("resume", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_STRUCT_OFFSET (GstWFDRTSPSrcClass, resume), NULL, NULL,
      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0, G_TYPE_NONE);

  gst_wfdrtspsrc_signals[SIGNAL_CLOSE] =
      g_signal_new ("close", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_STRUCT_OFFSET (GstWFDRTSPSrcClass, close), NULL, NULL,
      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0, G_TYPE_NONE);

  gst_wfdrtspsrc_signals[SIGNAL_SET_STANDBY] =
      g_signal_new ("set-standby", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_STRUCT_OFFSET (GstWFDRTSPSrcClass, set_standby), NULL, NULL,
      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0, G_TYPE_NONE);

  gstelement_class->send_event =
      GST_DEBUG_FUNCPTR(gst_wfdrtspsrc_send_event);
  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR(gst_wfdrtspsrc_change_state);

  gstbin_class->handle_message =
      GST_DEBUG_FUNCPTR(gst_wfdrtspsrc_handle_message);

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_wfdrtspsrc_src_template));

  gst_element_class_set_details_simple (gstelement_class, "Wi-Fi Display Sink source element",
      "Source/Network",
      "Negotiate the capability and receive the RTP packets from the Wi-Fi Display source",
      "YeJin Cho <cho.yejin@samsung.com>");

  klass->pause = GST_DEBUG_FUNCPTR (gst_wfdrtspsrc_send_pause_cmd);
  klass->resume = GST_DEBUG_FUNCPTR (gst_wfdrtspsrc_send_play_cmd);
  klass->close = GST_DEBUG_FUNCPTR (gst_wfdrtspsrc_send_close_cmd);
  klass->set_standby = GST_DEBUG_FUNCPTR (gst_wfdrtspsrc_set_standby);
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
gst_wfdrtspsrc_init (GstWFDRTSPSrc * src)
{
  GstPadTemplate *template = NULL;
  gint result = FALSE;

#ifdef G_OS_WIN32
  WSADATA wsa_data;

  if (WSAStartup (MAKEWORD (2, 2), &wsa_data) != 0) {
    GST_ERROR_OBJECT (src, "WSAStartup failed: 0x%08x", WSAGetLastError ());
  }
#endif

  src->conninfo.location = g_strdup (DEFAULT_LOCATION);
  src->conninfo.url_str = NULL;
  src->debug = DEFAULT_DEBUG;
  src->retry = DEFAULT_RETRY;
  gst_wfdrtspsrc_set_tcp_timeout (src, DEFAULT_TCP_TIMEOUT);
  src->rtp_blocksize = DEFAULT_RTP_BLOCKSIZE;
  src->user_agent = g_strdup (DEFAULT_USER_AGENT);
  src->do_stop = FALSE;
  src->hdcp_param = NULL;
  src->audio_param = gst_wfd_rtsp_set_default_audio_param ();
  src->video_param = gst_wfd_rtsp_set_default_video_param ();
  src->request_param.type = WFD_PARAM_NONE;
#ifdef ENABLE_WFD_MESSAGE
  result = __wfd_config_message_init(src);
  if(result == FALSE)
    return;
#endif

  /* init lock */
  g_rec_mutex_init (&(src->task_rec_lock));

  /* create manager */
  src->manager = wfd_rtsp_manager_new (GST_ELEMENT_CAST(src));
  if (G_UNLIKELY (src->manager == NULL)) {
    GST_ERROR_OBJECT (src, "could not create wfdrtspmanager element");
    return;
  }

  /* create ghost pad for using src pad */
  template = gst_static_pad_template_get (&gst_wfdrtspsrc_src_template);
  src->manager->srcpad = gst_ghost_pad_new_no_target_from_template ("src", template);
  gst_element_add_pad (GST_ELEMENT_CAST (src), src->manager->srcpad);
  gst_object_unref (template);

  gst_pad_set_event_function (src->manager->srcpad,
      GST_DEBUG_FUNCPTR (gst_wfdrtspsrc_handle_src_event));
  gst_pad_set_query_function (src->manager->srcpad,
      GST_DEBUG_FUNCPTR (gst_wfdrtspsrc_handle_src_query));


  src->state = GST_RTSP_STATE_INVALID;

  GST_OBJECT_FLAG_SET (src, GST_ELEMENT_FLAG_SOURCE);
}

static void
gst_wfdrtspsrc_finalize (GObject * object)
{
  GstWFDRTSPSrc *src;

  src = GST_WFDRTSPSRC (object);

  gst_rtsp_url_free (src->conninfo.url);
  if (src->conninfo.location)
    g_free (src->conninfo.location);
  src->conninfo.location = NULL;
  if (src->conninfo.url_str)
    g_free (src->conninfo.url_str);
  src->conninfo.url_str = NULL;
  if (src->user_agent)
    g_free (src->user_agent);
  src->user_agent = NULL;
  if(src->audio_param) {
    gst_structure_free(src->audio_param);
    src->audio_param = NULL;
  }
  if(src->video_param) {
    gst_structure_free(src->video_param);
    src->video_param = NULL;
  }
  if(src->hdcp_param) {
    gst_structure_free(src->hdcp_param);
    src->hdcp_param = NULL;
  }

  /* free locks */
  g_rec_mutex_clear (&(src->task_rec_lock));

#ifdef ENABLE_WFD_MESSAGE
__wfd_config_message_close(src);
#endif

  if(src->manager) {
    g_object_unref (G_OBJECT(src->manager));
    src->manager = NULL;
  }

#ifdef G_OS_WIN32
  WSACleanup ();
#endif

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_wfdrtspsrc_set_tcp_timeout (GstWFDRTSPSrc * src, guint64 timeout)
{
  src->tcp_timeout.tv_sec = timeout / G_USEC_PER_SEC;
  src->tcp_timeout.tv_usec = timeout % G_USEC_PER_SEC;

  if (timeout != 0)
    src->ptcp_timeout = &src->tcp_timeout;
  else
    src->ptcp_timeout = NULL;
}

static void
gst_wfdrtspsrc_set_property (GObject * object, guint prop_id, const GValue * value,
    GParamSpec * pspec)
{
  GstWFDRTSPSrc *src = GST_WFDRTSPSRC (object);
  GError *err = NULL;

  switch (prop_id) {
    case PROP_LOCATION:
      gst_wfdrtspsrc_uri_set_uri (GST_URI_HANDLER (src),
          g_value_get_string (value), &err);
      break;
    case PROP_DEBUG:
      src->debug = g_value_get_boolean (value);
      break;
    case PROP_RETRY:
      src->retry = g_value_get_uint (value);
      break;
    case PROP_TCP_TIMEOUT:
      gst_wfdrtspsrc_set_tcp_timeout (src, g_value_get_uint64 (value));
      break;
    case PROP_RTP_BLOCKSIZE:
      src->rtp_blocksize = g_value_get_uint (value);
      break;
    case PROP_USER_AGENT:
      if (src->user_agent)
        g_free(src->user_agent);
      src->user_agent = g_value_dup_string (value);
      break;
    case PROP_AUDIO_PARAM:
    {
      const GstStructure *s = gst_value_get_structure (value);
      if (src->audio_param)
        gst_structure_free (src->audio_param);
      if (s)
        src->audio_param = gst_structure_copy (s);
      else
        src->audio_param = NULL;
      break;
    }
    case PROP_VIDEO_PARAM:
    {
      const GstStructure *s = gst_value_get_structure (value);
      if (src->video_param)
        gst_structure_free (src->video_param);
      if (s)
        src->video_param = gst_structure_copy (s);
      else
        src->video_param = NULL;
      break;
    }
    case PROP_HDCP_PARAM:
    {
      const GstStructure *s = gst_value_get_structure (value);
      if (src->hdcp_param)
        gst_structure_free (src->hdcp_param);
      if (s)
        src->hdcp_param = gst_structure_copy (s);
      else
        src->hdcp_param = NULL;
      break;
    }
    case PROP_DO_RTCP:
      g_object_set_property (G_OBJECT (src->manager), "do-rtcp", value);
      break;
    case PROP_LATENCY:
      g_object_set_property (G_OBJECT (src->manager), "latency", value);
      break;
    case PROP_UDP_BUFFER_SIZE:
      g_object_set_property (G_OBJECT (src->manager), "udp-buffer-size", value);
      break;
    case PROP_UDP_TIMEOUT:
      g_object_set_property (G_OBJECT (src->manager), "timeout", value);
      break;
    case PROP_ENABLE_PAD_PROBE:
      g_object_set_property (G_OBJECT (src->manager), "enable-pad-probe", value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_wfdrtspsrc_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstWFDRTSPSrc *src = GST_WFDRTSPSRC (object);

  switch (prop_id) {
    case PROP_LOCATION:
      g_value_set_string (value, src->conninfo.location);
      break;
    case PROP_DEBUG:
      g_value_set_boolean (value, src->debug);
      break;
    case PROP_RETRY:
      g_value_set_uint (value, src->retry);
      break;
    case PROP_TCP_TIMEOUT:
    {
      guint64 timeout;

      timeout = src->tcp_timeout.tv_sec * (guint64)G_USEC_PER_SEC +
          src->tcp_timeout.tv_usec;
      g_value_set_uint64 (value, timeout);
      break;
    }
    case PROP_RTP_BLOCKSIZE:
      g_value_set_uint (value, src->rtp_blocksize);
      break;
    case PROP_USER_AGENT:
      g_value_set_string (value, src->user_agent);
      break;
    case PROP_AUDIO_PARAM:
      gst_value_set_structure (value, src->audio_param);
      break;
    case PROP_VIDEO_PARAM:
      gst_value_set_structure (value, src->video_param);
      break;
    case PROP_HDCP_PARAM:
      gst_value_set_structure (value, src->hdcp_param);
      break;
    case PROP_DO_RTCP:
      g_object_get_property (G_OBJECT (src->manager), "do-rtcp", value);
      break;
    case PROP_LATENCY:
      g_object_get_property (G_OBJECT (src->manager), "latency", value);
      break;
    case PROP_UDP_BUFFER_SIZE:
      g_object_get_property (G_OBJECT (src->manager), "udp-buffer-size", value);
      break;
    case PROP_UDP_TIMEOUT:
      g_object_get_property (G_OBJECT (src->manager), "timeout", value);
      break;
    case PROP_ENABLE_PAD_PROBE:
      g_object_get_property (G_OBJECT (src->manager), "enable-pad-probe", value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_wfdrtspsrc_cleanup (GstWFDRTSPSrc * src)
{
  GST_DEBUG_OBJECT (src, "cleanup");
}

static void
gst_wfdrtspsrc_flush (GstWFDRTSPSrc * src, gboolean flush)
{
  GstEvent *event;
  gint cmd;
  GstState state;

  if (flush) {
    event = gst_event_new_flush_start ();
    GST_DEBUG_OBJECT (src, "start flush");
    cmd = CMD_WAIT;
    state = GST_STATE_PAUSED;
  } else {
    event = gst_event_new_flush_stop (FALSE);
    GST_DEBUG_OBJECT (src, "stop flush");
    cmd = CMD_LOOP;
    state = GST_STATE_PLAYING;
  }
  gst_wfdrtspsrc_push_event (src, event);
  gst_wfdrtspsrc_loop_send_cmd (src, cmd);

  wfd_rtsp_manager_set_state (src->manager, state);
}

static GstRTSPResult
gst_wfdrtspsrc_connection_send (GstWFDRTSPSrc * src, GstRTSPConnection * conn,
    GstRTSPMessage * message, GTimeVal * timeout)
{
  GstRTSPResult ret = GST_RTSP_OK;

  if (src->debug)
    gst_wfdrtspsrc_message_dump (message);

  if (conn)
    ret = gst_rtsp_connection_send (conn, message, timeout);
  else
    ret = GST_RTSP_ERROR;

  return ret;
}

static GstRTSPResult
gst_wfdrtspsrc_connection_receive (GstWFDRTSPSrc * src, GstRTSPConnection * conn,
    GstRTSPMessage * message, GTimeVal * timeout)
{
  GstRTSPResult ret;

  if (conn)
    ret = gst_rtsp_connection_receive (conn, message, timeout);
  else
    ret = GST_RTSP_ERROR;

  if (src->debug)
    gst_wfdrtspsrc_message_dump (message);

  return ret;
}

static GstRTSPResult
gst_wfdrtspsrc_send_request (GstWFDRTSPSrc * src)
{
  GstWFDRequestParam request_param = {0};
  GstRTSPMessage request = { 0 };
  GstRTSPMessage response = { 0 };
  GstRTSPResult res = GST_RTSP_OK;
  WFDResult wfd_res = WFD_OK;
  WFDMessage *wfd_msg= NULL;
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
    src->conninfo.url_str);
  if (res < 0)
    goto error;

  gst_rtsp_message_add_header (&request, GST_RTSP_HDR_USER_AGENT, (const gchar*)src->user_agent);

  /* Create set_parameter body to be sent in the request */
  WFDCONFIG_MESSAGE_NEW (&wfd_msg, error);
  WFDCONFIG_MESSAGE_INIT(wfd_msg, error);

  switch(request_param.type) {
    case WFD_ROUTE:
     /* Note : RTSP M10  :
      *   Send RTSP SET_PARAMETER with wfd-route to change the WFD sink at which audio is rendered.
      *   Applies only when both a primary and secondary sinks are in WFD session with a WFD source.
      */
      WFDCONFIG_SET_AUDIO_SINK_TYPE(wfd_msg, request_param.route_setting.type, error);
      break;

    case WFD_CONNECTOR_TYPE:
     /* Note : RTSP M11  :
      *   Send RTSP SET_PARAMETER with wfd-connector-type to indicate change of active connector type,
      *     when the WFD source and WFD sink support content protection.
      */
      WFDCONFIG_SET_CONNECTOR_TYPE(wfd_msg, request_param.connector_setting.type, error);
      break;

    case WFD_STANDBY:
     /* Note : RTSP M12  :
      *   Send RTSP SET_PARAMETER with wfd-standby to indicate that the sender is entering WFD standby mode.
      */
      WFDCONFIG_SET_STANDBY(wfd_msg, TRUE, error);
      break;

    case WFD_IDR_REQUEST:
     /* Note : RTSP M13  :
      *   Send RTSP SET_PARAMETER with wfd-idr-request to request IDR refresh.
      */
      WFDCONFIG_SET_IDR_REQUEST(wfd_msg, error);
      break;

    default:
      GST_ERROR_OBJECT (src, "Unhandled WFD message type...");
      goto error;
      break;
  }

  WFDCONFIG_MESSAGE_DUMP(wfd_msg);
  WFDCONFIG_MESSAGE_AS_TEXT(wfd_msg, rtsp_body, error);

  if(rtsp_body == NULL)
    goto error;

  rtsp_body_length = strlen(rtsp_body);
  rtsp_body_length_str = g_string_new ("");
  g_string_append_printf (rtsp_body_length_str,"%d",rtsp_body_length);

  GST_DEBUG ("WFD message body: %s", rtsp_body);

  /* add content-length type */
  gst_rtsp_message_add_header (&request, GST_RTSP_HDR_CONTENT_LENGTH, g_string_free (rtsp_body_length_str, FALSE));

  /* adding wfdconfig data to request */
  res = gst_rtsp_message_set_body (&request,(guint8 *)rtsp_body, rtsp_body_length);
  if (res != GST_RTSP_OK) {
    GST_ERROR_OBJECT (src, "Failed to set body to rtsp request...");
    goto error;
  }

  WFDCONFIG_MESSAGE_FREE(wfd_msg);

  /* send request message  */
  GST_DEBUG_OBJECT (src, "send reuest...");
  if ((res = gst_wfdrtspsrc_send (src, src->conninfo.connection, &request, &response,
      NULL)) < 0)
    goto error;

  return res;

/* ERRORS */
error:
 {
    if(wfd_msg)
      WFDCONFIG_MESSAGE_FREE(wfd_msg);
    gst_rtsp_message_unset (&request);
    gst_rtsp_message_unset (&response);
    if(wfd_res != WFD_OK) {
      GST_ERROR_OBJECT(src, "Message config error : %d", wfd_res);
      return GST_RTSP_ERROR;
    }
    return res;
 }
}

static gboolean
gst_wfdrtspsrc_handle_src_event (GstPad * pad, GstObject *parent, GstEvent * event)
{
  GstWFDRTSPSrc *src;
  gboolean res = TRUE;
  gboolean forward = FALSE;
  const GstStructure *s;
  /*static gdouble elapsed = 0.0;
  static int count = 0;*/

  src = GST_WFDRTSPSRC_CAST (parent);
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
	 gst_wfdrtspsrc_loop_send_cmd(src, CMD_REQUEST);
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
gst_wfdrtspsrc_handle_src_query (GstPad * pad, GstObject *parent, GstQuery * query)
{
  GstWFDRTSPSrc *src = NULL;
  gboolean res = FALSE;

  src = GST_WFDRTSPSRC_CAST (parent);
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
      if (src->conninfo.location == NULL) {
        res = FALSE;
      } else {
        gst_query_set_uri( query, src->conninfo.location);
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
gst_wfdrtspsrc_configure_caps (GstWFDRTSPSrc * src)
{
  WFDRTSPManager *manager = src->manager;;
  GstCaps *caps;
  guint64 start, stop;
  gdouble play_speed, play_scale;
  GstStructure *structure;

  GST_DEBUG_OBJECT (src, "configuring manager caps");

  start = 0;
  stop = GST_CLOCK_TIME_NONE;
  play_speed = 1.000;
  play_scale = 1.000;

  if ((caps = manager->caps)) {
    caps = gst_caps_make_writable (caps);
    structure = gst_caps_get_structure (caps, 0);
    /* update caps */
    if (manager->timebase != GST_CLOCK_TIME_NONE)
      gst_structure_set (structure, "clock-base", G_TYPE_UINT,
          (guint) manager->timebase, NULL);
    if (manager->seqbase != GST_CLOCK_TIME_NONE)
      gst_structure_set (structure, "seqnum-base", G_TYPE_UINT,
          (guint) manager->seqbase, NULL);
    gst_structure_set (structure, "npt-start", G_TYPE_UINT64, start, NULL);

    if (stop != GST_CLOCK_TIME_NONE)
      gst_structure_set (structure, "npt-stop", G_TYPE_UINT64, stop, NULL);
    gst_structure_set (structure, "play-speed", G_TYPE_DOUBLE, play_speed, NULL);
    gst_structure_set (structure, "play-scale", G_TYPE_DOUBLE, play_scale, NULL);
    gst_structure_set (structure, "height", G_TYPE_INT, src->video_height, NULL);
    gst_structure_set (structure, "width", G_TYPE_INT, src->video_width, NULL);
    gst_structure_set (structure, "video-framerate", G_TYPE_INT, src->video_framerate, NULL);

    GST_DEBUG_OBJECT(src, "Frame rate : %d", src->video_framerate);

    manager->caps = caps;

#ifdef _SET_FRAMERATE_TO_CAPS
    /* Set caps to udpsrc */
    GstCaps *udp_caps = NULL;
    GstStructure *s = NULL;
    g_object_get(manager->udpsrc[0], "caps", &udp_caps, NULL);
    udp_caps = gst_caps_make_writable (udp_caps);
    s = gst_caps_get_structure (udp_caps, 0);
    gst_structure_set (s, "npt-start", G_TYPE_UINT64, start, NULL);
    gst_structure_set (s, "npt-stop", G_TYPE_UINT64, stop, NULL);
    gst_structure_set (s, "play-speed", G_TYPE_DOUBLE, play_speed, NULL);
    gst_structure_set (s, "play-scale", G_TYPE_DOUBLE, play_scale, NULL);
    gst_structure_set (s, "width", G_TYPE_INT, src->video_width, NULL);
    gst_structure_set (s, "height", G_TYPE_INT, src->video_height, NULL);
    gst_structure_set (s, "video-framerate", G_TYPE_INT, src->video_framerate, NULL);
    g_object_set(manager->udpsrc[0], "caps", udp_caps, NULL);
#endif
  }
  GST_DEBUG_OBJECT (src, "manager %p, caps %" GST_PTR_FORMAT, manager, caps);

  if (manager->srcpad) {
    GST_DEBUG_OBJECT (src, "set caps for srcpad");
    gst_pad_set_caps(manager->srcpad, manager->caps);
  }
}

static gboolean
gst_wfdrtspsrc_push_event (GstWFDRTSPSrc * src, GstEvent * event)
{
  WFDRTSPManager * manager;
  gboolean res = TRUE;

  g_return_val_if_fail(GST_IS_EVENT(event), FALSE);

  manager = src->manager;

  gst_event_ref (event);

  if (manager->udpsrc[0]) {
    gst_event_ref (event);
    res = gst_element_send_event (manager->udpsrc[0], event);
  } else if (manager->channelpad[0]) {
    gst_event_ref (event);
    if (GST_PAD_IS_SRC (manager->channelpad[0]))
      res = gst_pad_push_event (manager->channelpad[0], event);
    else
      res = gst_pad_send_event (manager->channelpad[0], event);
  }

  if (manager->udpsrc[1]) {
    gst_event_ref (event);
    res &= gst_element_send_event (manager->udpsrc[1], event);
  } else if (manager->channelpad[1]) {
    gst_event_ref (event);
    if (GST_PAD_IS_SRC (manager->channelpad[1]))
      res &= gst_pad_push_event (manager->channelpad[1], event);
    else
      res &= gst_pad_send_event (manager->channelpad[1], event);
  }

  gst_event_unref (event);

  return res;
}

static GstRTSPResult
gst_wfdrtspsrc_conninfo_connect (GstWFDRTSPSrc * src, GstWFDRTSPConnInfo * info)
{
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
    if (src->do_stop) {
      GST_ERROR_OBJECT (src, "stop connecting....");
      return GST_RTSP_EINTR;
    }

    if ((res =
            gst_rtsp_connection_connect (info->connection,
                src->ptcp_timeout)) < 0) {
      if (retry < src->retry) {
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
gst_wfdrtspsrc_conninfo_close (GstWFDRTSPSrc * src, GstWFDRTSPConnInfo * info,
    gboolean free)
{
  g_return_val_if_fail (info, GST_RTSP_EINVAL);

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
  return GST_RTSP_OK;
}

static void
gst_wfdrtspsrc_connection_flush (GstWFDRTSPSrc * src, gboolean flush)
{
  GST_DEBUG_OBJECT (src, "set flushing %d", flush);
  if (src->conninfo.connection) {
    GST_DEBUG_OBJECT (src, "connection flush %d", flush);
    gst_rtsp_connection_flush (src->conninfo.connection, flush);
  }

  if (src->manager) {
    WFDRTSPManager *manager = src->manager;
    GST_DEBUG_OBJECT (src, "manager %p flush %d", manager, flush);
    if (manager->conninfo.connection)
      gst_rtsp_connection_flush (manager->conninfo.connection, flush);
  }
  GST_DEBUG_OBJECT (src, "flushing %d is done", flush);
}

static GstRTSPResult
gst_wfdrtspsrc_handle_request (GstWFDRTSPSrc * src, GstRTSPConnection * conn,
    GstRTSPMessage * request)
{
  GstRTSPMethod method = GST_RTSP_INVALID;
  GstRTSPVersion version = GST_RTSP_VERSION_INVALID;
  GstRTSPMessage response = { 0 };
  GstRTSPResult res = GST_RTSP_OK;
  WFDResult wfd_res = WFD_OK;
  const gchar *uristr;
  guint8 *data = NULL;
  guint size = 0;
  WFDMessage *wfd_msg = NULL;

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
      GST_DEBUG_OBJECT (src, "Creating OPTIONS response : %s", options_str);

      res = gst_rtsp_message_init_response (&response, GST_RTSP_STS_OK,
         gst_rtsp_status_as_text (GST_RTSP_STS_OK), request);
      if(res < 0) {
       g_free (options_str);
       goto send_error;
      }
      gst_rtsp_message_add_header (&response, GST_RTSP_HDR_PUBLIC, options_str);
      gst_rtsp_message_add_header (&response, GST_RTSP_HDR_USER_AGENT, (const gchar*)src->user_agent);
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
      gst_rtsp_message_add_header (&response, GST_RTSP_HDR_USER_AGENT, (const gchar*)src->user_agent);

      res = gst_rtsp_message_get_body (request, &data, &size);
      if (res < 0)
        goto send_error;

      if (size==0) {
        /* Note : RTSP M16 : WFD keep-alive :
         *   The WFD keep-alive function is used to periodically ensure the status of WFD sesion.
         *   A WFD source indicates the timeout value via the "Session:" line in the RTSP M6 response.
         *   A WFD sink shall respond with an RTSP M16 request message upon successful receiving the RTSP M16 request message.
         */
        res = gst_rtsp_connection_reset_timeout (src->conninfo.connection);
        if (res < 0)
          goto send_error;
        break;
      }

      if(src->extended_wfd_message_support == FALSE)
        goto message_config_error;

      WFDCONFIG_MESSAGE_NEW(&wfd_msg, message_config_error);
      WFDCONFIG_MESSAGE_INIT(wfd_msg, message_config_error);
      WFDCONFIG_MESSAGE_PARSE_BUFFER(data, size, wfd_msg, message_config_error);
      WFDCONFIG_MESSAGE_DUMP(wfd_msg);

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

        if(src->audio_param != NULL) {
          if (gst_structure_has_field (src->audio_param, "audio_codec"))
            gst_structure_get_uint (src->audio_param, "audio_codec", &audio_codec);
          if (gst_structure_has_field (src->audio_param, "audio_latency"))
            gst_structure_get_uint (src->audio_param, "audio_latency", &audio_latency);
          if (gst_structure_has_field (src->audio_param, "audio_channels"))
            gst_structure_get_uint (src->audio_param, "audio_channels", &audio_channels);
          if (gst_structure_has_field (src->audio_param, "audio_sampling_frequency"))
            gst_structure_get_uint (src->audio_param, "audio_sampling_frequency", &audio_sampling_frequency);
        }

        WFDCONFIG_SET_SUPPORTED_AUDIO_FORMAT(wfd_msg,
          audio_codec,
          audio_sampling_frequency,
          audio_channels,
          16,
          audio_latency,
          message_config_error);
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

        if (src->video_param != NULL) {
          if (gst_structure_has_field (src->video_param, "video_codec"))
            gst_structure_get_uint (src->video_param, "video_codec", &video_codec);
          if (gst_structure_has_field (src->video_param, "video_native_resolution"))
            gst_structure_get_uint (src->video_param, "video_native_resolution", &video_native_resolution);
          if (gst_structure_has_field (src->video_param, "video_cea_support"))
            gst_structure_get_uint (src->video_param, "video_cea_support", &video_cea_support);
          if (gst_structure_has_field (src->video_param, "video_vesa_support"))
            gst_structure_get_uint (src->video_param, "video_vesa_support", &video_vesa_support);
          if (gst_structure_has_field (src->video_param, "video_hh_support"))
            gst_structure_get_uint (src->video_param, "video_hh_support", &video_hh_support);
          if (gst_structure_has_field (src->video_param, "video_profile"))
            gst_structure_get_uint (src->video_param, "video_profile", &video_profile);
          if (gst_structure_has_field (src->video_param, "video_level"))
            gst_structure_get_uint (src->video_param, "video_level", &video_level);
          if (gst_structure_has_field (src->video_param, "video_latency"))
            gst_structure_get_uint (src->video_param, "video_latency", &video_latency);
          if (gst_structure_has_field (src->video_param, "video_vertical_resolution"))
            gst_structure_get_int (src->video_param, "video_vertical_resolution", &video_vertical_resolution);
          if (gst_structure_has_field (src->video_param, "video_horizontal_resolution"))
            gst_structure_get_int (src->video_param, "video_horizontal_resolution", &video_horizontal_resolution);
          if (gst_structure_has_field (src->video_param, "video_minimum_slicing"))
            gst_structure_get_int (src->video_param, "video_minimum_slicing", &video_minimum_slicing);
          if (gst_structure_has_field (src->video_param, "video_slice_enc_param"))
            gst_structure_get_int (src->video_param, "video_slice_enc_param", &video_slice_enc_param);
          if (gst_structure_has_field (src->video_param, "video_framerate_control_support"))
            gst_structure_get_int (src->video_param, "video_framerate_control_support", &video_framerate_control_support);
        }

        WFDCONFIG_SET_SUPPORTED_VIDEO_FORMAT(wfd_msg,
            video_codec,
            WFD_VIDEO_CEA_RESOLUTION,
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
            WFD_PREFERRED_DISPLAY_MODE_NOT_SUPPORTED,
            message_config_error);
      }

      /* Note : wfd-3d-formats :
       *    The wfd-3d-formats parameter specifies the support for stereoscopic video capabilities.
       */
      if(wfd_msg->video_3d_formats) {
        /* TODO : Set preferred video_3d_formats */
        wfd_res = WFD_OK;
      }

      /* Note : wfd-content-protection :
       *   The wfd-content-protection parameter specifies whether the WFD sink supports the HDCP system 2.0/2.1 for content protection.
       */
     if(wfd_msg->content_protection) {
        gint hdcp_version = 0;
        gint hdcp_port_no = 0;

        if (src->hdcp_param != NULL) {
          if (gst_structure_has_field (src->hdcp_param, "hdcp_version"))
            gst_structure_get_int (src->hdcp_param, "hdcp_version", &hdcp_version);
          if (gst_structure_has_field (src->hdcp_param, "hdcp_port_no"))
            gst_structure_get_int (src->hdcp_param, "hdcp_port_no", &hdcp_port_no);
        }

        WFDCONFIG_SET_CONTENT_PROTECTION_TYPE(wfd_msg,
          hdcp_version,
          (guint32)hdcp_port_no,
          message_config_error);
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
        WFDCONFIG_SET_DISPLAY_EDID(wfd_msg,
		      FALSE,
		      0,
		      NULL,
		      message_config_error);
      }

      /* Note : wfd-coupled-sink :
       *   The wfd-coupled-sink parameter is used by a WFD sink to convey its coupled status
       *     and if coupled to another WFD sink, the coupled WFD sink's MAC address
       */
      if(wfd_msg->coupled_sink) {
        /* To test with dummy coupled sink address */
        WFDCONFIG_SET_COUPLED_SINK(wfd_msg,
          WFD_SINK_COUPLED,
          (gchar *)"1.0.0.1:435",
          message_config_error);
      }

      /* Note : wfd-client-rtp-ports :
       *   The wfd-coupled-sink parameter is used by a WFD sink to convey the RTP port(s) that the WFD sink is listening on
       *     and by the a WFD source to indicate how audio, video or both audio and video payload will be encapsulated in the MPEG2-TS stream
       *     transmitted from the WFD source to the WFD sink.
       */
      if(wfd_msg->client_rtp_ports) {
        /* Hardcoded as of now. This is to comply with dongle port settings.
        This should be derived from gst_wfdrtspsrc_alloc_udp_ports */
        src->primary_rtpport = 19000;
        src->secondary_rtpport = 0;
        WFDCONFIG_SET_PREFERD_RTP_PORT(wfd_msg,
          WFD_RTSP_TRANS_RTP,
          WFD_RTSP_PROFILE_AVP,
          WFD_RTSP_LOWER_TRANS_UDP,
          src->primary_rtpport,
          src->secondary_rtpport,
          message_config_error);
      }

      /* Note : wfd-I2C :
       *   The wfd-I2C parameter is used by a WFD source to inquire whether a WFD sink supports remote I2C read/write function or not.
       *   If the WFD sink supports remote I2C read/write function, it shall set the value of this parameter to the TCP port number
       *     to be used by the WFD source to exchange remote I2C read/write messaging transactions with the WFD sink.
       */
      if(wfd_msg->I2C) {
        /* TODO */
        wfd_res = WFD_OK;
      }

      /* Note : wfd-connector-type :
       *   The WFD source may send wfd-connector-type parameter to inquire about the connector type that is currently active in the WFD sink.
       *   The WFD sink shall not send wfd-connector-type parameter unless the WFD source support this parameter.
       *   The WFD sink dongle that is not connected to an external display and it is not acting as a WFD sink with embedded display
       *     (to render streamed content) shall return a value of "none". Otherwise, the WFD sink shall choose a non-reserved value.
       */
      if(wfd_msg->connector_type) {
        /* TODO */
        wfd_res = WFD_OK;
      }

      /* Note : wfd-standby-resume-capability :
       *   The wfd-standby-resume-capability parameter describes support of both standby control using
       *     a wfd-standby parameter and resume control using PLAY and using triggered-method setting PLAY.
       */
      if(wfd_msg->standby_resume_capability) {
        /* TODO */
        wfd_res = WFD_OK;
      }

      WFDCONFIG_MESSAGE_AS_TEXT(wfd_msg, rtsp_body, message_config_error);

      rtsp_body_length = strlen(rtsp_body);
      rtsp_body_length_str = g_string_new ("");
      g_string_append_printf (rtsp_body_length_str,"%d", rtsp_body_length);

      gst_rtsp_message_add_header (&response, GST_RTSP_HDR_CONTENT_LENGTH, g_string_free (rtsp_body_length_str, FALSE));

      res = gst_rtsp_message_set_body (&response, (guint8*)rtsp_body, rtsp_body_length);
      if (res < 0)
        goto send_error;

      break;
    }

    case GST_RTSP_SET_PARAMETER:
    {
      res = gst_rtsp_message_init_response (&response, GST_RTSP_STS_OK, gst_rtsp_status_as_text (GST_RTSP_STS_OK), request);
      if (res < 0)
        goto send_error;

      gst_rtsp_message_add_header (&response, GST_RTSP_HDR_USER_AGENT, (const gchar*)src->user_agent);

      res = gst_rtsp_message_get_body (request, &data, &size);
      if (res < 0)
        goto send_error;

      WFDCONFIG_MESSAGE_NEW(&wfd_msg, message_config_error);
      WFDCONFIG_MESSAGE_INIT(wfd_msg, message_config_error);
      WFDCONFIG_MESSAGE_PARSE_BUFFER(data, size, wfd_msg, message_config_error);
      WFDCONFIG_MESSAGE_DUMP(wfd_msg);

      if (!wfd_msg)
        goto message_config_error;

     /* Note : RTSP M4 :
       */
      /* Note : wfd-trigger-method :
       *   The wfd-trigger-method parameter is used by a WFD source to trigger the WFD sink to initiate an operation with the WFD source.
       */
      if (wfd_msg->trigger_method) {
        WFDTrigger trigger = WFD_TRIGGER_UNKNOWN;

        WFDCONFIG_GET_TRIGGER_TYPE(wfd_msg, &trigger, message_config_error);

        res = gst_wfdrtspsrc_connection_send (src, conn, &response, NULL);
        if (res < 0)
          goto send_error;

        GST_DEBUG_OBJECT (src, "got trigger method for %s", GST_STR_NULL(wfd_msg->trigger_method->wfd_trigger_method));
        switch(trigger) {
          case WFD_TRIGGER_PAUSE:
            gst_wfdrtspsrc_loop_send_cmd (src, CMD_PAUSE);
            break;
          case WFD_TRIGGER_PLAY:
            gst_wfdrtspsrc_loop_send_cmd (src, CMD_PLAY);
            break;
          case WFD_TRIGGER_TEARDOWN:
            gst_wfdrtspsrc_loop_send_cmd (src, CMD_CLOSE);
            break;
          case WFD_TRIGGER_SETUP:
            if (!gst_wfdrtspsrc_setup (src))
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
          res = gst_wfdrtspsrc_get_audio_parameter(src, wfd_msg);
          if(res != GST_RTSP_OK) {
            goto message_config_error;
          }

          gst_structure_set (stream_info,
              "audio_format", G_TYPE_STRING, src->audio_format,
              "audio_channels", G_TYPE_INT, src->audio_channels,
              "audio_rate", G_TYPE_INT, src->audio_frequency,
              "audio_bitwidth", G_TYPE_INT, src->audio_bitwidth/16,
              NULL);
        }

        if(wfd_msg->video_formats) {
          res = gst_wfdrtspsrc_get_video_parameter(src, wfd_msg);
          if(res != GST_RTSP_OK) {
            goto message_config_error;
          }

          gst_structure_set (stream_info,
              "video_format", G_TYPE_STRING, "H264",
              "video_width", G_TYPE_INT, src->video_width,
              "video_height", G_TYPE_INT, src->video_height,
              "video_framerate", G_TYPE_INT, src->video_framerate,
              NULL);
        }

        if(wfd_msg->video_3d_formats) {
        /* TO DO */
        }

        g_signal_emit (src, gst_wfdrtspsrc_signals[SIGNAL_UPDATE_MEDIA_INFO], 0, stream_info);
      }

      /* Note : wfd-presentation-url :
       *   The wfd-presentation-url parameter describes the Universial Resource Identified (URI)
       *     to be used in the RTSP Setup (RTSP M6) request message in order to setup the WFD session from the WFD sink to the WFD source.
       */
      if(wfd_msg->presentation_url) {
        gchar *url0 = NULL, *url1 = NULL;

        WFDCONFIG_GET_PRESENTATION_URL(wfd_msg, &url0, &url1, message_config_error);

        g_free (src->conninfo.location);
        src->conninfo.location = g_strdup (url0);
        /* url1 is ignored as of now */
      }

      /* Note : wfd-client-rtp-ports :
       *   The wfd-coupled-sink parameter is used by a WFD sink to convey the RTP port(s) that the WFD sink is listening on
       *     and by the a WFD source to indicate how audio, video or both audio and video payload will be encapsulated in the MPEG2-TS stream
       *     transmitted from the WFD source to the WFD sink.
       */
      if(wfd_msg->client_rtp_ports) {
        WFDRTSPTransMode trans = WFD_RTSP_TRANS_UNKNOWN;
        WFDRTSPProfile profile = WFD_RTSP_PROFILE_UNKNOWN;
        WFDRTSPLowerTrans lowertrans = WFD_RTSP_LOWER_TRANS_UNKNOWN;
        guint32 rtp_port0 =0, rtp_port1 =0;

        WFDCONFIG_GET_PREFERD_RTP_PORT(wfd_msg, &trans, &profile, &lowertrans, &rtp_port0, &rtp_port1, message_config_error);
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

        WFDCONFIG_GET_AV_FORMAT_CHANGE_TIMING(wfd_msg, &pts, &dts, message_config_error);

        if (src->state == GST_RTSP_STATE_PLAYING) {
          GST_DEBUG_OBJECT(src, "change format with PTS[%lld] and DTS[%lld]", pts, dts);

          g_signal_emit (src, gst_wfdrtspsrc_signals[SIGNAL_AV_FORMAT_CHANGE], 0, (gpointer)&need_to_flush);

          if (need_to_flush) {
            gst_wfdrtspsrc_flush(src,TRUE);
            gst_wfdrtspsrc_flush(src, FALSE);
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

        WFDCONFIG_GET_STANDBY(wfd_msg, &standby_enable, message_config_error);

        GST_DEBUG_OBJECT (src, "wfd source is entering standby mode");
      }
      break;
    }

    default:
    {
      res = gst_rtsp_message_init_response (&response, GST_RTSP_STS_OK, gst_rtsp_status_as_text (GST_RTSP_STS_OK), request);
      if (res < 0)
        goto send_error;

      gst_rtsp_message_add_header (&response, GST_RTSP_HDR_USER_AGENT, (const gchar*)src->user_agent);

      break;
    }
  }

  res = gst_wfdrtspsrc_connection_send (src, conn, &response, src->ptcp_timeout);
  if (res < 0)
    goto send_error;

done:
  gst_rtsp_message_unset (request);
  gst_rtsp_message_unset (&response);
  WFDCONFIG_MESSAGE_FREE(wfd_msg);

  return GST_RTSP_OK;

  /* ERRORS */
setup_failed:
  {
    GST_ERROR_OBJECT(src, "Error: Could not setup(error)");
    gst_rtsp_message_unset (request);
    gst_rtsp_message_unset (&response);
    WFDCONFIG_MESSAGE_FREE(wfd_msg);
    return GST_RTSP_ERROR;
  }
message_config_error:
  {
    GST_ERROR_OBJECT(src, "Error: Message config error (%d)", wfd_res);
    gst_rtsp_message_unset (request);
    gst_rtsp_message_unset (&response);
    WFDCONFIG_MESSAGE_FREE(wfd_msg);
    return GST_RTSP_ERROR;
  }
send_error:
  {
    GST_ERROR_OBJECT(src, "Error: Could not send message");
    gst_rtsp_message_unset (request);
    gst_rtsp_message_unset (&response);
    WFDCONFIG_MESSAGE_FREE(wfd_msg);
    return res;
  }
}

static void
gst_wfdrtspsrc_loop_start_cmd (GstWFDRTSPSrc * src, gint cmd)
{
  GST_DEBUG_OBJECT (src, "start cmd %d", cmd);

  switch (cmd) {
    case CMD_OPEN:
      GST_ELEMENT_PROGRESS (src, START, "open", ("Opening Stream"));
      break;
    case CMD_PLAY:
      GST_ELEMENT_PROGRESS (src, START, "play", ("Sending PLAY request"));
      break;
    case CMD_PAUSE:
      GST_ELEMENT_PROGRESS (src, START, "pause", ("Sending PAUSE request"));
      break;
    case CMD_CLOSE:
      GST_ELEMENT_PROGRESS (src, START, "close", ("Closing Stream"));
      break;
    default:
      break;
  }
}

static void
gst_wfdrtspsrc_loop_complete_cmd (GstWFDRTSPSrc * src, gint cmd)
{
  GST_DEBUG_OBJECT (src, "complete cmd %d", cmd);

  switch (cmd) {
    case CMD_OPEN:
      GST_ELEMENT_PROGRESS (src, COMPLETE, "open", ("Opened Stream"));
      break;
    case CMD_PLAY:
      GST_ELEMENT_PROGRESS (src, COMPLETE, "play", ("Sent PLAY request"));
      break;
    case CMD_PAUSE:
      GST_ELEMENT_PROGRESS (src, COMPLETE, "pause", ("Sent PAUSE request"));
      break;
    case CMD_CLOSE:
      GST_ELEMENT_PROGRESS (src, COMPLETE, "close", ("Closed Stream"));
      break;
    default:
      break;
  }
}

static void
gst_wfdrtspsrc_loop_cancel_cmd (GstWFDRTSPSrc * src, gint cmd)
{
  GST_DEBUG_OBJECT (src, "cancel cmd %d", cmd);

  switch (cmd) {
    case CMD_OPEN:
      GST_ELEMENT_PROGRESS (src, CANCELED, "open", ("Open canceled"));
      break;
    case CMD_PLAY:
      GST_ELEMENT_PROGRESS (src, CANCELED, "play", ("PLAY canceled"));
      break;
    case CMD_PAUSE:
      GST_ELEMENT_PROGRESS (src, CANCELED, "pause", ("PAUSE canceled"));
      break;
    case CMD_CLOSE:
      GST_ELEMENT_PROGRESS (src, CANCELED, "close", ("Close canceled"));
      break;
    default:
      break;
  }
}

static void
gst_wfdrtspsrc_loop_error_cmd (GstWFDRTSPSrc * src, gint cmd)
{
  GST_DEBUG_OBJECT (src, "error cmd %d", cmd);

  switch (cmd) {
    case CMD_OPEN:
      GST_ELEMENT_PROGRESS (src, ERROR, "open", ("Open failed"));
      break;
    case CMD_PLAY:
      GST_ELEMENT_PROGRESS (src, ERROR, "play", ("PLAY failed"));
      break;
    case CMD_PAUSE:
      GST_ELEMENT_PROGRESS (src, ERROR, "pause", ("PAUSE failed"));
      break;
    case CMD_CLOSE:
      GST_ELEMENT_PROGRESS (src, ERROR, "close", ("Close failed"));
      break;
    default:
      break;
  }
}

static void
gst_wfdrtspsrc_loop_end_cmd (GstWFDRTSPSrc * src, gint cmd, GstRTSPResult ret)
{
  GST_DEBUG_OBJECT (src, "end cmd %d", cmd);

  if (ret == GST_RTSP_OK)
    gst_wfdrtspsrc_loop_complete_cmd (src, cmd);
  else if (ret == GST_RTSP_EINTR)
    gst_wfdrtspsrc_loop_cancel_cmd (src, cmd);
  else
    gst_wfdrtspsrc_loop_error_cmd (src, cmd);
}

static void
gst_wfdrtspsrc_loop_send_cmd (GstWFDRTSPSrc * src, gint cmd)
{
  gint old;

  /* start new request */
  gst_wfdrtspsrc_loop_start_cmd (src, cmd);

  GST_OBJECT_LOCK (src);

  GST_DEBUG_OBJECT (src, "sending cmd %d", cmd);

  old = src->loop_cmd;
  if (old != CMD_WAIT) {
    src->loop_cmd = CMD_WAIT;
    GST_OBJECT_UNLOCK (src);
    /* cancel previous request */
    gst_wfdrtspsrc_loop_cancel_cmd (src, old);
    GST_OBJECT_LOCK (src);
  }
  src->loop_cmd = cmd;

  /* interrupt if allowed */
  if (src->waiting) {
    GST_DEBUG_OBJECT (src, "start connection flush");
    gst_wfdrtspsrc_connection_flush (src, TRUE);
  }

  if (src->task)
    gst_task_start (src->task);

  GST_OBJECT_UNLOCK (src);
}

static GstFlowReturn
gst_wfdrtspsrc_loop_udp (GstWFDRTSPSrc * src)
{
  GstRTSPResult res = GST_RTSP_OK;
  GstRTSPMessage message = { 0 };

  while (TRUE) {
    GTimeVal tv_timeout;

    /* get the next timeout interval */
    gst_rtsp_connection_next_timeout (src->conninfo.connection, &tv_timeout);

    GST_DEBUG_OBJECT (src, "doing receive with timeout %d seconds",
        (gint) tv_timeout.tv_sec);

    gst_rtsp_message_unset (&message);

    /* we should continue reading the TCP socket because the server might
     * send us requests. When the session timeout expires, we need to send a
     * keep-alive request to keep the session open. */
    res = gst_wfdrtspsrc_connection_receive (src, src->conninfo.connection,
        &message, &tv_timeout);

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
        res =
            gst_wfdrtspsrc_handle_request (src, src->conninfo.connection,
            &message);
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

  /* we get here when the connection got interrupted */
interrupt:
  {
    gst_rtsp_message_unset (&message);
    GST_DEBUG_OBJECT (src, "got interrupted");
    return GST_FLOW_FLUSHING;
  }
connect_error:
  {
    gchar *str = gst_rtsp_strresult (res);
    GstFlowReturn ret;

    src->conninfo.connected = FALSE;
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ_WRITE, (NULL),
        ("Could not connect to server. (%s)", str));
    g_free (str);
    ret = GST_FLOW_ERROR;

    return ret;
  }
receive_error:
  {
    gchar *str = gst_rtsp_strresult (res);

    GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL),
        ("Could not receive message. (%s)", str));
    g_free (str);
    return GST_FLOW_ERROR;
  }
handle_request_failed:
  {
    gchar *str = gst_rtsp_strresult (res);
    GstFlowReturn ret;

    gst_rtsp_message_unset (&message);
    if (res != GST_RTSP_EINTR) {
      GST_ELEMENT_ERROR (src, RESOURCE, WRITE, (NULL),
          ("Could not handle server message. (%s)", str));
      g_free (str);
      ret = GST_FLOW_ERROR;
    } else {
      ret = GST_FLOW_FLUSHING;
    }
    return ret;
  }
server_eof:
  {
    GST_DEBUG_OBJECT (src, "we got an eof from the server");
    GST_ELEMENT_WARNING (src, RESOURCE, READ, (NULL),
        ("The server closed the connection."));
    src->conninfo.connected = FALSE;
    gst_rtsp_message_unset (&message);
    return GST_FLOW_EOS;
  }
}

static gboolean
gst_wfdrtspsrc_loop (GstWFDRTSPSrc * src)
{
  GstFlowReturn ret;

  if (!src->conninfo.connection || !src->conninfo.connected)
    goto no_connection;

  ret = gst_wfdrtspsrc_loop_udp (src);

  if (ret != GST_FLOW_OK)
    goto pause;

  return TRUE;

  /* ERRORS */
no_connection:
  {
    GST_ERROR_OBJECT (src, "we are not connected");
    ret = GST_FLOW_FLUSHING;
    goto pause;
  }
pause:
  {
    const gchar *reason = gst_flow_get_name (ret);

    GST_DEBUG_OBJECT (src, "pausing task, reason %s", reason);
    if (ret == GST_FLOW_EOS) {
      /* perform EOS logic */
      gst_wfdrtspsrc_push_event (src, gst_event_new_eos ());
    } else if (ret == GST_FLOW_NOT_LINKED || ret < GST_FLOW_EOS) {
      /* for fatal errors we post an error message, post the error before the
       * EOS so the app knows about the error first. */
      GST_ELEMENT_ERROR (src, STREAM, FAILED,
          ("Internal data flow error."),
          ("streaming task paused, reason %s (%d)", reason, ret));
      gst_wfdrtspsrc_push_event (src, gst_event_new_eos ());
    }

    return FALSE;
  }
}

static GstRTSPResult
gst_wfdrtspsrc_try_send (GstWFDRTSPSrc * src, GstRTSPConnection * conn,
    GstRTSPMessage * request, GstRTSPMessage * response,
    GstRTSPStatusCode * code)
{
  GstRTSPResult res = GST_RTSP_OK;
  GstRTSPStatusCode thecode = GST_RTSP_STS_OK;

  GST_DEBUG_OBJECT (src, "sending message");

  res = gst_wfdrtspsrc_connection_send (src, conn, request, src->ptcp_timeout);
  if (res < 0)
    goto send_error;

  gst_rtsp_connection_reset_timeout (conn);

next:
  res = gst_wfdrtspsrc_connection_receive (src, conn, response, src->ptcp_timeout);
  if (res < 0)
    goto receive_error;

  switch (response->type) {
    case GST_RTSP_MESSAGE_REQUEST:
      res = gst_wfdrtspsrc_handle_request (src, conn, response);
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
 * gst_wfdrtspsrc_send:
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
gst_wfdrtspsrc_send (GstWFDRTSPSrc * src, GstRTSPConnection * conn,
    GstRTSPMessage * request, GstRTSPMessage * response,
    GstRTSPStatusCode * code)
{
  GstRTSPStatusCode int_code = GST_RTSP_STS_OK;
  GstRTSPResult res = GST_RTSP_ERROR;
  GstRTSPMethod method = GST_RTSP_INVALID;

  /* save method so we can disable it when the server complains */
  method = request->type_data.request.method;

  if ((res =
          gst_wfdrtspsrc_try_send (src, conn, request, response, &int_code)) < 0)
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
        src->methods &= ~method;
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
gst_wfdrtspsrc_parse_methods (GstWFDRTSPSrc * src, GstRTSPMessage * response)
{
  GstRTSPHeaderField field;
  gchar *respoptions;
  gchar **options;
  gint indx = 0;
  gint i;

  /* reset supported methods */
  src->methods = 0;

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
        src->methods |= method;
    }
    g_strfreev (options);

    indx++;
  }

  if (src->methods == 0) {
    /* neither Allow nor Public are required, assume the server supports
     * at least DESCRIBE, SETUP, we always assume it supports PLAY as
     * well. */
    GST_DEBUG_OBJECT (src, "could not get OPTIONS");
    src->methods = GST_RTSP_SETUP;
  }
  /* always assume PLAY, FIXME, extensions should be able to override
   * this */
  src->methods |= GST_RTSP_PLAY;

  if (!(src->methods & GST_RTSP_SETUP))
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
gst_wfdrtspsrc_create_transports_string (GstWFDRTSPSrc * src, gchar ** transports)
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
  g_string_append_printf (result, "%d", src->primary_rtpport);
  g_string_append (result, "-");
  g_string_append_printf (result, "%d", src->primary_rtpport+1);

  *transports = g_string_free (result, FALSE);

  GST_DEBUG_OBJECT (src, "prepared transports %s", GST_STR_NULL (*transports));

  return GST_RTSP_OK;
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
gst_wfdrtspsrc_setup (GstWFDRTSPSrc * src)
{
  GstRTSPResult res = GST_RTSP_OK;
  GstRTSPMessage request = { 0 };
  GstRTSPMessage response = { 0 };
  WFDRTSPManager *manager = NULL;
  GstRTSPLowerTrans protocols = GST_RTSP_LOWER_TRANS_UNKNOWN;
  GstRTSPStatusCode code = GST_RTSP_STS_OK;
  GstRTSPUrl *url;
  gchar *hval;
  gchar *transports = NULL;

  if (!src->conninfo.connection || !src->conninfo.connected)
    goto no_connection;

  url = gst_rtsp_connection_get_url (src->conninfo.connection);
  protocols = url->transports & GST_RTSP_LOWER_TRANS_UDP;
  if (protocols == 0)
    goto no_protocols;

  GST_DEBUG_OBJECT (src, "doing setup of with %s", GST_STR_NULL(src->conninfo.location));

  manager = src->manager;
  manager->control_connection = src->conninfo.connection;

  GST_DEBUG_OBJECT (src, "protocols = 0x%x", protocols);
  /* create a string with first transport in line */
  res = gst_wfdrtspsrc_create_transports_string (src, &transports);
  if (res < 0 || transports == NULL)
    goto setup_transport_failed;

  if (strlen (transports) == 0) {
    g_free (transports);
    GST_DEBUG_OBJECT (src, "no transports found");
    goto setup_transport_failed;
  }

    /* now prepare the manager with the selected transport */
    if (!wfd_rtsp_manager_prepare_transport (manager,
		src->primary_rtpport, src->primary_rtpport+1)) {
      GST_DEBUG_OBJECT (src, "could not prepare transport");
      goto setup_failed;
    }

  /* create SETUP request */
  res = gst_rtsp_message_init_request (&request, GST_RTSP_SETUP, src->conninfo.location);
  if (res < 0) {
    g_free (transports);
    goto create_request_failed;
  }

  gst_rtsp_message_add_header (&request, GST_RTSP_HDR_USER_AGENT, (const gchar *)src->user_agent);
  gst_rtsp_message_add_header (&request, GST_RTSP_HDR_TRANSPORT, transports);
  g_free (transports);

  /* if the user wants a non default RTP packet size we add the blocksize
   * parameter */
  if (src->rtp_blocksize > 0) {
    hval = g_strdup_printf ("%d", src->rtp_blocksize);
    gst_rtsp_message_add_header (&request, GST_RTSP_HDR_BLOCKSIZE, hval);
    g_free (hval);
  }

  /* handle the code ourselves */
  if ((res = gst_wfdrtspsrc_send (src, src->conninfo.connection, &request, &response, &code) < 0))
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
  {
    gchar *resptrans = NULL;
    GstRTSPTransport transport = { 0 };

    gst_rtsp_message_get_header (&response, GST_RTSP_HDR_TRANSPORT,
        &resptrans, 0);
    if (!resptrans)
      goto no_transport;

    /* parse transport, go to next manager on parse error */
    if (gst_rtsp_transport_parse (resptrans, &transport) != GST_RTSP_OK) {
      GST_ERROR_OBJECT (src, "failed to parse transport %s", resptrans);
      goto setup_failed;
    }

    switch (transport.lower_transport) {
      case GST_RTSP_LOWER_TRANS_UDP:
        protocols = GST_RTSP_LOWER_TRANS_UDP;
        break;
      case GST_RTSP_LOWER_TRANS_TCP:
      case GST_RTSP_LOWER_TRANS_UDP_MCAST:
        GST_DEBUG_OBJECT (src, "transport %d is not supported",
            transport.lower_transport);
        goto setup_failed;
	  break;
      default:
        GST_DEBUG_OBJECT (src, "manager %p unknown transport %d", manager,
            transport.lower_transport);
        goto setup_failed;
        break;
    }

    /* now configure the manager with the selected transport */
    if (!wfd_rtsp_manager_configure_transport (manager, &transport)) {
      GST_DEBUG_OBJECT (src, "could not configure transport");
      goto setup_failed;
    }

    /* clean up our transport struct */
    gst_rtsp_transport_init (&transport);
    /* clean up used RTSP messages */
    gst_rtsp_message_unset (&request);
    gst_rtsp_message_unset (&response);
  }

  src->state = GST_RTSP_STATE_READY;

  return TRUE;

  /* ERRORS */
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


/* Parse all the wifi related messages.
 *
 * Uses wfdconfig utility to parse the wfd parameters sent from the server
 */

static GstRTSPResult
gst_wfdrtspsrc_retrieve_wifi_parameters (GstWFDRTSPSrc * src)
{
  GstRTSPResult res = GST_RTSP_OK;
  GstRTSPMessage request = { 0 };
  GstRTSPMessage response = { 0 };
  GstRTSPMessage message = { 0 };
  GstRTSPMethod method = { 0 };
  gchar *str = NULL;

  /* can't continue without a valid url */
  if (G_UNLIKELY (src->conninfo.url == NULL))
    goto no_url;

  if ((res = gst_wfdrtspsrc_conninfo_connect (src, &src->conninfo)) < 0)
    goto connect_failed;

  res = gst_wfdrtspsrc_connection_receive (src, src->conninfo.connection, &message,
    src->ptcp_timeout);
  if(res != GST_RTSP_OK)
    goto connect_failed;

  if(message.type == GST_RTSP_MESSAGE_REQUEST) {
    method = message.type_data.request.method;
    if(method == GST_RTSP_OPTIONS) {
      res = gst_wfdrtspsrc_handle_request (src, src->conninfo.connection, &message);
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
  gst_rtsp_message_add_header (&request, GST_RTSP_HDR_USER_AGENT, (const gchar*)src->user_agent);

  /* send OPTIONS */
  GST_DEBUG_OBJECT (src, "send options...");
  if ((res = gst_wfdrtspsrc_send (src, src->conninfo.connection, &request, &response,
          NULL)) < 0)
    goto send_error;

  /* parse OPTIONS */
  if (!gst_wfdrtspsrc_parse_methods (src, &response))
    goto methods_error;

 /* Receive request message from source */
receive_request_message:
  res = gst_wfdrtspsrc_connection_receive (src, src->conninfo.connection, &message,
    src->ptcp_timeout);
  if(res != GST_RTSP_OK)
    goto connect_failed;

  if(message.type == GST_RTSP_MESSAGE_REQUEST) {
    method = message.type_data.request.method;
    if(method == GST_RTSP_GET_PARAMETER ||method == GST_RTSP_SET_PARAMETER)
      gst_wfdrtspsrc_handle_request (src, src->conninfo.connection, &message);
    else
      goto methods_error;

    if (src->state != GST_RTSP_STATE_READY)
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

cleanup_error:
  {
    if (src->conninfo.connection) {
      GST_ERROR_OBJECT (src, "free connection");
      gst_wfdrtspsrc_conninfo_close (src, &src->conninfo, TRUE);
    }
    gst_rtsp_message_unset (&request);
    gst_rtsp_message_unset (&response);
    return res;
  }
}


/* Note : RTSP M1~M6 :
 *   WFD session capability negotiation
 */
static GstRTSPResult
gst_wfdrtspsrc_open (GstWFDRTSPSrc * src)
{
  GstRTSPResult res;

  if ((res = gst_wfdrtspsrc_retrieve_wifi_parameters (src)) < 0)
    goto open_failed;

done:
  gst_wfdrtspsrc_loop_end_cmd (src, CMD_OPEN, res);

  return res;

  /* ERRORS */
open_failed:
  {
    GST_ERROR_OBJECT (src, "failed to open");
    goto done;
  }
}

static void
gst_wfdrtspsrc_send_close_cmd (GstWFDRTSPSrc * src)
{
  gst_wfdrtspsrc_loop_send_cmd (src, CMD_CLOSE);
}

/* Note : RTSP M8 :
 *   Send TEARDOWN request to WFD source.
 */
static GstRTSPResult
gst_wfdrtspsrc_close (GstWFDRTSPSrc * src, gboolean only_close)
{
  WFDRTSPManager *manager = NULL;
  GstRTSPMessage request = { 0 };
  GstRTSPMessage response = { 0 };
  GstRTSPResult res = GST_RTSP_OK;

  GST_DEBUG_OBJECT (src, "TEARDOWN...");

  manager = src->manager;

  wfd_rtsp_manager_set_state (manager, GST_STATE_READY);

  if (src->state < GST_RTSP_STATE_READY) {
    GST_DEBUG_OBJECT (src, "not ready, doing cleanup");
    goto close;
  }

  if (only_close)
    goto close;

  if (!src->conninfo.connection || !src->conninfo.connected)
    goto close;

  if (!(src->methods & (GST_RTSP_PLAY | GST_RTSP_TEARDOWN)))
    goto not_supported;

  /* do TEARDOWN */
  res =
      gst_rtsp_message_init_request (&request, GST_RTSP_TEARDOWN, src->conninfo.url_str);
  if (res < 0)
    goto create_request_failed;

  if ((res =
          gst_wfdrtspsrc_send (src, src->conninfo.connection, &request, &response,
              NULL)) < 0)
    goto send_error;

  /* FIXME, parse result? */
  gst_rtsp_message_unset (&request);
  gst_rtsp_message_unset (&response);

close:
  /* close connections */
  GST_DEBUG_OBJECT (src, "closing connection...");
  gst_wfdrtspsrc_conninfo_close (src, &src->conninfo, TRUE);
  if (manager) {
    GST_DEBUG_OBJECT (src, "closing manager %p connection...", manager);
    gst_wfdrtspsrc_conninfo_close (src, &manager->conninfo, TRUE);
  }

  /* cleanup */
  gst_wfdrtspsrc_cleanup (src);

  src->state = GST_RTSP_STATE_INVALID;

  gst_wfdrtspsrc_loop_end_cmd (src, CMD_CLOSE, res);

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
gst_wfdrtspsrc_parse_rtpinfo (GstWFDRTSPSrc * src, gchar * rtpinfo)
{
  gchar **infos;
  gint i, j;

  GST_DEBUG_OBJECT (src, "parsing RTP-Info %s", rtpinfo);

  infos = g_strsplit (rtpinfo, ",", 0);
  for (i = 0; infos[i]; i++) {
    gchar **fields;
    WFDRTSPManager *manager;
    gint64 seqbase;
    gint64 timebase;

    GST_DEBUG_OBJECT (src, "parsing info %s", infos[i]);

    /* init values, types of seqbase and timebase are bigger than needed so we
     * can store -1 as uninitialized values */
    manager = NULL;
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
        manager = src->manager;
      } else if (g_str_has_prefix (fields[j], "seq=")) {
        seqbase = atoi (fields[j] + 4);
      } else if (g_str_has_prefix (fields[j], "rtptime=")) {
        timebase = g_ascii_strtoll (fields[j] + 8, NULL, 10);
      }
    }
    g_strfreev (fields);
    /* now we need to store the values for the caps of the manager */
    if (manager != NULL) {
      GST_DEBUG_OBJECT (src,
          "found manager %p, setting: seqbase %"G_GINT64_FORMAT", timebase %" G_GINT64_FORMAT,
          manager, seqbase, timebase);

      /* we have a manager, configure detected params */
      manager->seqbase = seqbase;
      manager->timebase = timebase;
    }
  }
  g_strfreev (infos);

  return TRUE;
}

static void
gst_wfdrtspsrc_send_play_cmd (GstWFDRTSPSrc * src)
{
  gst_wfdrtspsrc_loop_send_cmd (src, CMD_PLAY);
}

/* Note : RTSP M7 :
 *   Send PLAY request to WFD source. WFD source begins audio and/or video streaming.
 */
static GstRTSPResult
gst_wfdrtspsrc_play (GstWFDRTSPSrc * src)
{
  GstRTSPMessage request = { 0 };
  GstRTSPMessage response = { 0 };
  GstRTSPResult res = GST_RTSP_OK;
  gchar *hval;
  gint hval_idx;

  GST_DEBUG_OBJECT (src, "PLAY...");

  if (!src->conninfo.connection || !src->conninfo.connected)
    goto done;

  if (!(src->methods & GST_RTSP_PLAY))
    goto not_supported;

  if (src->state == GST_RTSP_STATE_PLAYING)
    goto was_playing;

  /* do play */
  res = gst_rtsp_message_init_request (&request, GST_RTSP_PLAY, src->conninfo.url_str);
  if (res < 0)
    goto create_request_failed;

  gst_rtsp_message_add_header (&request, GST_RTSP_HDR_USER_AGENT, (const gchar*)src->user_agent);

  if ((res = gst_wfdrtspsrc_send (src, src->conninfo.connection, &request, &response, NULL)) < 0)
    goto send_error;

  gst_rtsp_message_unset (&request);

  /* parse the RTP-Info header field (if ANY) to get the base seqnum and timestamp
   * for the RTP packets. If this is not present, we assume all starts from 0...
   * This is info for the RTP session manager that we pass to it in caps. */
  hval_idx = 0;
  while (gst_rtsp_message_get_header (&response, GST_RTSP_HDR_RTP_INFO,
          &hval, hval_idx++) == GST_RTSP_OK)
    gst_wfdrtspsrc_parse_rtpinfo (src, hval);

  gst_rtsp_message_unset (&response);

  /* configure the caps of the streams after we parsed all headers. */
  gst_wfdrtspsrc_configure_caps (src);

  /* set to PLAYING after we have configured the caps, otherwise we
   * might end up calling request_key (with SRTP) while caps are still
   * being configured. */
  wfd_rtsp_manager_set_state (src->manager, GST_STATE_PLAYING);

  src->state = GST_RTSP_STATE_PLAYING;

done:
  gst_wfdrtspsrc_loop_end_cmd (src, CMD_PLAY, res);

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
gst_wfdrtspsrc_send_pause_cmd (GstWFDRTSPSrc * src)
{
  gst_wfdrtspsrc_loop_send_cmd (src, CMD_PAUSE);
}

/* Note : RTSP M9  :
 *   Send PAUSE request to WFD source. WFD source pauses the audio video stream(s).
 */
static GstRTSPResult
gst_wfdrtspsrc_pause (GstWFDRTSPSrc * src)
{
  GstRTSPResult res = GST_RTSP_OK;
  GstRTSPMessage request = { 0 };
  GstRTSPMessage response = { 0 };

  GST_DEBUG_OBJECT (src, "PAUSE...");

  if (!src->conninfo.connection || !src->conninfo.connected)
    goto no_connection;

  if (!(src->methods & GST_RTSP_PAUSE))
    goto not_supported;

  if (src->state == GST_RTSP_STATE_READY)
    goto was_paused;

  if (gst_rtsp_message_init_request (&request, GST_RTSP_PAUSE, src->conninfo.url_str) < 0)
    goto create_request_failed;

  gst_rtsp_message_add_header (&request, GST_RTSP_HDR_USER_AGENT, (const gchar *)src->user_agent);

  if ((res = gst_wfdrtspsrc_send (src, src->conninfo.connection, &request, &response, NULL)) < 0)
    goto send_error;

  gst_rtsp_message_unset (&request);
  gst_rtsp_message_unset (&response);

  wfd_rtsp_manager_set_state (src->manager, GST_STATE_PAUSED);

no_connection:
  src->state = GST_RTSP_STATE_READY;

done:
  gst_wfdrtspsrc_loop_end_cmd (src, CMD_PAUSE, res);

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
gst_wfdrtspsrc_handle_message (GstBin * bin, GstMessage * message)
{
  GstWFDRTSPSrc *src;

  src = GST_WFDRTSPSRC (bin);

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
      WFDRTSPManager *manager;
      GstObject *message_src;
      gchar* debug = NULL;
      GError* error = NULL;

      message_src = GST_MESSAGE_SRC (message);

      gst_message_parse_error (message, &error, &debug);
      GST_ERROR_OBJECT (src, "error : %s", error->message);
      GST_ERROR_OBJECT (src, "debug : %s", debug);

      if (debug)
        g_free (debug);
      debug = NULL;
      g_error_free (error);

      manager = src->manager;
      if (!manager)
        goto forward;

      /* we ignore the RTCP udpsrc */
      if (manager->udpsrc[1] == GST_ELEMENT_CAST (message_src))
        goto done;

    forward:
      /* fatal but not our message, forward */
      GST_BIN_CLASS (parent_class)->handle_message (bin, message);
      break;
    done:
      gst_message_unref (message);
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
gst_wfdrtspsrc_thread (GstWFDRTSPSrc * src)
{
  GstRTSPResult ret = GST_RTSP_OK;
  gboolean running = FALSE;
  gint cmd;

  GST_OBJECT_LOCK (src);
  cmd = src->loop_cmd;
  src->loop_cmd = CMD_WAIT;
  GST_DEBUG_OBJECT (src, "got command %d", cmd);

  /* we got the message command, so ensure communication is possible again */
  gst_wfdrtspsrc_connection_flush (src, FALSE);

  /* we allow these to be interrupted */
  if (cmd == CMD_LOOP)
    src->waiting = TRUE;
  GST_OBJECT_UNLOCK (src);

  switch (cmd) {
    case CMD_OPEN:
      ret = gst_wfdrtspsrc_open (src);
      break;
    case CMD_PLAY:
      ret = gst_wfdrtspsrc_play (src);
      if (ret == GST_RTSP_OK)
        running = TRUE;
      break;
    case CMD_PAUSE:
      ret = gst_wfdrtspsrc_pause (src);
      if (ret == GST_RTSP_OK)
        running = TRUE;
      break;
    case CMD_CLOSE:
      ret = gst_wfdrtspsrc_close (src, FALSE);
      break;
    case CMD_LOOP:
      running = gst_wfdrtspsrc_loop (src);
      break;
     case CMD_REQUEST:
      ret = gst_wfdrtspsrc_send_request (src);
      if (ret == GST_RTSP_OK)
        running = TRUE;
      break;
   default:
      break;
  }

  GST_OBJECT_LOCK (src);
  /* and go back to sleep */
  if (src->loop_cmd == CMD_WAIT) {
    if (running)
      src->loop_cmd = CMD_LOOP;
    else if (src->task)
      gst_task_pause (src->task);
  }

  /* reset waiting */
  src->waiting = FALSE;
  GST_OBJECT_UNLOCK (src);
}

static gboolean
gst_wfdrtspsrc_start (GstWFDRTSPSrc * src)
{
  GST_DEBUG_OBJECT (src, "starting");

  GST_OBJECT_LOCK (src);

  src->state = GST_RTSP_STATE_INIT;

  src->loop_cmd = CMD_WAIT;

  if (src->task == NULL) {
    src->task = gst_task_new ((GstTaskFunction) gst_wfdrtspsrc_thread, src, NULL);
    if (src->task == NULL)
      goto task_error;

    gst_task_set_lock (src->task, &(GST_WFD_RTSP_TASK_GET_LOCK (src)));
  }
  GST_OBJECT_UNLOCK (src);

  return TRUE;

  /* ERRORS */
task_error:
  {
    GST_ERROR_OBJECT (src, "failed to create task");
    return FALSE;
  }
}

static gboolean
gst_wfdrtspsrc_stop (GstWFDRTSPSrc * src)
{
  GstTask *task;

  GST_DEBUG_OBJECT (src, "stopping");

  GST_OBJECT_LOCK (src);
  GST_DEBUG_OBJECT (src, "interrupt all command");
  src->waiting = TRUE;
  src->do_stop = TRUE;
  GST_OBJECT_UNLOCK (src);

  /* also cancels pending task */
  gst_wfdrtspsrc_loop_send_cmd (src, CMD_WAIT);

  GST_OBJECT_LOCK (src);
  if ((task = src->task)) {
    src->task = NULL;
    GST_OBJECT_UNLOCK (src);

    gst_task_stop (task);

    /* make sure it is not running */
    GST_WFD_RTSP_TASK_LOCK (src);
    GST_WFD_RTSP_TASK_UNLOCK (src);

    /* now wait for the task to finish */
    gst_task_join (task);

    /* and free the task */
    gst_object_unref (GST_OBJECT (task));
    GST_OBJECT_LOCK (src);
  }
  GST_OBJECT_UNLOCK (src);

  /* ensure synchronously all is closed and clean */
  gst_wfdrtspsrc_close (src, TRUE);

  return TRUE;
}


static GstStateChangeReturn
gst_wfdrtspsrc_change_state (GstElement * element, GstStateChange transition)
{
  GstWFDRTSPSrc *src;
  GstStateChangeReturn ret;

  src = GST_WFDRTSPSRC (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      GST_DEBUG_OBJECT (src, "NULL->READY");
      if (!gst_wfdrtspsrc_start (src))
        goto start_failed;
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      GST_DEBUG_OBJECT (src, "READY->PAUSED");
      gst_wfdrtspsrc_loop_send_cmd (src, CMD_OPEN);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      GST_DEBUG_OBJECT(src, "PAUSED->PLAYING");
      gst_wfdrtspsrc_loop_send_cmd (src, CMD_WAIT);
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
      gst_wfdrtspsrc_loop_send_cmd (src, CMD_PLAY);
      break;
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      ret = GST_STATE_CHANGE_NO_PREROLL;
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      gst_wfdrtspsrc_stop (src);
      break;
    default:
      break;
  }

done:
  GST_DEBUG_OBJECT (src, "state change is done");
  return ret;

start_failed:
  {
    GST_ERROR_OBJECT (src, "start failed");
    return GST_STATE_CHANGE_FAILURE;
  }
}

static gboolean
gst_wfdrtspsrc_send_event (GstElement * element, GstEvent * event)
{
  gboolean res;
  GstWFDRTSPSrc *src;

  src = GST_WFDRTSPSRC (element);

  if (GST_EVENT_IS_DOWNSTREAM (event)) {
    res = gst_wfdrtspsrc_push_event (src, event);
  } else {
    res = GST_ELEMENT_CLASS (parent_class)->send_event (element, event);
  }

  return res;
}

static void
gst_wfdrtspsrc_set_standby (GstWFDRTSPSrc * src)
{
  GST_OBJECT_LOCK(src);
  memset (&src->request_param, 0, sizeof(GstWFDRequestParam));
  src->request_param.type = WFD_STANDBY;
  GST_OBJECT_UNLOCK(src);

  gst_wfdrtspsrc_loop_send_cmd(src, CMD_REQUEST);
}

/*** GSTURIHANDLER INTERFACE *************************************************/
static GstURIType
gst_wfdrtspsrc_uri_get_type (GType type)
{
  return GST_URI_SRC;
}

static const gchar * const *
gst_wfdrtspsrc_uri_get_protocols (GType type)
{
  static const gchar *protocols[] =
      { "rtsp", NULL };

  return protocols;
}

static gchar *
gst_wfdrtspsrc_uri_get_uri (GstURIHandler * handler)
{
  GstWFDRTSPSrc *src = GST_WFDRTSPSRC (handler);

  /* should not dup */
  return src->conninfo.location;
}

static gboolean
gst_wfdrtspsrc_uri_set_uri (GstURIHandler * handler, const gchar * uri, GError **error)
{
  GstWFDRTSPSrc *src;
  GstRTSPResult res;
  GstRTSPUrl *newurl = NULL;

  src = GST_WFDRTSPSRC (handler);

  /* same URI, we're fine */
  if (src->conninfo.location && uri && !strcmp (uri, src->conninfo.location))
    goto was_ok;

    /* try to parse */
    GST_DEBUG_OBJECT (src, "parsing URI");
    if ((res = gst_rtsp_url_parse (uri, &newurl)) < 0)
      goto parse_error;


  /* if worked, free previous and store new url object along with the original
   * location. */
  GST_DEBUG_OBJECT (src, "configuring URI");
  g_free (src->conninfo.location);
  src->conninfo.location = g_strdup (uri);
  gst_rtsp_url_free (src->conninfo.url);
  src->conninfo.url = newurl;
  g_free (src->conninfo.url_str);
  if (newurl)
    src->conninfo.url_str = gst_rtsp_url_get_request_uri (src->conninfo.url);
  else
    src->conninfo.url_str = NULL;

  GST_DEBUG_OBJECT (src, "set uri: %s", GST_STR_NULL (uri));
  GST_DEBUG_OBJECT (src, "request uri is: %s",
      GST_STR_NULL (src->conninfo.url_str));

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
gst_wfdrtspsrc_uri_handler_init (gpointer g_iface, gpointer iface_data)
{
  GstURIHandlerInterface *iface = (GstURIHandlerInterface *) g_iface;

  iface->get_type = gst_wfdrtspsrc_uri_get_type;
  iface->get_protocols = gst_wfdrtspsrc_uri_get_protocols;
  iface->get_uri = gst_wfdrtspsrc_uri_get_uri;
  iface->set_uri = gst_wfdrtspsrc_uri_set_uri;
}

static GstRTSPResult _get_cea_resolution_and_set_to_src(GstWFDRTSPSrc *src, WFDVideoCEAResolution Resolution)
{
  WFDVideoCEAResolution CEARes = Resolution;
  switch(CEARes)
  {
    case WFD_CEA_UNKNOWN:
      break;
    case WFD_CEA_640x480P60:
      src->video_width=640;
      src->video_height=480;
      src->video_framerate=60;
      break;
    case WFD_CEA_720x480P60:
      src->video_width=720;
      src->video_height=480;
      src->video_framerate=60;
      break;
    case WFD_CEA_720x480I60:
      src->video_width=720;
      src->video_height=480;
      src->video_framerate=60;
      break;
    case WFD_CEA_720x576P50:
      src->video_width=720;
      src->video_height=576;
      src->video_framerate=50;
      break;
    case WFD_CEA_720x576I50:
       src->video_width=720;
      src->video_height=576;
      src->video_framerate=50;
      break;
    case WFD_CEA_1280x720P30:
      src->video_width=1280;
      src->video_height=720;
      src->video_framerate=30;
      break;
    case WFD_CEA_1280x720P60:
      src->video_width=1280;
      src->video_height=720;
      src->video_framerate=60;
      break;
    case WFD_CEA_1920x1080P30:
      src->video_width=1920;
      src->video_height=1080;
      src->video_framerate=30;
      break;
    case WFD_CEA_1920x1080P60:
      src->video_width=1920;
      src->video_height=1080;
      src->video_framerate=60;
      break;
    case WFD_CEA_1920x1080I60:
      src->video_width=1920;
      src->video_height=1080;
      src->video_framerate=60;
      break;
    case WFD_CEA_1280x720P25:
      src->video_width=1280;
      src->video_height=720;
      src->video_framerate=25;
      break;
    case WFD_CEA_1280x720P50:
      src->video_width=1280;
      src->video_height=720;
      src->video_framerate=50;
      break;
    case WFD_CEA_1920x1080P25:
      src->video_width=1920;
      src->video_height=1080;
      src->video_framerate=25;
      break;
    case WFD_CEA_1920x1080P50:
      src->video_width=1920;
      src->video_height=1080;
      src->video_framerate=50;
      break;
    case WFD_CEA_1920x1080I50:
      src->video_width=1920;
      src->video_height=1080;
      src->video_framerate=50;
      break;
    case WFD_CEA_1280x720P24:
      src->video_width=1280;
      src->video_height=720;
      src->video_framerate=24;
      break;
    case WFD_CEA_1920x1080P24:
      src->video_width=1920;
      src->video_height=1080;
      src->video_framerate=24;
      break;
    default:
      break;
  }
  return GST_RTSP_OK;
}

static GstRTSPResult _get_vesa_resolution_and_set_to_src(GstWFDRTSPSrc *src, WFDVideoVESAResolution Resolution)
{
  WFDVideoVESAResolution VESARes = Resolution;
  switch(VESARes)
  {
    case WFD_VESA_UNKNOWN:
      break;
    case WFD_VESA_800x600P30:
      src->video_width=800;
      src->video_height=600;
      src->video_framerate=30;
      break;
    case WFD_VESA_800x600P60:
      src->video_width=800;
      src->video_height=600;
      src->video_framerate=60;
      break;
    case WFD_VESA_1024x768P30:
      src->video_width=1024;
      src->video_height=768;
      src->video_framerate=30;
      break;
    case WFD_VESA_1024x768P60:
      src->video_width=1024;
      src->video_height=768;
      src->video_framerate=60;
      break;
    case WFD_VESA_1152x864P30:
      src->video_width=1152;
      src->video_height=864;
      src->video_framerate=30;
      break;
    case WFD_VESA_1152x864P60:
      src->video_width=1152;
      src->video_height=864;
      src->video_framerate=60;
      break;
    case WFD_VESA_1280x768P30:
      src->video_width=1280;
      src->video_height=768;
      src->video_framerate=30;
      break;
    case WFD_VESA_1280x768P60:
      src->video_width=1280;
      src->video_height=768;
      src->video_framerate=60;
      break;
    case WFD_VESA_1280x800P30:
      src->video_width=1280;
      src->video_height=800;
      src->video_framerate=30;
      break;
    case WFD_VESA_1280x800P60:
      src->video_width=1280;
      src->video_height=800;
      src->video_framerate=60;
      break;
    case WFD_VESA_1360x768P30:
      src->video_width=1360;
      src->video_height=768;
      src->video_framerate=30;
      break;
    case WFD_VESA_1360x768P60:
      src->video_width=1360;
      src->video_height=768;
      src->video_framerate=60;
      break;
    case WFD_VESA_1366x768P30:
      src->video_width=1366;
      src->video_height=768;
      src->video_framerate=30;
      break;
    case WFD_VESA_1366x768P60:
      src->video_width=1366;
      src->video_height=768;
      src->video_framerate=60;
      break;
    case WFD_VESA_1280x1024P30:
      src->video_width=1280;
      src->video_height=1024;
      src->video_framerate=30;
      break;
    case WFD_VESA_1280x1024P60:
      src->video_width=1280;
      src->video_height=1024;
      src->video_framerate=60;
      break;
    case WFD_VESA_1400x1050P30:
      src->video_width=1400;
      src->video_height=1050;
      src->video_framerate=30;
      break;
    case WFD_VESA_1400x1050P60:
      src->video_width=1400;
      src->video_height=1050;
      src->video_framerate=60;
      break;
    case WFD_VESA_1440x900P30:
      src->video_width=1440;
      src->video_height=900;
      src->video_framerate=30;
      break;
    case WFD_VESA_1440x900P60:
      src->video_width=1440;
      src->video_height=900;
      src->video_framerate=60;
      break;
    case WFD_VESA_1600x900P30:
      src->video_width=1600;
      src->video_height=900;
      src->video_framerate=30;
      break;
    case WFD_VESA_1600x900P60:
      src->video_width=1600;
      src->video_height=900;
      src->video_framerate=60;
      break;
    case WFD_VESA_1600x1200P30:
      src->video_width=1600;
      src->video_height=1200;
      src->video_framerate=30;
      break;
    case WFD_VESA_1600x1200P60:
      src->video_width=1600;
      src->video_height=1200;
      src->video_framerate=60;
      break;
    case WFD_VESA_1680x1024P30:
      src->video_width=1680;
      src->video_height=1024;
      src->video_framerate=30;
      break;
    case WFD_VESA_1680x1024P60:
      src->video_width=1680;
      src->video_height=1024;
      src->video_framerate=60;
      break;
    case WFD_VESA_1680x1050P30:
      src->video_width=1680;
      src->video_height=1050;
      src->video_framerate=30;
      break;
    case WFD_VESA_1680x1050P60:
      src->video_width=1680;
      src->video_height=1050;
      src->video_framerate=60;
      break;
    case WFD_VESA_1920x1200P30:
      src->video_width=1920;
      src->video_height=1200;
      src->video_framerate=30;
      break;
    case WFD_VESA_1920x1200P60:
      src->video_width=1920;
      src->video_height=1200;
      src->video_framerate=60;
      break;
    default:
      break;
  }
  return GST_RTSP_OK;
}

static GstRTSPResult _get_hh_resolution_and_set_to_src(GstWFDRTSPSrc *src, WFDVideoHHResolution Resolution)
{
  WFDVideoHHResolution HHRes = Resolution;
  switch(HHRes)
  {
    case WFD_HH_UNKNOWN:
      break;
    case WFD_HH_800x480P30:
      src->video_width=800;
      src->video_height=480;
      src->video_framerate=30;
      break;
    case WFD_HH_800x480P60:
      src->video_width=800;
      src->video_height=480;
      src->video_framerate=60;
      break;
    case WFD_HH_854x480P30:
      src->video_width=854;
      src->video_height=480;
      src->video_framerate=30;
      break;
    case WFD_HH_854x480P60:
      src->video_width=854;
      src->video_height=480;
      src->video_framerate=60;
      break;
    case WFD_HH_864x480P30:
      src->video_width=864;
      src->video_height=480;
      src->video_framerate=30;
      break;
    case WFD_HH_864x480P60:
      src->video_width=864;
      src->video_height=480;
      src->video_framerate=60;
      break;
    case WFD_HH_640x360P30:
      src->video_width=640;
      src->video_height=360;
      src->video_framerate=30;
      break;
    case WFD_HH_640x360P60:
      src->video_width=640;
      src->video_height=360;
      src->video_framerate=60;
      break;
    case WFD_HH_960x540P30:
      src->video_width=960;
      src->video_height=540;
      src->video_framerate=30;
      break;
    case WFD_HH_960x540P60:
      src->video_width=960;
      src->video_height=540;
      src->video_framerate=60;
      break;
    case WFD_HH_848x480P30:
      src->video_width=848;
      src->video_height=480;
      src->video_framerate=30;
      break;
    case WFD_HH_848x480P60:
      src->video_width=848;
      src->video_height=480;
      src->video_framerate=60;
      break;
    default:
      break;
  }
  return GST_RTSP_OK;
}

static GstRTSPResult
gst_wfdrtspsrc_get_audio_parameter(GstWFDRTSPSrc * src, WFDMessage * msg)
{
  WFDAudioFormats audio_format = WFD_AUDIO_UNKNOWN;
  WFDAudioChannels audio_channels = WFD_CHANNEL_UNKNOWN;
  WFDAudioFreq audio_frequency = WFD_FREQ_UNKNOWN;
  guint audio_bitwidth = 0;
  guint32 audio_latency = 0;
  WFDResult wfd_res = WFD_OK;

  WFDCONFIG_GET_PREFERED_AUDIO_FORMAT(msg, &audio_format, &audio_frequency, &audio_channels, &audio_bitwidth, &audio_latency);
  if(wfd_res != WFD_OK) {
    GST_ERROR("Failed to get prefered audio format.");
    return GST_RTSP_ERROR;
  }

  src->audio_format = g_strdup(msg->audio_codecs->list->audio_format);
  if(audio_frequency == WFD_FREQ_48000)
    audio_frequency = 48000;
  else if(audio_frequency == WFD_FREQ_44100)
    audio_frequency = 44100;

  if(audio_channels == WFD_CHANNEL_2)
    audio_channels = 2;
  else if(audio_channels == WFD_CHANNEL_4)
    audio_channels = 4;
  else if(audio_channels == WFD_CHANNEL_6)
    audio_channels = 6;
  else if(audio_channels == WFD_CHANNEL_8)
    audio_channels = 8;

  src->audio_channels = audio_channels;
  src->audio_frequency = audio_frequency;
  src->audio_bitwidth = audio_bitwidth;

  return GST_RTSP_OK;
}

static GstRTSPResult
gst_wfdrtspsrc_get_video_parameter(GstWFDRTSPSrc * src, WFDMessage * msg)
{
  WFDVideoCodecs cvCodec = WFD_VIDEO_UNKNOWN;
  WFDVideoNativeResolution cNative = WFD_VIDEO_CEA_RESOLUTION;
  guint64 cNativeResolution = 0;
  WFDVideoCEAResolution cCEAResolution = WFD_CEA_UNKNOWN;
  WFDVideoVESAResolution cVESAResolution = WFD_VESA_UNKNOWN;
  WFDVideoHHResolution cHHResolution = WFD_HH_UNKNOWN;
  WFDVideoH264Profile cProfile = WFD_H264_UNKNOWN_PROFILE;
  WFDVideoH264Level cLevel = WFD_H264_LEVEL_UNKNOWN;
  guint32 cMaxHeight = 0;
  guint32 cMaxWidth = 0;
  guint32 cmin_slice_size = 0;
  guint32 cslice_enc_params = 0;
  guint cframe_rate_control = 0;
  guint cvLatency = 0;
  WFDResult wfd_res = WFD_OK;

  WFDCONFIG_GET_PREFERED_VIDEO_FORMAT(msg, &cvCodec, &cNative, &cNativeResolution,
      &cCEAResolution, &cVESAResolution, &cHHResolution,
      &cProfile, &cLevel, &cvLatency, &cMaxHeight,
      &cMaxWidth, &cmin_slice_size, &cslice_enc_params, &cframe_rate_control);
  if(wfd_res != WFD_OK) {
      GST_ERROR("Failed to get prefered video format.");
      return GST_RTSP_ERROR;
  }
#if 0
  switch(cNative)
  {
    case WFD_VIDEO_CEA_RESOLUTION:
      _get_cea_resolution_and_set_to_src(src, cCEAResolution);
      break;
    case WFD_VIDEO_VESA_RESOLUTION:
      _get_vesa_resolution_and_set_to_src(src, cVESAResolution);
      break;
    case WFD_VIDEO_HH_RESOLUTION:
      _get_hh_resolution_and_set_to_src(src, cHHResolution);
      break;
    default:
      break;
  }
#endif

  if(cCEAResolution != WFD_CEA_UNKNOWN) {
    _get_cea_resolution_and_set_to_src(src, cCEAResolution);
  }
  else if(cVESAResolution != WFD_VESA_UNKNOWN) {
    _get_vesa_resolution_and_set_to_src(src, cVESAResolution);
  }
   else if(cHHResolution != WFD_HH_UNKNOWN) {
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


static GstRTSPResult
gst_wfdrtspsrc_message_dump (GstRTSPMessage * msg)
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
