/*
 * wfdsrc
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
* SECTION:element-wfdsrc
*
* Makes a connection to an RTSP server and read the data.
* Device recognition is through wifi direct.
* wfdsrc strictly follows Wifi display specification.
*
* RTSP supports transport over TCP or UDP in unicast or multicast mode. By
* default wfdsrc will negotiate a connection in the following order:
* UDP unicast/UDP multicast/TCP. The order cannot be changed but the allowed
* protocols can be controlled with the #GstWFDSrc:protocols property.
*
* wfdsrc currently understands WFD capability negotiation messages
*
* wfdsrc will internally instantiate an RTP session manager element
* that will handle the RTCP messages to and from the server, jitter removal,
* packet reordering along with providing a clock for the pipeline.
* This feature is implemented using the gstrtpbin element.
*
* wfdsrc acts like a live source and will therefore only generate data in the
* PLAYING state.
*
* <refsect2>
* <title>Example launch line</title>
* |[
* gst-launch wfdsrc location=rtsp://some.server/url ! fakesink
* ]| Establish a connection to an RTSP server and send the raw RTP packets to a
* fakesink.
* </refsect2>
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstwfdsrc.h"

GST_DEBUG_CATEGORY_STATIC (wfdsrc_debug);
#define GST_CAT_DEFAULT (wfdsrc_debug)

/* signals and args */
enum
{
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_DO_RTCP,
  PROP_LATENCY,
  PROP_UDP_BUFFER_SIZE,
  PROP_UDP_TIMEOUT,
  PROP_LAST
};

#define DEFAULT_DO_RTCP          TRUE
#define DEFAULT_LATENCY_MS       2000
#define DEFAULT_UDP_BUFFER_SIZE  0x80000
#define DEFAULT_UDP_TIMEOUT          10000000


/* object */
static void gst_wfd_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_wfd_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

/* wfdbasesrc */
static GstRTSPResult gst_wfd_src_configure_transport (GstWFDBaseSrc *bsrc,
    GstRTSPTransport * transport);
static GstRTSPResult gst_wfd_src_prepare_transport (GstWFDBaseSrc *bsrc,
    gint rtpport, gint rtcpport);
static gboolean gst_wfd_src_push_event (GstWFDBaseSrc *bsrc, GstEvent * event);
static void gst_wfd_src_set_state (GstWFDBaseSrc *src, GstState state);
static void gst_wfd_src_cleanup (GstWFDBaseSrc *bsrc);


//static guint gst_wfd_srcext_signals[LAST_SIGNAL] = { 0 };

#define _do_init \
    GST_DEBUG_CATEGORY_INIT (wfdsrc_debug, "wfdsrc", 0, "Wi-Fi Display Sink source");

#define gst_wfd_src_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstWFDSrc, gst_wfd_src, GST_TYPE_WFD_BASE_SRC, _do_init);

static void
gst_wfd_src_class_init (GstWFDSrcClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstWFDBaseSrcClass *gstwfdbasesrc_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstwfdbasesrc_class = (GstWFDBaseSrcClass *) klass;

  gobject_class->set_property = gst_wfd_src_set_property;
  gobject_class->get_property = gst_wfd_src_get_property;

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


  gst_element_class_set_static_metadata (gstelement_class, "Wi-Fi Display Sink source element",
      "Source/Network",
      "Negotiate the capability and receive the RTP packets from the Wi-Fi Display source",
      "YeJin Cho <cho.yejin@samsung.com>");

  gstwfdbasesrc_class->configure_transport = GST_DEBUG_FUNCPTR (gst_wfd_src_configure_transport);
  gstwfdbasesrc_class->prepare_transport = GST_DEBUG_FUNCPTR (gst_wfd_src_prepare_transport);
  gstwfdbasesrc_class->push_event = GST_DEBUG_FUNCPTR (gst_wfd_src_push_event);
  gstwfdbasesrc_class->set_state = GST_DEBUG_FUNCPTR (gst_wfd_src_set_state);
  gstwfdbasesrc_class->cleanup = GST_DEBUG_FUNCPTR (gst_wfd_src_cleanup);
}

