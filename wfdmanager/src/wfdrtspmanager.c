/*
 * wfdrtspsrcmanager
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

#include <sys/ioctl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>

#include "wfdrtspmanager.h"

#ifdef G_OS_WIN32
#include <winsock2.h>
#endif

GST_DEBUG_CATEGORY_STATIC (wfd_rtsp_manager_debug);
#define GST_CAT_DEFAULT wfd_rtsp_manager_debug

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
  PROP_ENABLE_PAD_PROBE,
  PROP_LAST
};

#define DEFAULT_DO_RTCP          TRUE
#define DEFAULT_LATENCY_MS       2000
#define DEFAULT_UDP_BUFFER_SIZE  0x80000
#define DEFAULT_UDP_TIMEOUT          10000000

G_DEFINE_TYPE (WFDRTSPManager, wfd_rtsp_manager, G_TYPE_OBJECT);

/* GObject vmethods */
static void wfd_rtsp_manager_finalize (GObject * object);
static void wfd_rtsp_manager_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void wfd_rtsp_manager_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void wfd_rtsp_manager_enable_pad_probe(WFDRTSPManager * manager);
GstPadProbeReturn
wfd_rtsp_manager_pad_probe_cb(GstPad * pad, GstPadProbeInfo *info, gpointer u_data);