static void
gst_wfd_src_init (GstWFDSrc* src)
{
  gint i;

  src->do_rtcp = DEFAULT_DO_RTCP;
  src->latency = DEFAULT_LATENCY_MS;
  src->udp_buffer_size = DEFAULT_UDP_BUFFER_SIZE;
  src->udp_timeout = DEFAULT_UDP_TIMEOUT;

  src->session = NULL;
  src->wfdrtpbuffer = NULL;
  for (i=0; i<2; i++) {
    src->channelpad[i] = NULL;
    src->udpsrc[i] = NULL;
    src->udpsink[i] = NULL;
  }
  src->blockid = 0;
  src->blockedpad = NULL;
}

static void
gst_wfd_src_set_property (GObject * object, guint prop_id, const GValue * value,
    GParamSpec * pspec)
{
  GstWFDSrc *src = GST_WFD_SRC (object);

  switch (prop_id) {
    case PROP_DO_RTCP:
      src->do_rtcp = g_value_get_boolean (value);
      break;
    case PROP_LATENCY:
      src->latency = g_value_get_uint (value);
      break;
    case PROP_UDP_BUFFER_SIZE:
      src->udp_buffer_size = g_value_get_int (value);
      break;
    case PROP_UDP_TIMEOUT:
      src->udp_timeout = g_value_get_uint64 (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_wfd_src_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstWFDSrc *src = GST_WFD_SRC (object);

  switch (prop_id) {
    case PROP_DO_RTCP:
      g_value_set_boolean (value, src->do_rtcp);
      break;
    case PROP_LATENCY:
      g_value_set_uint (value, src->latency);
      break;
    case PROP_UDP_BUFFER_SIZE:
      g_value_set_int (value, src->udp_buffer_size);
      break;
    case PROP_UDP_TIMEOUT:
      g_value_set_uint64 (value, src->udp_timeout);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_wfd_src_set_state (GstWFDBaseSrc *bsrc, GstState state)
{
  GstWFDSrc *src = GST_WFD_SRC (bsrc);
  gint i;

  GST_DEBUG_OBJECT(src, "try to set %s state", gst_element_state_get_name(state));

  for (i=0; i <2; i++) {
    if (src->udpsrc[i])
      gst_element_set_state (src->udpsrc[i], state);
    if (src->udpsink[i])
      gst_element_set_state (src->udpsink[i], state);
  }

  if (src->session)
    gst_element_set_state (src->session, state);

  if (src->wfdrtpbuffer)
    gst_element_set_state (src->wfdrtpbuffer, state);
}

static void
gst_wfd_src_cleanup (GstWFDBaseSrc *bsrc)
{
  GstWFDSrc *src = GST_WFD_SRC (bsrc);
  gint i;

  GST_DEBUG_OBJECT (src, "cleanup");

  for (i=0; i <2; i++) {
    if (src->channelpad[i]) {
      gst_object_unref (src->channelpad[i]);
      src->channelpad[i] = NULL;
    }
    if (src->udpsrc[i]) {
      gst_element_set_state (src->udpsrc[i], GST_STATE_NULL);
      gst_bin_remove (GST_BIN_CAST (src), src->udpsrc[i]);
      gst_object_unref (src->udpsrc[i]);
      src->udpsrc[i] = NULL;
    }
    if (src->udpsink[i]) {
      gst_element_set_state (src->udpsink[i], GST_STATE_NULL);
      gst_bin_remove (GST_BIN_CAST (src), src->udpsink[i]);
      gst_object_unref (src->udpsink[i]);
      src->udpsink[i] = NULL;
    }
  }
  if (src->session) {
    gst_element_set_state (src->session, GST_STATE_NULL);
    gst_bin_remove (GST_BIN_CAST (src), src->session);
    gst_object_unref (src->session);
    src->session = NULL;
  }
  if (src->wfdrtpbuffer) {
    gst_element_set_state (src->wfdrtpbuffer, GST_STATE_NULL);
    gst_bin_remove (GST_BIN_CAST (src), src->wfdrtpbuffer);
    gst_object_unref (src->wfdrtpbuffer);
    src->wfdrtpbuffer = NULL;
  }
}


static GstRTSPResult
gst_wfd_src_prepare_transport (GstWFDBaseSrc *bsrc,
    gint rtpport, gint rtcpport)
{
  GstWFDSrc *src = GST_WFD_SRC (bsrc);
  GstStateChangeReturn ret;
  GstElement *udpsrc0, *udpsrc1;
  gint tmp_rtp, tmp_rtcp;
  const gchar *host;

  udpsrc0 = NULL;
  udpsrc1 = NULL;

  if (bsrc->is_ipv6)
    host = "udp://[::0]";
  else
    host = "udp://0.0.0.0";

  /* try to allocate 2 UDP ports */
  udpsrc0 = gst_element_make_from_uri (GST_URI_SRC, host, NULL, NULL);
  if (udpsrc0 == NULL)
    goto no_udp_protocol;
  g_object_set (G_OBJECT (udpsrc0), "port", rtpport, "reuse", FALSE, NULL);

  if (src->udp_buffer_size != 0)
    g_object_set (G_OBJECT (udpsrc0), "buffer-size", src->udp_buffer_size,
        NULL);

  GST_DEBUG_OBJECT (src, "starting RTP on port %d", rtpport);
  ret = gst_element_set_state (udpsrc0, GST_STATE_READY);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    GST_ERROR_OBJECT (src, "Unable to make udpsrc from RTP port %d", rtpport);
    goto no_ports;
  }

  g_object_get (G_OBJECT (udpsrc0), "port", &tmp_rtp, NULL);
  GST_DEBUG_OBJECT (src, "got RTP port %d", tmp_rtp);

  /* check if port is even */
  if ((tmp_rtp & 0x01) != 0) {
    GST_DEBUG_OBJECT (src, "RTP port not even");
    /* port not even, free RTP udpsrc */
    goto no_ports;
  }

  /* allocate port+1 for RTCP now */
  udpsrc1 = gst_element_make_from_uri (GST_URI_SRC, host, NULL, NULL);
  if (udpsrc1 == NULL)
    goto no_udp_protocol;

  /* set port */
  g_object_set (G_OBJECT (udpsrc1), "port", rtcpport, "reuse", FALSE, NULL);

  GST_DEBUG_OBJECT (src, "starting RTCP on port %d", rtcpport);
  ret = gst_element_set_state (udpsrc1, GST_STATE_READY);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    GST_ERROR_OBJECT (src, "Unable to make udpsrc from RTCP port %d", rtcpport);
    goto no_ports;
  }

  /* all fine, do port check */
  g_object_get (G_OBJECT (udpsrc0), "port", &tmp_rtp, NULL);
  g_object_get (G_OBJECT (udpsrc1), "port", &tmp_rtcp, NULL);

  /* this should not happen... */
  if (rtpport != tmp_rtp || rtcpport != tmp_rtcp)
    goto port_error;

  /* we keep these elements, we configure all in configure_transport when the
   * server told us to really use the UDP ports. */
  src->udpsrc[0] = gst_object_ref_sink (udpsrc0);
  src->udpsrc[1] = gst_object_ref_sink (udpsrc1);
  gst_element_set_locked_state (src->udpsrc[0], TRUE);
  gst_element_set_locked_state (src->udpsrc[1], TRUE);

  return GST_RTSP_OK;

  /* ERRORS */
no_udp_protocol:
  {
    GST_DEBUG_OBJECT (src, "could not get UDP source");
    goto cleanup;
  }
no_ports:
  {
    GST_DEBUG_OBJECT (src, "could not allocate UDP port pair");
    goto cleanup;
  }
port_error:
  {
    GST_DEBUG_OBJECT (src, "ports don't match rtp: %d<->%d, rtcp: %d<->%d",
        tmp_rtp, rtpport, tmp_rtcp, rtcpport);
    goto cleanup;
  }
cleanup:
  {
    if (udpsrc0) {
      gst_element_set_state (udpsrc0, GST_STATE_NULL);
      gst_object_unref (udpsrc0);
    }
    if (udpsrc1) {
      gst_element_set_state (udpsrc1, GST_STATE_NULL);
      gst_object_unref (udpsrc1);
    }
    return GST_RTSP_ERROR;
  }
}

static void
on_bye_ssrc (GObject * session, GObject * source, GstWFDSrc *src)
{
  GST_DEBUG_OBJECT (src, "source in session received BYE");

  //gst_wfdsrc_do_stream_eos (src, manager);
}

static void
on_new_ssrc (GObject * session, GObject * source, GstWFDSrc *src)
{
  GST_DEBUG_OBJECT (src, "source in session received NEW");
}

static void
on_timeout (GObject * session, GObject * source, GstWFDSrc *src)
{
  GST_DEBUG_OBJECT (src, "source in session timed out");

  //gst_wfdsrc_do_stream_eos (src, manager);
}

static void
on_ssrc_active (GObject * session, GObject * source, GstWFDSrc *src)
{
  GST_DEBUG_OBJECT (src, "source in session  is active");
}


static GstCaps *
request_pt_map_for_wfdrtpbuffer (GstElement * wfdrtpbuffer, guint pt, GstWFDSrc *src)
{
  GstCaps *caps;

  GST_DEBUG_OBJECT (src, "getting pt map for pt %d", pt);

  GST_WFD_BASE_STATE_LOCK (src);
  caps = GST_WFD_BASE_SRC_CAST(src)->caps;
  if (caps)
    gst_caps_ref (caps);
  GST_WFD_BASE_STATE_UNLOCK (src);

  return caps;
}

static GstCaps *
request_pt_map_for_session (GstElement * session, guint session_id, guint pt, GstWFDSrc *src)
{
  GstCaps *caps;

  GST_DEBUG_OBJECT (src, "getting pt map for pt %d in session %d", pt, session_id);

  GST_WFD_BASE_STATE_LOCK (src);
  caps = GST_WFD_BASE_SRC_CAST(src)->caps;
  if (caps)
    gst_caps_ref (caps);
  GST_WFD_BASE_STATE_UNLOCK (src);

  return caps;
}

static gboolean
gst_wfd_src_configure_manager (GstWFDSrc *src)
{
  GstPad *pad = NULL;

  /* construct wfdsrc */
  src->session = gst_element_factory_make ("rtpsession", "wfdsrc_session");
  if (G_UNLIKELY (src->session == NULL)) {
    GST_ERROR_OBJECT (src, "could not create gstrtpsession element");
    return FALSE;
  }  else {
    g_signal_connect (src->session, "on-bye-ssrc", (GCallback) on_bye_ssrc,
        src);
    g_signal_connect (src->session, "on-bye-timeout", (GCallback) on_timeout,
        src);
    g_signal_connect (src->session, "on-timeout", (GCallback) on_timeout,
        src);
    g_signal_connect (src->session, "on-ssrc-active",
        (GCallback) on_ssrc_active, src);
    g_signal_connect (src->session, "on-new-ssrc", (GCallback) on_new_ssrc,
        src);
    g_signal_connect (src->session, "request-pt-map",
        (GCallback) request_pt_map_for_session, src);

    g_object_set (G_OBJECT(src->session), "rtcp-min-interval", (guint64)1000000000, NULL);

    src->channelpad[0] = gst_element_get_request_pad (src->session, "recv_rtp_sink");
    if (G_UNLIKELY (src->channelpad[0]  == NULL)) {
      GST_ERROR_OBJECT (src, "could not create rtp channel pad");
      return FALSE;
    }

    src->channelpad[1] = gst_element_get_request_pad (src->session, "recv_rtcp_sink");
    if (G_UNLIKELY (src->channelpad[1]  == NULL)) {
      GST_ERROR_OBJECT (src, "could not create rtcp channel pad");
      return FALSE;
    }

    /* we manage session element */
    gst_element_set_locked_state (src->session, TRUE);

    if (!gst_bin_add(GST_BIN_CAST(src), src->session)) {
      GST_ERROR_OBJECT (src, "failed to add rtpsession to wfdsrc");
      return FALSE;
    }
  }

  src->wfdrtpbuffer = gst_element_factory_make ("wfdrtpbuffer", "wfdsrc_wfdrtpbuffer");
  if (G_UNLIKELY (src->wfdrtpbuffer == NULL)) {
    GST_ERROR_OBJECT (src, "could not create wfdrtpbuffer element");
    return FALSE;
  } else {
    /* configure latency and packet lost */
    g_object_set (src->wfdrtpbuffer, "latency", src->latency, NULL);

    g_signal_connect (src->wfdrtpbuffer, "request-pt-map",
	 (GCallback) request_pt_map_for_wfdrtpbuffer, src);

    /* we manage wfdrtpbuffer element */
    gst_element_set_locked_state (src->wfdrtpbuffer, TRUE);

    if (!gst_bin_add(GST_BIN_CAST(src), src->wfdrtpbuffer)) {
      GST_ERROR_OBJECT (src, "failed to add wfdrtpbuffer to wfdsrc");
      return FALSE;
    }
  }

  if (!gst_element_link_many(src->session, src->wfdrtpbuffer, NULL)) {
    GST_ERROR_OBJECT (src, "failed to link elements for wfdsrc");
    return FALSE;
  }

  if (!gst_element_sync_state_with_parent(src->session)) {
    GST_ERROR_OBJECT (src, "failed for %s to sync state with wfdsrc", GST_ELEMENT_NAME(src->session));
    return FALSE;
  }

  if (!gst_element_sync_state_with_parent(src->wfdrtpbuffer)) {
    GST_ERROR_OBJECT (src, "failed for %s to sync state with wfdsrc", GST_ELEMENT_NAME(src->wfdrtpbuffer));
    return FALSE;
  }

  /* set ghost pad */
  pad = gst_element_get_static_pad (src->wfdrtpbuffer, "src");
  if (G_UNLIKELY (pad == NULL)) {
    GST_ERROR_OBJECT (src, "failed to get src pad of wfdrtpbuffer for setting ghost pad of wfdsrc");
    return FALSE;
  }

  if (!gst_wfd_base_src_set_target(GST_WFD_BASE_SRC(src), pad)) {
    GST_ERROR_OBJECT (src, "failed to set target pad of ghost pad");
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_wfd_src_configure_udp_sinks (GstWFDSrc *src, GstRTSPTransport * transport)
{
  GstPad *pad = NULL;
  GSocket *socket = NULL;
  gint rtp_port = -1, rtcp_port = -1;
  gboolean do_rtcp;
  const gchar *destination =  NULL;
  gchar *uri = NULL;

  /* get transport info */
  gst_wfd_base_src_get_transport_info (GST_WFD_BASE_SRC(src), transport, &destination,
      &rtp_port, &rtcp_port);

  /* it's possible that the server does not want us to send RTCP in which case
   * the port is -1 */
  do_rtcp = (rtcp_port != -1 && src->session != NULL && src->do_rtcp);

  /* we need a destination when we have RTCP RR */
  if (destination == NULL && do_rtcp)
    goto no_destination;

  if (do_rtcp) {
    GstPad *rtcppad = NULL;

    GST_DEBUG_OBJECT (src, "configure RTCP UDP sink for %s:%d", destination,
        rtcp_port);

    uri = g_strdup_printf ("udp://%s:%d", destination, rtcp_port);
    src->udpsink[1] = gst_element_make_from_uri (GST_URI_SINK, uri, NULL, NULL);
    g_free (uri);
    if (src->udpsink[1] == NULL)
      goto no_sink_element;

    /* don't join multicast group, we will have the source socket do that */
    /* no sync or async state changes needed */
    g_object_set (G_OBJECT (src->udpsink[1]), "auto-multicast", FALSE,
        "loop", FALSE, "sync", FALSE, "async", FALSE, NULL);

    if (src->udpsrc[1]) {
      /* configure socket, we give it the same UDP socket as the udpsrc for RTCP
       * because some servers check the port number of where it sends RTCP to identify
       * the RTCP packets it receives */
      g_object_get (G_OBJECT (src->udpsrc[1]), "used-socket", &socket, NULL);
      GST_DEBUG_OBJECT (src, "RTCP UDP src has sock %p", socket);
      /* configure socket and make sure udpsink does not close it when shutting
       * down, it belongs to udpsrc after all. */
      g_object_set (G_OBJECT (src->udpsink[1]), "socket", socket,
          "close-socket", FALSE, NULL);
      g_object_unref (socket);
    }

    /* we don't want to consider this a sink */
    GST_OBJECT_FLAG_UNSET (src->udpsink[1], GST_ELEMENT_FLAG_SINK);

    /* we keep this playing always */
    gst_element_set_locked_state (src->udpsink[1], TRUE);
    gst_element_set_state (src->udpsink[1], GST_STATE_PLAYING);

    gst_object_ref (src->udpsink[1]);
    gst_bin_add (GST_BIN_CAST (src), src->udpsink[1]);

    rtcppad = gst_element_get_static_pad (src->udpsink[1], "sink");

    /* get session RTCP pad */
    pad = gst_element_get_request_pad (src->session, "send_rtcp_src");

    /* and link */
    if (pad && rtcppad) {
      gst_pad_link_full (pad, rtcppad, GST_PAD_LINK_CHECK_NOTHING);
      gst_object_unref (pad);
      gst_object_unref (rtcppad);
    }
  }

  return TRUE;

  /* ERRORS */
no_destination:
  {
    GST_DEBUG_OBJECT (src, "no destination address specified");
    return FALSE;
  }
no_sink_element:
  {
    GST_DEBUG_OBJECT (src, "no UDP sink element found");
    return FALSE;
  }
}

static void
pad_blocked (GstPad * pad, gboolean blocked, GstWFDSrc *src)
{
  GST_DEBUG_OBJECT (src, "pad %s:%s blocked, activating streams",
      GST_DEBUG_PAD_NAME (pad));

  if (src->udpsrc[0]) {
    /* remove timeout, we are streaming now and timeouts will be handled by
     * the session manager and jitter buffer */
    g_object_set (G_OBJECT (src->udpsrc[0]), "timeout", (guint64) 0, NULL);
  }

  /* activate the streams */
  gst_wfd_base_src_activate (GST_WFD_BASE_SRC(src));

  /* unblock all pads */
  if (src->blockedpad && src->blockid != 0) {
    GST_DEBUG_OBJECT (src, "unblocking blocked pad");
    gst_pad_remove_probe (src->blockedpad, src->blockid);
    src->blockid = 0;
    src->blockedpad = NULL;
  }
}

static gboolean
gst_wfd_src_configure_udp (GstWFDSrc *src)
{
  GstPad *outpad;

  /* we manage the UDP elements now. For unicast, the UDP sources where
   * allocated in the stream when we suggested a transport. */
  if (src->udpsrc[0]) {
    GstCaps *caps;

    gst_element_set_locked_state (src->udpsrc[0], TRUE);
    gst_bin_add (GST_BIN_CAST (src), src->udpsrc[0]);

    GST_DEBUG_OBJECT (src, "setting up UDP source");

    /* configure a timeout on the UDP port. When the timeout message is
     * posted */
    g_object_set (G_OBJECT (src->udpsrc[0]), "timeout",
        src->udp_timeout * 1000, NULL);

    caps = gst_caps_new_simple ("application/x-rtp",
                    "media", G_TYPE_STRING, "video", "payload", G_TYPE_INT, 33,
                    "clock-rate", G_TYPE_INT, 90000, NULL);
    g_object_set (src->udpsrc[0], "caps", caps, NULL);
    gst_caps_unref (caps);

    /* get output pad of the UDP source. */
    outpad = gst_element_get_static_pad (src->udpsrc[0], "src");

    /* save it so we can unblock */
    src->blockedpad = outpad;

    /* configure pad block on the pad. As soon as there is dataflow on the
     * UDP source, we know that UDP is not blocked by a firewall and we can
     * configure all the streams to let the application autoplug decoders. */
    src->blockid =
        gst_pad_add_probe (src->blockedpad,
        GST_PAD_PROBE_TYPE_BLOCK | GST_PAD_PROBE_TYPE_BUFFER |
        GST_PAD_PROBE_TYPE_BUFFER_LIST, (GstPadProbeCallback)pad_blocked, src, NULL);

    if (src->channelpad[0]) {
      GST_DEBUG_OBJECT (src, "connecting UDP source 0 to session");
      /* configure for UDP delivery, we need to connect the UDP pads to
       * the session plugin. */
      gst_pad_link_full (outpad, src->channelpad[0],
          GST_PAD_LINK_CHECK_NOTHING);
      /* we connected to pad-added signal to get pads from the manager */
    } else {
      /* leave unlinked */
    }
  }

  /* RTCP port */
  if (src->udpsrc[1]) {
    GstCaps *caps;

    gst_element_set_locked_state (src->udpsrc[1], TRUE);
    gst_bin_add (GST_BIN_CAST (src), src->udpsrc[1]);

    caps = gst_caps_new_empty_simple ("application/x-rtcp");
    g_object_set (src->udpsrc[1], "caps", caps, NULL);
    gst_caps_unref (caps);

    if (src->channelpad[1]) {
      GstPad *pad;

      GST_DEBUG_OBJECT (src, "connecting UDP source 1 to session");

      pad = gst_element_get_static_pad (src->udpsrc[1], "src");
      gst_pad_link_full (pad, src->channelpad[1],
          GST_PAD_LINK_CHECK_NOTHING);
      gst_object_unref (pad);
    } else {
      /* leave unlinked */
    }
  }

  return TRUE;
}

static GstRTSPResult
gst_wfd_src_configure_transport (GstWFDBaseSrc *bsrc,
    GstRTSPTransport * transport)
{
  GstWFDSrc *src = GST_WFD_SRC (bsrc);
  const gchar * mime;

  g_return_val_if_fail(transport,  GST_RTSP_EINVAL);

  GST_DEBUG_OBJECT (src, "configuring transport");

  /* get the proper mime type for this manager now */
  if (gst_rtsp_transport_get_mime (transport->trans, &mime) < 0)
    goto unknown_transport;
  if (!mime)
    goto unknown_transport;

  /* configure the final mime type */
  GST_DEBUG_OBJECT (src, "setting mime to %s", mime);

  if (!gst_wfd_src_configure_manager (src))
    goto no_manager;

  switch (transport->lower_transport) {
    case GST_RTSP_LOWER_TRANS_TCP:
    case GST_RTSP_LOWER_TRANS_UDP_MCAST:
     goto transport_failed;
    case GST_RTSP_LOWER_TRANS_UDP:
      if (!gst_wfd_src_configure_udp (src))
        goto transport_failed;
      if (!gst_wfd_src_configure_udp_sinks (src, transport))
        goto transport_failed;
      break;
    default:
      goto unknown_transport;
  }

  return GST_RTSP_OK;

  /* ERRORS */
unknown_transport:
  {
    GST_DEBUG_OBJECT (src, "unknown transport");
    return GST_RTSP_ERROR;
  }
no_manager:
  {
    GST_DEBUG_OBJECT (src, "cannot configure manager");
    return GST_RTSP_ERROR;
  }
transport_failed:
  {
    GST_DEBUG_OBJECT (src, "failed to configure transport");
    return GST_RTSP_ERROR;
  }
}

static gboolean
gst_wfd_src_push_event (GstWFDBaseSrc *bsrc, GstEvent * event)
{
  GstWFDSrc *src = GST_WFD_SRC (bsrc);
  gboolean res = TRUE;

  if (src->udpsrc[0]) {
    gst_event_ref (event);
    res = gst_element_send_event (src->udpsrc[0], event);
  } else if (src->channelpad[0]) {
    gst_event_ref (event);
    if (GST_PAD_IS_SRC (src->channelpad[0]))
      res = gst_pad_push_event (src->channelpad[0], event);
    else
      res = gst_pad_send_event (src->channelpad[0], event);
  }

  if (src->udpsrc[1]) {
    gst_event_ref (event);
    res &= gst_element_send_event (src->udpsrc[1], event);
  } else if (src->channelpad[1]) {
    gst_event_ref (event);
    if (GST_PAD_IS_SRC (src->channelpad[1]))
      res &= gst_pad_push_event (src->channelpad[1], event);
    else
      res &= gst_pad_send_event (src->channelpad[1], event);
  }

  gst_event_unref (event);

  return res;
}