static void
wfd_rtsp_manager_class_init (WFDRTSPManagerClass * klass)
{
  GObjectClass *gobject_class;

  gobject_class = (GObjectClass *) klass;

  gobject_class->finalize = wfd_rtsp_manager_finalize;
  gobject_class->set_property = wfd_rtsp_manager_set_property;
  gobject_class->get_property = wfd_rtsp_manager_get_property;

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
      g_param_spec_uint64 ("timeout", "UDP Timeout",
          "Fail after timeout microseconds on UDP connections (0 = disabled)",
          0, G_MAXUINT64, DEFAULT_UDP_TIMEOUT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_ENABLE_PAD_PROBE,
          g_param_spec_boolean ("enable-pad-probe", "Enable Pad Probe",
          "Enable pad probe for debugging",
          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  GST_DEBUG_CATEGORY_INIT (wfd_rtsp_manager_debug, "wfdrtspmanager", 0, "WFD RTSP Manager");
}

static void
wfd_rtsp_manager_init (WFDRTSPManager * manager)
{
  GstStructure *s = NULL;
  gint i = 0;

  /* initialize variables */
  manager->do_rtcp = DEFAULT_DO_RTCP;
  manager->latency = DEFAULT_LATENCY_MS;
  manager->udp_buffer_size = DEFAULT_UDP_BUFFER_SIZE;
  manager->udp_timeout = DEFAULT_UDP_TIMEOUT;
  manager->enable_pad_probe = FALSE;

  manager->eos = FALSE;
  manager->seqbase = GST_CLOCK_TIME_NONE;
  manager->timebase = GST_CLOCK_TIME_NONE;
  manager->is_ipv6 = FALSE;
  manager->caps = gst_caps_new_simple ("application/x-rtp",
                  "media", G_TYPE_STRING, "video", "payload", G_TYPE_INT, 33, NULL);

  manager->srcpad = NULL;
  manager->blockedpad = NULL;
  manager->session = NULL;
  manager->wfdrtpbuffer = NULL;
  for (i=0; i<2; i++) {
    manager->udpsrc[i] = NULL;
    manager->udpsink[i] = NULL;
    manager->channelpad[i] = NULL;
  }

  s = gst_caps_get_structure (manager->caps, 0);
  gst_structure_set (s, "clock-rate", G_TYPE_INT, 90000, NULL);
  gst_structure_set (s, "encoding-params", G_TYPE_STRING, "MP2T-ES", NULL);

  g_rec_mutex_init (&(manager->state_rec_lock));

  manager->protocol = GST_RTSP_LOWER_TRANS_UDP;
}


static void
wfd_rtsp_manager_finalize (GObject * object)
{
  WFDRTSPManager *manager;
  gint i;

  manager = WFD_RTSP_MANAGER_CAST (object);

  if (manager->caps)
    gst_caps_unref (manager->caps);
  manager->caps = NULL;

  for (i=0; i <2; i++) {
    if (manager->udpsrc[i]) {
      gst_element_set_state (manager->udpsrc[i], GST_STATE_NULL);
      gst_bin_remove (GST_BIN_CAST (manager->wfdrtspsrc), manager->udpsrc[i]);
      gst_object_unref (manager->udpsrc[i]);
      manager->udpsrc[i] = NULL;
    }
    if (manager->channelpad[i]) {
      gst_object_unref (manager->channelpad[i]);
      manager->channelpad[i] = NULL;
    }
    if (manager->udpsink[i]) {
      gst_element_set_state (manager->udpsink[i], GST_STATE_NULL);
      gst_bin_remove (GST_BIN_CAST (manager->wfdrtspsrc), manager->udpsink[i]);
      gst_object_unref (manager->udpsink[i]);
      manager->udpsink[i] = NULL;
    }
  }
  if (manager->session) {
    gst_element_set_state (manager->session, GST_STATE_NULL);
    gst_bin_remove (GST_BIN_CAST (manager->wfdrtspsrc), manager->session);
    gst_object_unref (manager->session);
    manager->session = NULL;
  }
  if (manager->wfdrtpbuffer) {
    gst_element_set_state (manager->wfdrtpbuffer, GST_STATE_NULL);
    gst_bin_remove (GST_BIN_CAST (manager->wfdrtspsrc), manager->wfdrtpbuffer);
    gst_object_unref (manager->wfdrtpbuffer);
    manager->wfdrtpbuffer = NULL;
  }
  g_rec_mutex_clear (&(manager->state_rec_lock));

  G_OBJECT_CLASS (wfd_rtsp_manager_parent_class)->finalize (object);
}


WFDRTSPManager *
wfd_rtsp_manager_new (GstElement *wfdrtspsrc)
{
  WFDRTSPManager *manager;

  g_return_val_if_fail (wfdrtspsrc, NULL);
  g_return_val_if_fail (GST_IS_ELEMENT(wfdrtspsrc), NULL);

  manager = g_object_new (WFD_TYPE_RTSP_MANAGER, NULL);
  if (G_UNLIKELY(!manager)) {
    GST_ERROR("failed to create Wi-Fi Display manager.");
    return NULL;
  }

  manager->wfdrtspsrc = wfdrtspsrc;

  return manager;
}

static void
wfd_rtsp_manager_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  WFDRTSPManager *manager;

  manager = WFD_RTSP_MANAGER_CAST (object);

  switch (prop_id) {
    case PROP_DO_RTCP:
      manager->do_rtcp = g_value_get_boolean (value);
      break;
    case PROP_LATENCY:
      manager->latency = g_value_get_uint (value);
      g_object_set (G_OBJECT(manager->wfdrtpbuffer), "latency", manager->latency, NULL);
      break;
    case PROP_UDP_BUFFER_SIZE:
      manager->udp_buffer_size = g_value_get_int (value);
      break;
    case PROP_UDP_TIMEOUT:
      manager->udp_timeout = g_value_get_uint64 (value);
      break;
    case PROP_ENABLE_PAD_PROBE:
      manager->enable_pad_probe = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void wfd_rtsp_manager_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  WFDRTSPManager *manager;

  manager = WFD_RTSP_MANAGER_CAST (object);

  switch (prop_id) {
    case PROP_DO_RTCP:
      g_value_set_boolean (value, manager->do_rtcp);
      break;
    case PROP_LATENCY:
      g_value_set_uint (value, manager->latency);
      break;
    case PROP_UDP_BUFFER_SIZE:
      g_value_set_int (value, manager->udp_buffer_size);
      break;
    case PROP_UDP_TIMEOUT:
      g_value_set_uint64 (value, manager->udp_timeout);
      break;
    case PROP_ENABLE_PAD_PROBE:
      g_value_set_boolean (value, manager->enable_pad_probe);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
wfd_rtsp_manager_activate (WFDRTSPManager *manager)
{
  GST_DEBUG_OBJECT (manager, "activating streams");

  if (manager->udpsrc[0]) {
    /* remove timeout, we are streaming now and timeouts will be handled by
     * the session manager and jitter buffer */
    g_object_set (G_OBJECT (manager->udpsrc[0]), "timeout", (guint64) 0, NULL);
  }

  if (manager->srcpad) {
    GST_DEBUG_OBJECT (manager, "setting pad caps for manager %p", manager);
    gst_pad_set_caps (manager->srcpad, manager->caps);

    GST_DEBUG_OBJECT (manager, "activating manager pad %p", manager);
    gst_pad_set_active (manager->srcpad, TRUE);
  }

  /* unblock all pads */
  if (manager->blockedpad && manager->blockid != 0) {
    GST_DEBUG_OBJECT (manager, "unblocking manager pad %p", manager);
    gst_pad_remove_probe (manager->blockedpad, manager->blockid);
    manager->blockid = 0;
    manager->blockedpad = NULL;
  }

  return TRUE;
}


static void
pad_blocked (GstPad * pad, gboolean blocked, WFDRTSPManager *manager)
{
  GST_DEBUG_OBJECT (manager, "pad %s:%s blocked, activating streams",
      GST_DEBUG_PAD_NAME (pad));

  /* activate the streams */
  if (!manager->need_activate)
    goto was_ok;

  manager->need_activate = FALSE;

  wfd_rtsp_manager_activate (manager);

was_ok:
  return;
}

static gboolean
wfd_rtsp_manager_configure_udp (WFDRTSPManager *manager)
{
  GstPad *outpad = NULL;

  /* we manage the UDP elements now. For unicast, the UDP sources where
   * allocated in the stream when we suggested a transport. */
  if (manager->udpsrc[0]) {
    GstCaps *caps;

    gst_element_set_locked_state (manager->udpsrc[0], TRUE);
    gst_bin_add (GST_BIN_CAST (manager->wfdrtspsrc), manager->udpsrc[0]);

    GST_DEBUG_OBJECT (manager, "setting up UDP source");

    /* configure a timeout on the UDP port. When the timeout message is
     * posted */
    g_object_set (G_OBJECT (manager->udpsrc[0]), "timeout",
        manager->udp_timeout * 1000, NULL);

    caps = gst_caps_new_simple ("application/x-rtp",
                    "media", G_TYPE_STRING, "video", "payload", G_TYPE_INT, 33,
                    "clock-rate", G_TYPE_INT, 90000, "encoding-params", G_TYPE_STRING, "MP2T-ES", NULL);
    g_object_set (manager->udpsrc[0], "caps", caps, NULL);
    gst_caps_unref (caps);

    /* get output pad of the UDP source. */
    outpad = gst_element_get_static_pad (manager->udpsrc[0], "src");

    /* save it so we can unblock */
    manager->blockedpad = outpad;

    /* configure pad block on the pad. As soon as there is dataflow on the
     * UDP source, we know that UDP is not blocked by a firewall and we can
     * configure all the streams to let the application autoplug decoders. */
    manager->blockid =
        gst_pad_add_probe (manager->blockedpad,
        GST_PAD_PROBE_TYPE_BLOCK | GST_PAD_PROBE_TYPE_BUFFER |
        GST_PAD_PROBE_TYPE_BUFFER_LIST, pad_blocked, manager, NULL);

    if (manager->channelpad[0]) {
      GST_DEBUG_OBJECT (manager, "connecting UDP source 0 to session");
      /* configure for UDP delivery, we need to connect the UDP pads to
       * the session plugin. */
      gst_pad_link_full (outpad, manager->channelpad[0],
          GST_PAD_LINK_CHECK_NOTHING);
      gst_object_unref (outpad);
      outpad = NULL;
      /* we connected to pad-added signal to get pads from the manager */
    } else {
      /* leave unlinked */
    }
  }

  /* RTCP port */
  if (manager->udpsrc[1]) {
    GstCaps *caps;

    gst_element_set_locked_state (manager->udpsrc[1], TRUE);
    gst_bin_add (GST_BIN_CAST (manager->wfdrtspsrc), manager->udpsrc[1]);

    caps = gst_caps_new_empty_simple ("application/x-rtcp");
    g_object_set (manager->udpsrc[1], "caps", caps, NULL);
    gst_caps_unref (caps);

    if (manager->channelpad[1]) {
      GstPad *pad;

      GST_DEBUG_OBJECT (manager, "connecting UDP source 1 to session");

      pad = gst_element_get_static_pad (manager->udpsrc[1], "src");
      gst_pad_link_full (pad, manager->channelpad[1],
          GST_PAD_LINK_CHECK_NOTHING);
      gst_object_unref (pad);
    } else {
      /* leave unlinked */
    }
  }

  return TRUE;
}

static void
gst_wfdrtspsrc_get_transport_info (WFDRTSPManager * manager,
    GstRTSPTransport * transport, const gchar ** destination, gint * min, gint * max)
{
  g_return_if_fail (transport);
  g_return_if_fail (transport->lower_transport == GST_RTSP_LOWER_TRANS_UDP);

  if (destination) {
    /* first take the source, then the endpoint to figure out where to send
     * the RTCP. */
    if (!(*destination = transport->source)) {
      if (manager->control_connection )
        *destination = gst_rtsp_connection_get_ip (manager->control_connection );
      else if (manager->conninfo.connection)
        *destination =
            gst_rtsp_connection_get_ip (manager->conninfo.connection);
    }
  }
  if (min && max) {
    /* for unicast we only expect the ports here */
    *min = transport->server_port.min;
    *max = transport->server_port.max;
  }
}


/* configure the UDP sink back to the server for status reports */
static gboolean
wfd_rtsp_manager_configure_udp_sinks ( WFDRTSPManager * manager, GstRTSPTransport * transport)
{
  GstPad *pad = NULL;
  GSocket *socket = NULL;
  gint rtp_port = -1, rtcp_port = -1;
  gboolean do_rtcp = FALSE;
  const gchar *destination =  NULL;
  gchar *uri = NULL;

  /* get transport info */
  gst_wfdrtspsrc_get_transport_info (manager, transport, &destination,
      &rtp_port, &rtcp_port);

  /* it's possible that the server does not want us to send RTCP in which case
   * the port is -1 */
  do_rtcp = (rtcp_port != -1 && manager->session != NULL && manager->do_rtcp);

  /* we need a destination when we have RTCP port */
  if (destination == NULL && do_rtcp)
    goto no_destination;

  if (do_rtcp) {
    GstPad *rtcppad = NULL;

    GST_DEBUG_OBJECT (manager, "configure RTCP UDP sink for %s:%d", destination,
        rtcp_port);

    uri = g_strdup_printf ("udp://%s:%d", destination, rtcp_port);
    manager->udpsink[1] = gst_element_make_from_uri (GST_URI_SINK, uri, NULL, NULL);
    g_free (uri);
    if (manager->udpsink[1] == NULL)
      goto no_sink_element;

    /* don't join multicast group, we will have the source socket do that */
    /* no sync or async state changes needed */
    g_object_set (G_OBJECT (manager->udpsink[1]), "auto-multicast", FALSE,
        "loop", FALSE, "sync", FALSE, "async", FALSE, NULL);

    if (manager->udpsrc[1]) {
      /* configure socket, we give it the same UDP socket as the udpsrc for RTCP
       * because some servers check the port number of where it sends RTCP to identify
       * the RTCP packets it receives */
      g_object_get (G_OBJECT (manager->udpsrc[1]), "used-socket", &socket, NULL);
      GST_DEBUG_OBJECT (manager, "RTCP UDP src has sock %p", socket);
      /* configure socket and make sure udpsink does not close it when shutting
       * down, it belongs to udpsrc after all. */
      g_object_set (G_OBJECT (manager->udpsink[1]), "socket", socket,
          "close-socket", FALSE, NULL);
      g_object_unref (socket);
    }

    /* we don't want to consider this a sink */
    GST_OBJECT_FLAG_UNSET (manager->udpsink[1], GST_ELEMENT_FLAG_SINK);

    /* we keep this playing always */
    gst_element_set_locked_state (manager->udpsink[1], TRUE);
    gst_element_set_state (manager->udpsink[1], GST_STATE_PLAYING);

    gst_object_ref (manager->udpsink[1]);
    gst_bin_add (GST_BIN_CAST (manager->wfdrtspsrc), manager->udpsink[1]);

    rtcppad = gst_element_get_static_pad (manager->udpsink[1], "sink");

    /* get session RTCP pad */
    pad = gst_element_get_request_pad (manager->session, "send_rtcp_src");

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
    GST_DEBUG_OBJECT (manager, "no destination address specified");
    return FALSE;
  }
no_sink_element:
  {
    GST_DEBUG_OBJECT (manager, "no UDP sink element found");
    return FALSE;
  }
}

static void
on_bye_ssrc (GObject * session, GObject * source, WFDRTSPManager * manager)
{
  GST_DEBUG_OBJECT (manager, "source in session received BYE");

  //gst_wfdrtspsrc_do_stream_eos (src, manager);
}

static void
on_new_ssrc (GObject * session, GObject * source, WFDRTSPManager * manager)
{
  GST_DEBUG_OBJECT (manager, "source in session received NEW");
}

static void
on_timeout (GObject * session, GObject * source, WFDRTSPManager * manager)
{
  GST_DEBUG_OBJECT (manager, "source in session timed out");

  //gst_wfdrtspsrc_do_stream_eos (src, manager);
}

static void
on_ssrc_active (GObject * session, GObject * source, WFDRTSPManager * manager)
{
  GST_DEBUG_OBJECT (manager, "source in session  is active");
}


static GstCaps *
request_pt_map_for_wfdrtpbuffer (GstElement * wfdrtpbuffer, guint pt, WFDRTSPManager * manager)
{
  GstCaps *caps;

  if (!manager)
    goto unknown_stream;

  GST_DEBUG_OBJECT (manager, "getting pt map for pt %d", pt);

  WFD_RTSP_MANAGER_STATE_LOCK (manager);

  caps = manager->caps;
  if (caps)
    gst_caps_ref (caps);
  WFD_RTSP_MANAGER_STATE_UNLOCK (manager);

  return caps;

unknown_stream:
  {
    GST_ERROR ( " manager is NULL");
    return NULL;
  }
}

static GstCaps *
request_pt_map_for_session (GstElement * session, guint session_id, guint pt, WFDRTSPManager * manager)
{
  GstCaps *caps;

  GST_DEBUG_OBJECT (manager, "getting pt map for pt %d in session %d", pt, session_id);

  WFD_RTSP_MANAGER_STATE_LOCK (manager);
  caps = manager->caps;
  if (caps)
    gst_caps_ref (caps);
  WFD_RTSP_MANAGER_STATE_UNLOCK (manager);

  return caps;
}

gboolean
wfd_rtsp_manager_prepare_transport (WFDRTSPManager * manager,
    gint rtpport, gint rtcpport)
{
  GstStateChangeReturn ret;
  GstElement *udpsrc0, *udpsrc1;
  gint tmp_rtp, tmp_rtcp;
  const gchar *host;

  udpsrc0 = NULL;
  udpsrc1 = NULL;

  if (manager->is_ipv6)
    host = "udp://[::0]";
  else
    host = "udp://0.0.0.0";

  /* try to allocate 2 UDP ports */
  udpsrc0 = gst_element_make_from_uri (GST_URI_SRC, host, NULL, NULL);
  if (udpsrc0 == NULL)
    goto no_udp_protocol;
  g_object_set (G_OBJECT (udpsrc0), "port", rtpport, "reuse", FALSE, NULL);

  if (manager->udp_buffer_size != 0)
    g_object_set (G_OBJECT (udpsrc0), "buffer-size", manager->udp_buffer_size,
        NULL);

  ret = gst_element_set_state (udpsrc0, GST_STATE_READY);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    GST_ERROR_OBJECT (manager, "Unable to make udpsrc from RTP port %d", tmp_rtp);
    /* port not even, free RTP udpsrc */
    goto no_ports;
  }

  g_object_get (G_OBJECT (udpsrc0), "port", &tmp_rtp, NULL);
  GST_DEBUG_OBJECT (manager, "got RTP port %d", tmp_rtp);

  /* check if port is even */
  if ((tmp_rtp & 0x01) != 0) {
    GST_DEBUG_OBJECT (manager, "RTP port not even");
    /* port not even, free RTP udpsrc */
    goto no_ports;
  }

  /* allocate port+1 for RTCP now */
  udpsrc1 = gst_element_make_from_uri (GST_URI_SRC, host, NULL, NULL);
  if (udpsrc1 == NULL)
    goto no_udp_rtcp_protocol;

  /* set port */
  g_object_set (G_OBJECT (udpsrc1), "port", rtcpport, "reuse", FALSE, NULL);

  GST_DEBUG_OBJECT (manager, "starting RTCP on port %d", rtcpport);
  ret = gst_element_set_state (udpsrc1, GST_STATE_READY);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    GST_ERROR_OBJECT (manager, "Unable to make udpsrc from RTCP port %d", tmp_rtcp);
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
  manager->udpsrc[0] = gst_object_ref_sink (udpsrc0);
  manager->udpsrc[1] = gst_object_ref_sink (udpsrc1);
  gst_element_set_locked_state (manager->udpsrc[0], TRUE);
  gst_element_set_locked_state (manager->udpsrc[1], TRUE);

  return TRUE;

  /* ERRORS */
no_udp_protocol:
  {
    GST_DEBUG_OBJECT (manager, "could not get UDP source");
    goto cleanup;
  }
no_ports:
  {
    GST_DEBUG_OBJECT (manager, "could not allocate UDP port pair");
    goto cleanup;
  }
no_udp_rtcp_protocol:
  {
    GST_DEBUG_OBJECT (manager, "could not get UDP source for RTCP");
    goto cleanup;
  }
port_error:
  {
    GST_DEBUG_OBJECT (manager, "ports don't match rtp: %d<->%d, rtcp: %d<->%d",
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
    return FALSE;
  }
}

static gboolean
wfd_rtsp_manager_configure_manager (WFDRTSPManager * manager)
{
  GstPad *pad = NULL;

  g_return_val_if_fail (manager, FALSE);

  /* construct wfdrtspsrc */
  manager->session = gst_element_factory_make ("rtpsession", "wfdrtspsrc_session");
  if (G_UNLIKELY (manager->session == NULL)) {
    GST_ERROR_OBJECT (manager, "could not create gstrtpsession element");
    return FALSE;
  }  else {
    g_signal_connect (manager->session, "on-bye-ssrc", (GCallback) on_bye_ssrc,
        manager);
    g_signal_connect (manager->session, "on-bye-timeout", (GCallback) on_timeout,
        manager);
    g_signal_connect (manager->session, "on-timeout", (GCallback) on_timeout,
        manager);
    g_signal_connect (manager->session, "on-ssrc-active",
        (GCallback) on_ssrc_active, manager);
    g_signal_connect (manager->session, "on-new-ssrc", (GCallback) on_new_ssrc,
        manager);
    g_signal_connect (manager->session, "request-pt-map",
        (GCallback) request_pt_map_for_session, manager);

    g_object_set (G_OBJECT(manager->session), "rtcp-min-interval", (guint64)1000000000, NULL);

    manager->channelpad[0] = gst_element_get_request_pad (manager->session, "recv_rtp_sink");
    if (G_UNLIKELY (manager->channelpad[0]  == NULL)) {
      GST_ERROR_OBJECT (manager, "could not create rtp channel pad");
      return FALSE;
    }

    manager->channelpad[1] = gst_element_get_request_pad (manager->session, "recv_rtcp_sink");
    if (G_UNLIKELY (manager->channelpad[1]  == NULL)) {
      GST_ERROR_OBJECT (manager, "could not create rtcp channel pad");
      return FALSE;
    }

    /* we manage sesion element */
    gst_element_set_locked_state (manager->session, TRUE);

    if (!gst_bin_add(GST_BIN_CAST(manager->wfdrtspsrc), manager->session)) {
      GST_ERROR_OBJECT (manager, "failed to add rtpsession to wfdrtspsrc");
      return FALSE;
    }
  }

  manager->wfdrtpbuffer = gst_element_factory_make ("wfdrtpbuffer", "wfdrtspsrc_wfdrtpbuffer");
  if (G_UNLIKELY (manager->wfdrtpbuffer == NULL)) {
    GST_ERROR_OBJECT (manager, "could not create wfdrtpbuffer element");
    return FALSE;
  } else {
    /* configure latency and packet lost */
    g_object_set (manager->wfdrtpbuffer, "latency", manager->latency, NULL);

    g_signal_connect (manager->wfdrtpbuffer, "request-pt-map",
	 (GCallback) request_pt_map_for_wfdrtpbuffer, manager);

    /* we manage wfdrtpbuffer element */
    gst_element_set_locked_state (manager->wfdrtpbuffer, TRUE);

    if (!gst_bin_add(GST_BIN_CAST(manager->wfdrtspsrc), manager->wfdrtpbuffer)) {
      GST_ERROR_OBJECT (manager, "failed to add wfdrtpbuffer to wfdrtspsrc");
      return FALSE;
    }
  }

  if (!gst_element_link_many(manager->session, manager->wfdrtpbuffer, NULL)) {
    GST_ERROR_OBJECT (manager, "failed to link elements for wfdrtspsrc");
    return FALSE;
  }

  if (!gst_element_sync_state_with_parent(manager->session)) {
    GST_ERROR_OBJECT (manager, "failed for %s to sync state with wfdrtspsrc", GST_ELEMENT_NAME(manager->session));
    return FALSE;
  }

  if (!gst_element_sync_state_with_parent(manager->wfdrtpbuffer)) {
    GST_ERROR_OBJECT (manager, "failed for %s to sync state with wfdrtspsrc", GST_ELEMENT_NAME(manager->wfdrtpbuffer));
    return FALSE;
  }

  /* set ghost pad */
  pad = gst_element_get_static_pad (manager->wfdrtpbuffer, "src");
  if (G_UNLIKELY (pad == NULL)) {
    GST_ERROR_OBJECT (manager, "failed to get src pad of wfdrtpbuffer for setting ghost pad of wfdrtspsrc");
    return FALSE;
  }
  if (!gst_ghost_pad_set_target (GST_GHOST_PAD (manager->srcpad), pad)) {
    GST_ERROR_OBJECT (manager, "failed to set target pad of wfdrtpbuffer ghost pad");
    gst_object_unref (pad);
    return FALSE;
  }
  gst_object_unref (pad);

  if(manager->enable_pad_probe)
    wfd_rtsp_manager_enable_pad_probe(manager);

  return TRUE;
}


gboolean
wfd_rtsp_manager_configure_transport (WFDRTSPManager * manager,
    GstRTSPTransport * transport)
{
  const gchar * mime;

  g_return_val_if_fail(transport,  FALSE);

  GST_DEBUG_OBJECT (manager, "configuring transport");

  /* get the proper mime type for this manager now */
  if (gst_rtsp_transport_get_mime (transport->trans, &mime) < 0)
    goto unknown_transport;
  if (!mime)
    goto unknown_transport;

  /* configure the final mime type */
  GST_DEBUG_OBJECT (manager, "setting mime to %s", mime);

  if (!wfd_rtsp_manager_configure_manager (manager))
    goto no_manager;

  switch (transport->lower_transport) {
    case GST_RTSP_LOWER_TRANS_TCP:
    case GST_RTSP_LOWER_TRANS_UDP_MCAST:
     goto transport_failed;
    case GST_RTSP_LOWER_TRANS_UDP:
      if (!wfd_rtsp_manager_configure_udp (manager))
        goto transport_failed;
      if (!wfd_rtsp_manager_configure_udp_sinks (manager, transport))
        goto transport_failed;
      break;
    default:
      goto unknown_transport;
  }

  manager->need_activate = TRUE;
  manager->protocol = transport->lower_transport;

  return TRUE;

  /* ERRORS */
transport_failed:
  {
    GST_DEBUG_OBJECT (manager, "failed to configure transport");
    return FALSE;
  }
unknown_transport:
  {
    GST_DEBUG_OBJECT (manager, "unknown transport");
    return FALSE;
  }
no_manager:
  {
    GST_DEBUG_OBJECT (manager, "cannot get a session manager");
    return FALSE;
  }
}

void
wfd_rtsp_manager_set_state (WFDRTSPManager * manager, GstState state)
{
  gint i=0;

  g_return_if_fail(manager);

  if (manager->session)
    gst_element_set_state (manager->session, state);

  if (manager->wfdrtpbuffer)
    gst_element_set_state (manager->wfdrtpbuffer, state);

  for (i = 0; i < 2; i++) {
    if (manager->udpsrc[i])
      gst_element_set_state (manager->udpsrc[i], state);
  }
}

GstPadProbeReturn
wfd_rtsp_manager_pad_probe_cb(GstPad * pad, GstPadProbeInfo *info, gpointer u_data)
{
  GstElement* parent = NULL;
  const GstSegment *segment = NULL;

  g_return_val_if_fail (pad, GST_PAD_PROBE_DROP);
  g_return_val_if_fail (info, GST_PAD_PROBE_DROP);

  parent = (GstElement*)gst_object_get_parent(GST_OBJECT(pad));
  if(!parent)
    return GST_PAD_PROBE_DROP;

  if (info->type & GST_PAD_PROBE_TYPE_BUFFER) {
    GstBuffer *buffer = gst_pad_probe_info_get_buffer (info);

    /* show name and timestamp */
    GST_DEBUG_OBJECT(WFD_RTSP_MANAGER (u_data), "BUFFER PROBE : %s:%s :  %u:%02u:%02u.%09u  (%d bytes)",
        GST_STR_NULL(GST_ELEMENT_NAME(parent)), GST_STR_NULL(GST_PAD_NAME(pad)),
        GST_TIME_ARGS(GST_BUFFER_TIMESTAMP(buffer)), gst_buffer_get_size(buffer));
  } else if (info->type & GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM ||
                info->type & GST_PAD_PROBE_TYPE_EVENT_UPSTREAM ||
                info->type & GST_PAD_PROBE_TYPE_EVENT_FLUSH ||
                info->type & GST_PAD_PROBE_TYPE_EVENT_BOTH) {
      GstEvent *event = gst_pad_probe_info_get_event (info);
    /* show name and event type */
    GST_DEBUG_OBJECT(WFD_RTSP_MANAGER (u_data), "EVENT PROBE : %s:%s :  %s",
      GST_STR_NULL(GST_ELEMENT_NAME(parent)), GST_STR_NULL(GST_PAD_NAME(pad)),
      GST_EVENT_TYPE_NAME(event));

    if (GST_EVENT_TYPE (event) == GST_EVENT_SEGMENT) {
      gst_event_parse_segment (event, &segment);
      if (segment)
        GST_DEBUG_OBJECT (WFD_RTSP_MANAGER (u_data), "NEWSEGMENT : %"
            G_GINT64_FORMAT " -- %" G_GINT64_FORMAT ", time %" G_GINT64_FORMAT,
             segment->start, segment->stop, segment->time);
    }
  }

  if ( parent )
    gst_object_unref(parent);

  return GST_PAD_PROBE_OK;
}

static void wfd_rtsp_manager_enable_pad_probe(WFDRTSPManager * manager)
{
  GstPad * pad = NULL;

  g_return_if_fail(manager);

  if(manager->udpsrc[0]) {
    pad = NULL;
    pad = gst_element_get_static_pad(manager->udpsrc[0], "src");
    if (pad) {
      if( gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_DATA_DOWNSTREAM, wfd_rtsp_manager_pad_probe_cb, (gpointer)manager, NULL)) {
        GST_DEBUG_OBJECT (manager, "added pad probe (pad : %s, element : %s)", gst_pad_get_name(pad), gst_element_get_name(manager->udpsrc[0]));
      }
      gst_object_unref (pad);
      pad = NULL;
    }
  }

  if(manager->session) {
    pad = gst_element_get_static_pad(manager->session, "recv_rtp_sink");
    if (pad) {
      if( gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_DATA_DOWNSTREAM, wfd_rtsp_manager_pad_probe_cb, (gpointer)manager, NULL)) {
        GST_DEBUG_OBJECT (manager, "added pad probe (pad : %s, element : %s)", gst_pad_get_name(pad), gst_element_get_name(manager->session));
      }
      gst_object_unref (pad);
      pad = NULL;
    }
    pad = gst_element_get_static_pad(manager->session, "recv_rtp_src");
    if (pad) {
      if( gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_DATA_DOWNSTREAM, wfd_rtsp_manager_pad_probe_cb, (gpointer)manager, NULL)) {
        GST_DEBUG_OBJECT (manager, "added pad probe (pad : %s, element : %s)", gst_pad_get_name(pad), gst_element_get_name(manager->session));
      }
      gst_object_unref (pad);
      pad = NULL;
    }
  }

  if(manager->wfdrtpbuffer) {
    pad = gst_element_get_static_pad(manager->wfdrtpbuffer, "sink");
    if (pad) {
      if( gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_DATA_DOWNSTREAM, wfd_rtsp_manager_pad_probe_cb, (gpointer)manager, NULL)) {
        GST_DEBUG_OBJECT (manager, "added pad probe (pad : %s, element : %s)", gst_pad_get_name(pad), gst_element_get_name(manager->wfdrtpbuffer));
      }
      gst_object_unref (pad);
      pad = NULL;
    }

    pad = gst_element_get_static_pad(manager->wfdrtpbuffer, "src");
    if (pad) {
      if( gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_DATA_DOWNSTREAM, wfd_rtsp_manager_pad_probe_cb, (gpointer)manager, NULL)) {
        GST_DEBUG_OBJECT (manager, "added pad probe (pad : %s, element : %s)", gst_pad_get_name(pad), gst_element_get_name(manager->wfdrtpbuffer));
      }
      gst_object_unref (pad);
      pad = NULL;
    }
  }

}
