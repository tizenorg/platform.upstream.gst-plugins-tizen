/*
 * wfdrtsp message
 *
 * Copyright (c) 2011 - 2013 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Taewan Kim<taewan79.kim@samsung.com>, Yejin Cho<cho.yejin@samsung.com>, Sangkyu Park<sk1122.park@samsung.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library (COPYING); if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#include <glib.h>               /* for G_OS_WIN32 */
#include "gstwfdsinkmessage.h"
#include <gio/gio.h>

/* FIXME, is currently allocated on the stack */
#define MAX_LINE_LEN    (1024 * 16)

#define FREE_STRING(field)              if (field != NULL) g_free(field); (field) = NULL;
#define REPLACE_STRING(field, val)      FREE_STRING(field); (field) = g_strdup(val);
#define EDID_BLOCK_SIZE 128
#define EDID_BLOCK_COUNT_MAX_SIZE 256
#define MAX_PORT_SIZE 65535

enum {
  GST_WFD_SESSION,
  GST_WFD_MEDIA,
};

typedef struct {
  guint state;
  GstWFDMessage *msg;
} WFDContext;

/**
* gst_wfd_message_new:
* @msg: pointer to new #GstWFDMessage
*
* Allocate a new GstWFDMessage and store the result in @msg.
*
* Returns: a #GstWFDResult.
*/
GstWFDResult
gst_wfd_message_new(GstWFDMessage **msg)
{
  GstWFDMessage *newmsg;

  g_return_val_if_fail(msg != NULL, GST_WFD_EINVAL);

  newmsg = g_new0(GstWFDMessage, 1);

  *msg = newmsg;

  return gst_wfd_message_init(newmsg);
}

/**
* gst_wfd_message_init:
* @msg: a #GstWFDMessage
*
* Initialize @msg so that its contents are as if it was freshly allocated
* with gst_wfd_message_new(). This function is mostly used to initialize a message
* allocated on the stack. gst_wfd_message_uninit() undoes this operation.
*
* When this function is invoked on newly allocated data(with malloc or on the
* stack), its contents should be set to 0 before calling this function.
*
* Returns: a #GstWFDResult.
*/
GstWFDResult
gst_wfd_message_init(GstWFDMessage *msg)
{
  g_return_val_if_fail(msg != NULL, GST_WFD_EINVAL);

  return GST_WFD_OK;
}

/**
* gst_wfd_message_uninit:
* @msg: a #GstWFDMessage
*
* Free all resources allocated in @msg. @msg should not be used anymore after
* this function. This function should be used when @msg was allocated on the
* stack and initialized with gst_wfd_message_init().
*
* Returns: a #GstWFDResult.
*/
GstWFDResult
gst_wfd_message_uninit(GstWFDMessage *msg)
{
  g_return_val_if_fail(msg != NULL, GST_WFD_EINVAL);

  if (msg->audio_codecs) {
    guint i = 0;
    if (msg->audio_codecs->list) {
      for (; i < msg->audio_codecs->count; i++) {
        FREE_STRING(msg->audio_codecs->list[i].audio_format);
        msg->audio_codecs->list[i].modes = 0;
        msg->audio_codecs->list[i].latency = 0;
      }
      FREE_STRING(msg->audio_codecs->list);
    }
    FREE_STRING(msg->audio_codecs);
  }

  if (msg->video_formats) {
    FREE_STRING(msg->video_formats->list);
    FREE_STRING(msg->video_formats);
  }

  if (msg->video_3d_formats) {
    FREE_STRING(msg->video_3d_formats->list);
    FREE_STRING(msg->video_3d_formats);
  }

  if (msg->content_protection) {
    if (msg->content_protection->hdcp2_spec) {
      FREE_STRING(msg->content_protection->hdcp2_spec->hdcpversion);
      FREE_STRING(msg->content_protection->hdcp2_spec->TCPPort);
      FREE_STRING(msg->content_protection->hdcp2_spec);
    }
    FREE_STRING(msg->content_protection);
  }

  if (msg->display_edid) {
    if (msg->display_edid->edid_payload)
      FREE_STRING(msg->display_edid->edid_payload);
    FREE_STRING(msg->display_edid);
  }

  if (msg->coupled_sink) {
    if (msg->coupled_sink->coupled_sink_cap) {
      FREE_STRING(msg->coupled_sink->coupled_sink_cap->sink_address);
      FREE_STRING(msg->coupled_sink->coupled_sink_cap);
    }
    FREE_STRING(msg->coupled_sink);
  }

  if (msg->trigger_method) {
    FREE_STRING(msg->trigger_method->wfd_trigger_method);
    FREE_STRING(msg->trigger_method);
  }

  if (msg->presentation_url) {
    FREE_STRING(msg->presentation_url->wfd_url0);
    FREE_STRING(msg->presentation_url->wfd_url1);
    FREE_STRING(msg->presentation_url);
  }

  if (msg->client_rtp_ports) {
    FREE_STRING(msg->client_rtp_ports->profile);
    FREE_STRING(msg->client_rtp_ports->mode);
    FREE_STRING(msg->client_rtp_ports);
  }

  if (msg->route) {
    FREE_STRING(msg->route->destination);
    FREE_STRING(msg->route);
  }

  if (msg->I2C) {
    FREE_STRING(msg->I2C);
  }

  if (msg->av_format_change_timing) {
    FREE_STRING(msg->av_format_change_timing);
  }

  if (msg->preferred_display_mode) {
    FREE_STRING(msg->preferred_display_mode);
  }

  if (msg->standby_resume_capability) {
    FREE_STRING(msg->standby_resume_capability);
  }

  if (msg->standby) {
    FREE_STRING(msg->standby);
  }

  if (msg->connector_type) {
    FREE_STRING(msg->connector_type);
  }

  if (msg->idr_request) {
    FREE_STRING(msg->idr_request);
  }

  return GST_WFD_OK;
}

/**
* gst_wfd_message_free:
* @msg: a #GstWFDMessage
*
* Free all resources allocated by @msg. @msg should not be used anymore after
* this function. This function should be used when @msg was dynamically
* allocated with gst_wfd_message_new().
*
* Returns: a #GstWFDResult.
*/
GstWFDResult
gst_wfd_message_free(GstWFDMessage *msg)
{
  g_return_val_if_fail(msg != NULL, GST_WFD_EINVAL);

  gst_wfd_message_uninit(msg);
  g_free(msg);

  return GST_WFD_OK;
}

/**
* gst_wfd_message_as_text:
* @msg: a #GstWFDMessage
*
* Convert the contents of @msg to a text string.
*
* Returns: A dynamically allocated string representing the WFD description.
*/
gchar *
gst_wfd_message_as_text(const GstWFDMessage *msg)
{
  /* change all vars so they match rfc? */
  GString *lines;
  g_return_val_if_fail(msg != NULL, NULL);

  lines = g_string_new("");

  /* list of audio codecs */
  if (msg->audio_codecs) {
    guint i = 0;
    g_string_append_printf(lines, GST_STRING_WFD_AUDIO_CODECS);
    if (msg->audio_codecs->list) {
      g_string_append_printf(lines, GST_STRING_WFD_COLON);
      for (; i < msg->audio_codecs->count; i++) {
        g_string_append_printf(lines, " %s", msg->audio_codecs->list[i].audio_format);
        g_string_append_printf(lines, " %08x", msg->audio_codecs->list[i].modes);
        g_string_append_printf(lines, " %02x", msg->audio_codecs->list[i].latency);
        if ((i + 1) < msg->audio_codecs->count)
          g_string_append_printf(lines, GST_STRING_WFD_COMMA);
      }
    }
    g_string_append_printf(lines, GST_STRING_WFD_CRLF);
  }

  /* list of video codecs */
  if (msg->video_formats) {
    g_string_append_printf(lines, GST_STRING_WFD_VIDEO_FORMATS);
    if (msg->video_formats->list) {
      g_string_append_printf(lines, GST_STRING_WFD_COLON);
      g_string_append_printf(lines, " %02x", msg->video_formats->list->native);
      g_string_append_printf(lines, " %02x", msg->video_formats->list->preferred_display_mode_supported);
      g_string_append_printf(lines, " %02x", msg->video_formats->list->H264_codec.profile);
      g_string_append_printf(lines, " %02x", msg->video_formats->list->H264_codec.level);
      g_string_append_printf(lines, " %08x", msg->video_formats->list->H264_codec.misc_params.CEA_Support);
      g_string_append_printf(lines, " %08x", msg->video_formats->list->H264_codec.misc_params.VESA_Support);
      g_string_append_printf(lines, " %08x", msg->video_formats->list->H264_codec.misc_params.HH_Support);
      g_string_append_printf(lines, " %02x", msg->video_formats->list->H264_codec.misc_params.latency);
      g_string_append_printf(lines, " %04x", msg->video_formats->list->H264_codec.misc_params.min_slice_size);
      g_string_append_printf(lines, " %04x", msg->video_formats->list->H264_codec.misc_params.slice_enc_params);
      g_string_append_printf(lines, " %02x", msg->video_formats->list->H264_codec.misc_params.frame_rate_control_support);

      if (msg->video_formats->list->preferred_display_mode_supported == GST_WFD_PREFERRED_DISPLAY_MODE_SUPPORTED
        && msg->video_formats->list->H264_codec.max_hres) {
        g_string_append_printf(lines, " %04x", msg->video_formats->list->H264_codec.max_hres);
      } else {
        g_string_append_printf(lines, GST_STRING_WFD_SPACE);
        g_string_append_printf(lines, GST_STRING_WFD_NONE);
      }
      if (msg->video_formats->list->preferred_display_mode_supported == GST_WFD_PREFERRED_DISPLAY_MODE_SUPPORTED
        && msg->video_formats->list->H264_codec.max_vres) {
        g_string_append_printf(lines, " %04x", msg->video_formats->list->H264_codec.max_vres);
      } else {
        g_string_append_printf(lines, GST_STRING_WFD_SPACE);
        g_string_append_printf(lines, GST_STRING_WFD_NONE);
      }
    }
    g_string_append_printf(lines, GST_STRING_WFD_CRLF);
  }

  /* list of video 3D codecs */
  if (msg->video_3d_formats) {
    g_string_append_printf(lines, GST_STRING_WFD_3D_VIDEO_FORMATS);
    g_string_append_printf(lines, GST_STRING_WFD_COLON);
    if (msg->video_3d_formats->list) {
      g_string_append_printf(lines, " %02x", msg->video_3d_formats->list->native);
      g_string_append_printf(lines, " %02x", msg->video_3d_formats->list->preferred_display_mode_supported);
      g_string_append_printf(lines, " %02x", msg->video_3d_formats->list->H264_codec.profile);
      g_string_append_printf(lines, " %02x", msg->video_3d_formats->list->H264_codec.level);
      g_string_append_printf(lines, " %016llx", msg->video_3d_formats->list->H264_codec.misc_params.video_3d_capability);
      g_string_append_printf(lines, " %02x", msg->video_3d_formats->list->H264_codec.misc_params.latency);
      g_string_append_printf(lines, " %04x", msg->video_3d_formats->list->H264_codec.misc_params.min_slice_size);
      g_string_append_printf(lines, " %04x", msg->video_3d_formats->list->H264_codec.misc_params.slice_enc_params);
      g_string_append_printf(lines, " %02x", msg->video_3d_formats->list->H264_codec.misc_params.frame_rate_control_support);
      if (msg->video_3d_formats->list->preferred_display_mode_supported == GST_WFD_PREFERRED_DISPLAY_MODE_SUPPORTED
        && msg->video_3d_formats->list->H264_codec.max_hres) {
        g_string_append_printf(lines, " %04x", msg->video_3d_formats->list->H264_codec.max_hres);
      } else {
        g_string_append_printf(lines, GST_STRING_WFD_SPACE);
        g_string_append_printf(lines, GST_STRING_WFD_NONE);
      }

      if (msg->video_3d_formats->list->preferred_display_mode_supported == GST_WFD_PREFERRED_DISPLAY_MODE_SUPPORTED
        && msg->video_3d_formats->list->H264_codec.max_vres) {
        g_string_append_printf(lines, " %04x", msg->video_3d_formats->list->H264_codec.max_vres);
      } else {
        g_string_append_printf(lines, GST_STRING_WFD_SPACE);
        g_string_append_printf(lines, GST_STRING_WFD_NONE);
      }
    } else {
      g_string_append_printf(lines, GST_STRING_WFD_SPACE);
      g_string_append_printf(lines, GST_STRING_WFD_NONE);
    }
    g_string_append_printf(lines, GST_STRING_WFD_CRLF);
  }

  if (msg->content_protection) {
    g_string_append_printf(lines, GST_STRING_WFD_CONTENT_PROTECTION);
    g_string_append_printf(lines, GST_STRING_WFD_COLON);
    if (msg->content_protection->hdcp2_spec) {
      if (msg->content_protection->hdcp2_spec->hdcpversion) {
        g_string_append_printf(lines, " %s", msg->content_protection->hdcp2_spec->hdcpversion);
        g_string_append_printf(lines, " %s", msg->content_protection->hdcp2_spec->TCPPort);
      } else {
        g_string_append_printf(lines, GST_STRING_WFD_SPACE);
        g_string_append_printf(lines, GST_STRING_WFD_NONE);
      }
    } else {
      g_string_append_printf(lines, GST_STRING_WFD_SPACE);
      g_string_append_printf(lines, GST_STRING_WFD_NONE);
    }
    g_string_append_printf(lines, GST_STRING_WFD_CRLF);
  }

  if (msg->display_edid) {
    g_string_append_printf(lines, GST_STRING_WFD_DISPLAY_EDID);
    g_string_append_printf(lines, GST_STRING_WFD_COLON);
    if (msg->display_edid->edid_supported) {
      if (msg->display_edid->edid_block_count > 0 &&
        msg->display_edid->edid_block_count <= EDID_BLOCK_COUNT_MAX_SIZE) {
        g_string_append_printf(lines, GST_STRING_WFD_SPACE "%04x", msg->display_edid->edid_block_count);
        g_string_append_printf(lines, GST_STRING_WFD_SPACE "%s", msg->display_edid->edid_payload);

      } else {
        g_string_append_printf(lines, GST_STRING_WFD_SPACE);
        g_string_append_printf(lines, GST_STRING_WFD_NONE);
      }
    } else {
      g_string_append_printf(lines, GST_STRING_WFD_SPACE);
      g_string_append_printf(lines, GST_STRING_WFD_NONE);
    }
    g_string_append_printf(lines, GST_STRING_WFD_CRLF);
  }

  if (msg->coupled_sink) {
    g_string_append_printf(lines, GST_STRING_WFD_COUPLED_SINK);
    g_string_append_printf(lines, GST_STRING_WFD_COLON);
    if (msg->coupled_sink->coupled_sink_cap) {
      g_string_append_printf(lines, " %02x", msg->coupled_sink->coupled_sink_cap->status);
      if (msg->coupled_sink->coupled_sink_cap->sink_address) {
        g_string_append_printf(lines, " %s", msg->coupled_sink->coupled_sink_cap->sink_address);
      } else {
        g_string_append_printf(lines, GST_STRING_WFD_SPACE);
        g_string_append_printf(lines, GST_STRING_WFD_NONE);
      }
    } else {
      g_string_append_printf(lines, GST_STRING_WFD_SPACE);
      g_string_append_printf(lines, GST_STRING_WFD_NONE);
    }
    g_string_append_printf(lines, GST_STRING_WFD_CRLF);
  }

  if (msg->trigger_method) {
    g_string_append_printf(lines, GST_STRING_WFD_TRIGGER_METHOD);
    g_string_append_printf(lines, GST_STRING_WFD_COLON);
    g_string_append_printf(lines, " %s", msg->trigger_method->wfd_trigger_method);
    g_string_append_printf(lines, GST_STRING_WFD_CRLF);
  }

  if (msg->presentation_url) {
    g_string_append_printf(lines, GST_STRING_WFD_PRESENTATION_URL);
    g_string_append_printf(lines, GST_STRING_WFD_COLON);
    if (msg->presentation_url->wfd_url0) {
      g_string_append_printf(lines, " %s", msg->presentation_url->wfd_url0);
    } else {
      g_string_append_printf(lines, GST_STRING_WFD_SPACE);
      g_string_append_printf(lines, GST_STRING_WFD_NONE);
    }
    if (msg->presentation_url->wfd_url1) {
      g_string_append_printf(lines, " %s", msg->presentation_url->wfd_url1);
    } else {
      g_string_append_printf(lines, GST_STRING_WFD_SPACE);
      g_string_append_printf(lines, GST_STRING_WFD_NONE);
    }
    g_string_append_printf(lines, GST_STRING_WFD_CRLF);
  }

  if (msg->client_rtp_ports) {
    g_string_append_printf(lines, GST_STRING_WFD_CLIENT_RTP_PORTS);
    if (msg->client_rtp_ports->profile) {
      g_string_append_printf(lines, GST_STRING_WFD_COLON);
      g_string_append_printf(lines, " %s", msg->client_rtp_ports->profile);
      g_string_append_printf(lines, " %d", msg->client_rtp_ports->rtp_port0);
      g_string_append_printf(lines, " %d", msg->client_rtp_ports->rtp_port1);
      g_string_append_printf(lines, " %s", msg->client_rtp_ports->mode);
    }
    g_string_append_printf(lines, GST_STRING_WFD_CRLF);
  }

  if (msg->route) {
    g_string_append_printf(lines, GST_STRING_WFD_ROUTE);
    g_string_append_printf(lines, GST_STRING_WFD_COLON);
    g_string_append_printf(lines, " %s", msg->route->destination);
    g_string_append_printf(lines, GST_STRING_WFD_CRLF);
  }

  if (msg->I2C) {
    g_string_append_printf(lines, GST_STRING_WFD_I2C);
    g_string_append_printf(lines, GST_STRING_WFD_COLON);
    g_string_append_printf(lines, GST_STRING_WFD_SPACE);
    if (msg->I2C->I2CPresent) {
      g_string_append_printf(lines, "%x", msg->I2C->I2C_port);
    } else {
      g_string_append_printf(lines, GST_STRING_WFD_NONE);
    }
    g_string_append_printf(lines, GST_STRING_WFD_CRLF);
  }

  if (msg->av_format_change_timing) {
    g_string_append_printf(lines, GST_STRING_WFD_AV_FORMAT_CHANGE_TIMING);
    g_string_append_printf(lines, GST_STRING_WFD_COLON);
    g_string_append_printf(lines, " %010llx", msg->av_format_change_timing->PTS);
    g_string_append_printf(lines, " %010llx", msg->av_format_change_timing->DTS);
    g_string_append_printf(lines, GST_STRING_WFD_CRLF);
  }

  if (msg->preferred_display_mode) {
    g_string_append_printf(lines, GST_STRING_WFD_PREFERRED_DISPLAY_MODE);
    g_string_append_printf(lines, GST_STRING_WFD_COLON);
    if (msg->preferred_display_mode->displaymodesupported) {
      g_string_append_printf(lines, " %06llx", msg->preferred_display_mode->p_clock);
      g_string_append_printf(lines, " %04x", msg->preferred_display_mode->H);
      g_string_append_printf(lines, " %04x", msg->preferred_display_mode->HB);
      g_string_append_printf(lines, " %04x", msg->preferred_display_mode->HSPOL_HSOFF);
      g_string_append_printf(lines, " %04x", msg->preferred_display_mode->HSW);
      g_string_append_printf(lines, " %04x", msg->preferred_display_mode->V);
      g_string_append_printf(lines, " %04x", msg->preferred_display_mode->VB);
      g_string_append_printf(lines, " %04x", msg->preferred_display_mode->VSPOL_VSOFF);
      g_string_append_printf(lines, " %04x", msg->preferred_display_mode->VSW);
      g_string_append_printf(lines, " %02x", msg->preferred_display_mode->VBS3D);
      g_string_append_printf(lines, " %02x", msg->preferred_display_mode->V2d_s3d_modes);
      g_string_append_printf(lines, " %02x", msg->preferred_display_mode->P_depth);
    } else {
      g_string_append_printf(lines, GST_STRING_WFD_SPACE);
      g_string_append_printf(lines, GST_STRING_WFD_NONE);
    }
    g_string_append_printf(lines, GST_STRING_WFD_CRLF);
  }

  if (msg->standby_resume_capability) {
    g_string_append_printf(lines, GST_STRING_WFD_STANDBY_RESUME_CAPABILITY);
    g_string_append_printf(lines, GST_STRING_WFD_COLON);
    g_string_append_printf(lines, GST_STRING_WFD_SPACE);
    if (msg->standby_resume_capability->standby_resume_cap)
      g_string_append_printf(lines, GST_STRING_WFD_SUPPORTED);
    else
      g_string_append_printf(lines, GST_STRING_WFD_NONE);
    g_string_append_printf(lines, GST_STRING_WFD_CRLF);
  }

  if (msg->standby) {
    g_string_append_printf(lines, GST_STRING_WFD_STANDBY);
    g_string_append_printf(lines, GST_STRING_WFD_CRLF);
  }

  if (msg->connector_type) {
    g_string_append_printf(lines, GST_STRING_WFD_CONNECTOR_TYPE);
    g_string_append_printf(lines, GST_STRING_WFD_COLON);
    g_string_append_printf(lines, GST_STRING_WFD_CRLF);
  }

  if (msg->idr_request) {
    g_string_append_printf(lines, GST_STRING_WFD_IDR_REQUEST);
    g_string_append_printf(lines, GST_STRING_WFD_CRLF);
  }

  /*g_string_append_printf (lines, "\0"); */
  /*if(g_str_has_suffix (lines, "\r\n\0"))
  {
  guint32 length = g_strlen(lines);
  lines[length-2] = '\0';
  }*/
  return g_string_free(lines, FALSE);
}

gchar *gst_wfd_message_param_names_as_text(const GstWFDMessage *msg)
{
  /* change all vars so they match rfc? */
  GString *lines;
  g_return_val_if_fail(msg != NULL, NULL);

  lines = g_string_new("");

  /* list of audio codecs */
  if (msg->audio_codecs) {
    g_string_append_printf(lines, GST_STRING_WFD_AUDIO_CODECS);
    g_string_append_printf(lines, GST_STRING_WFD_CRLF);
  }
  /* list of video codecs */
  if (msg->video_formats) {
    g_string_append_printf(lines, GST_STRING_WFD_VIDEO_FORMATS);
    g_string_append_printf(lines, GST_STRING_WFD_CRLF);
  }
  /* list of video 3D codecs */
  if (msg->video_3d_formats) {
    g_string_append_printf(lines, GST_STRING_WFD_3D_VIDEO_FORMATS);
    g_string_append_printf(lines, GST_STRING_WFD_CRLF);
  }
  if (msg->content_protection) {
    g_string_append_printf(lines, GST_STRING_WFD_CONTENT_PROTECTION);
    g_string_append_printf(lines, GST_STRING_WFD_CRLF);
  }
  if (msg->display_edid) {
    g_string_append_printf(lines, GST_STRING_WFD_DISPLAY_EDID);
    g_string_append_printf(lines, GST_STRING_WFD_CRLF);
  }
  if (msg->coupled_sink) {
    g_string_append_printf(lines, GST_STRING_WFD_COUPLED_SINK);
    g_string_append_printf(lines, GST_STRING_WFD_CRLF);
  }
  if (msg->trigger_method) {
    g_string_append_printf(lines, GST_STRING_WFD_TRIGGER_METHOD);
    g_string_append_printf(lines, GST_STRING_WFD_CRLF);
  }
  if (msg->presentation_url) {
    g_string_append_printf(lines, GST_STRING_WFD_PRESENTATION_URL);
    g_string_append_printf(lines, GST_STRING_WFD_CRLF);
  }
  if (msg->client_rtp_ports) {
    g_string_append_printf(lines, GST_STRING_WFD_CLIENT_RTP_PORTS);
    g_string_append_printf(lines, GST_STRING_WFD_CRLF);
  }
  if (msg->route) {
    g_string_append_printf(lines, GST_STRING_WFD_ROUTE);
    g_string_append_printf(lines, GST_STRING_WFD_CRLF);
  }
  if (msg->I2C) {
    g_string_append_printf(lines, GST_STRING_WFD_I2C);
    g_string_append_printf(lines, GST_STRING_WFD_CRLF);
  }
  if (msg->av_format_change_timing) {
    g_string_append_printf(lines, GST_STRING_WFD_AV_FORMAT_CHANGE_TIMING);
    g_string_append_printf(lines, GST_STRING_WFD_CRLF);
  }
  if (msg->preferred_display_mode) {
    g_string_append_printf(lines, GST_STRING_WFD_PREFERRED_DISPLAY_MODE);
    g_string_append_printf(lines, GST_STRING_WFD_CRLF);
  }
  if (msg->standby_resume_capability) {
    g_string_append_printf(lines, GST_STRING_WFD_STANDBY_RESUME_CAPABILITY);
    g_string_append_printf(lines, GST_STRING_WFD_CRLF);
  }
  if (msg->standby) {
    g_string_append_printf(lines, GST_STRING_WFD_STANDBY);
    g_string_append_printf(lines, GST_STRING_WFD_CRLF);
  }
  if (msg->connector_type) {
    g_string_append_printf(lines, GST_STRING_WFD_CONNECTOR_TYPE);
    g_string_append_printf(lines, GST_STRING_WFD_CRLF);
  }
  if (msg->idr_request) {
    g_string_append_printf(lines, GST_STRING_WFD_IDR_REQUEST);
    g_string_append_printf(lines, GST_STRING_WFD_CRLF);
  }
  return g_string_free(lines, FALSE);
}

static void
read_string_space_ended(gchar *dest, guint size, gchar *src)
{
  guint idx = 0;

  while (!g_ascii_isspace(*src) && *src != '\0') {
    if (idx < size - 1)
      dest[idx++] = *src;
    src++;
  }

  if (size > 0)
    dest[idx] = '\0';
}

static void
read_string_type_and_value(gchar *type, gchar *value, guint tsize, guint vsize, gchar del, gchar *src)
{
  guint idx;

  idx = 0;
  while (*src != del && *src != '\0') {
    if (idx < tsize - 1)
      type[idx++] = *src;
    src++;
  }

  if (tsize > 0)
    type[idx] = '\0';

  src++;
  idx = 0;
  while (*src != '\0') {
    if (idx < vsize - 1)
      value[idx++] = *src;
    src++;
  }
  if (vsize > 0)
    value[idx] = '\0';
}

static gboolean
gst_wfd_parse_line(GstWFDMessage *msg, gchar *buffer)
{
  gchar type[8192] = {0};
  gchar value[8192] = {0};
  gchar temp[8192] = {0};
  gchar *p = buffer;
  gchar *v = value;

#define GST_WFD_SKIP_SPACE(q) if (*q && g_ascii_isspace(*q)) q++;
#define GST_WFD_SKIP_EQUAL(q) if (*q && *q == '=') q++;
#define GST_WFD_SKIP_COMMA(q) if (*q && g_ascii_ispunct(*q)) q++;
#define GST_WFD_READ_STRING(field) read_string_space_ended(temp, sizeof(temp), v); v += strlen(temp); REPLACE_STRING(field, temp);
#define GST_WFD_READ_UINT32(field) read_string_space_ended(temp, sizeof(temp), v); v += strlen(temp); field = strtoul(temp, NULL, 16);
#define GST_WFD_READ_UINT32_DIGIT(field) read_string_space_ended(temp, sizeof(temp), v); v += strlen(temp); field = strtoul(temp, NULL, 10);

  /*g_print("gst_wfd_parse_line input: %s\n", buffer); */
  read_string_type_and_value(type, value, sizeof(type), sizeof(value), ':', p);
  /*g_print("gst_wfd_parse_line type:%s value:%s\n", type, value); */
  if (!g_strcmp0(type, GST_STRING_WFD_AUDIO_CODECS)) {
    msg->audio_codecs = g_new0(GstWFDAudioCodeclist, 1);
    if (strlen(v)) {
      guint i = 0;
      msg->audio_codecs->count = strlen(v) / 16;
      msg->audio_codecs->list = g_new0(GstWFDAudioCodec, msg->audio_codecs->count);
      for (; i < msg->audio_codecs->count; i++) {
        GST_WFD_SKIP_SPACE(v);
        GST_WFD_READ_STRING(msg->audio_codecs->list[i].audio_format);
        GST_WFD_SKIP_SPACE(v);
        GST_WFD_READ_UINT32(msg->audio_codecs->list[i].modes);
        GST_WFD_SKIP_SPACE(v);
        GST_WFD_READ_UINT32(msg->audio_codecs->list[i].latency);
        GST_WFD_SKIP_COMMA(v);
      }
    }
  } else if (!g_strcmp0(type, GST_STRING_WFD_VIDEO_FORMATS)) {
    msg->video_formats = g_new0(GstWFDVideoCodeclist, 1);
    if (strlen(v)) {
      msg->video_formats->count = 1;
      msg->video_formats->list = g_new0(GstWFDVideoCodec, 1);
      GST_WFD_SKIP_SPACE(v);
      GST_WFD_READ_UINT32(msg->video_formats->list->native);
      GST_WFD_SKIP_SPACE(v);
      GST_WFD_READ_UINT32(msg->video_formats->list->preferred_display_mode_supported);
      GST_WFD_SKIP_SPACE(v);
      GST_WFD_READ_UINT32(msg->video_formats->list->H264_codec.profile);
      GST_WFD_SKIP_SPACE(v);
      GST_WFD_READ_UINT32(msg->video_formats->list->H264_codec.level);
      GST_WFD_SKIP_SPACE(v);
      GST_WFD_READ_UINT32(msg->video_formats->list->H264_codec.misc_params.CEA_Support);
      GST_WFD_SKIP_SPACE(v);
      GST_WFD_READ_UINT32(msg->video_formats->list->H264_codec.misc_params.VESA_Support);
      GST_WFD_SKIP_SPACE(v);
      GST_WFD_READ_UINT32(msg->video_formats->list->H264_codec.misc_params.HH_Support);
      GST_WFD_SKIP_SPACE(v);
      GST_WFD_READ_UINT32(msg->video_formats->list->H264_codec.misc_params.latency);
      GST_WFD_SKIP_SPACE(v);
      GST_WFD_READ_UINT32(msg->video_formats->list->H264_codec.misc_params.min_slice_size);
      GST_WFD_SKIP_SPACE(v);
      GST_WFD_READ_UINT32(msg->video_formats->list->H264_codec.misc_params.slice_enc_params);
      GST_WFD_SKIP_SPACE(v);
      GST_WFD_READ_UINT32(msg->video_formats->list->H264_codec.misc_params.frame_rate_control_support);
      GST_WFD_SKIP_SPACE(v);
      if (msg->video_formats->list->preferred_display_mode_supported == GST_WFD_PREFERRED_DISPLAY_MODE_SUPPORTED) {
        GST_WFD_READ_UINT32(msg->video_formats->list->H264_codec.max_hres);
        GST_WFD_SKIP_SPACE(v);
        GST_WFD_READ_UINT32(msg->video_formats->list->H264_codec.max_vres);
        GST_WFD_SKIP_SPACE(v);
      }
    }
  } else if (!g_strcmp0(type, GST_STRING_WFD_3D_VIDEO_FORMATS)) {
    msg->video_3d_formats = g_new0(GstWFD3DFormats, 1);
    if (strlen(v)) {
      msg->video_3d_formats->count = 1;
      msg->video_3d_formats->list = g_new0(GstWFD3dCapList, 1);
      GST_WFD_SKIP_SPACE(v);
      GST_WFD_READ_UINT32(msg->video_3d_formats->list->native);
      GST_WFD_SKIP_SPACE(v);
      GST_WFD_READ_UINT32(msg->video_3d_formats->list->preferred_display_mode_supported);
      GST_WFD_SKIP_SPACE(v);
      GST_WFD_READ_UINT32(msg->video_3d_formats->list->H264_codec.profile);
      GST_WFD_SKIP_SPACE(v);
      GST_WFD_READ_UINT32(msg->video_3d_formats->list->H264_codec.level);
      GST_WFD_SKIP_SPACE(v);
      GST_WFD_READ_UINT32(msg->video_3d_formats->list->H264_codec.misc_params.video_3d_capability);
      GST_WFD_SKIP_SPACE(v);
      GST_WFD_READ_UINT32(msg->video_3d_formats->list->H264_codec.misc_params.latency);
      GST_WFD_SKIP_SPACE(v);
      GST_WFD_READ_UINT32(msg->video_3d_formats->list->H264_codec.misc_params.min_slice_size);
      GST_WFD_SKIP_SPACE(v);
      GST_WFD_READ_UINT32(msg->video_3d_formats->list->H264_codec.misc_params.slice_enc_params);
      GST_WFD_SKIP_SPACE(v);
      GST_WFD_READ_UINT32(msg->video_3d_formats->list->H264_codec.misc_params.frame_rate_control_support);
      GST_WFD_SKIP_SPACE(v);
      if (msg->video_3d_formats->list->preferred_display_mode_supported == GST_WFD_PREFERRED_DISPLAY_MODE_SUPPORTED) {
        GST_WFD_READ_UINT32(msg->video_3d_formats->list->H264_codec.max_hres);
        GST_WFD_SKIP_SPACE(v);
        GST_WFD_READ_UINT32(msg->video_3d_formats->list->H264_codec.max_vres);
        GST_WFD_SKIP_SPACE(v);
      }
    }
  } else if (!g_strcmp0(type, GST_STRING_WFD_CONTENT_PROTECTION)) {
    msg->content_protection = g_new0(GstWFDContentProtection, 1);
    if (strlen(v)) {
      GST_WFD_SKIP_SPACE(v);
      msg->content_protection->hdcp2_spec = g_new0(GstWFDHdcp2Spec, 1);
      if (strstr(v, GST_STRING_WFD_NONE)) {
        msg->content_protection->hdcp2_spec->hdcpversion = g_strdup(GST_STRING_WFD_NONE);
      } else {
        GST_WFD_READ_STRING(msg->content_protection->hdcp2_spec->hdcpversion);
        GST_WFD_SKIP_SPACE(v);
        GST_WFD_READ_STRING(msg->content_protection->hdcp2_spec->TCPPort);
      }
    }
  } else if (!g_strcmp0(type, GST_STRING_WFD_DISPLAY_EDID)) {
    msg->display_edid = g_new0(GstWFDDisplayEdid, 1);
    if (strlen(v)) {
      GST_WFD_SKIP_SPACE(v);
      if (strstr(v, GST_STRING_WFD_NONE)) {
        msg->display_edid->edid_supported = 0;
      } else {
        msg->display_edid->edid_supported = 1;
        GST_WFD_READ_UINT32(msg->display_edid->edid_block_count);
        GST_WFD_SKIP_SPACE(v);
        if (msg->display_edid->edid_block_count) {
          gchar *edid_string = v;
          int i = 0, j = 0, size = 0;
          guint32 payload_size = EDID_BLOCK_SIZE * msg->display_edid->edid_block_count;
          msg->display_edid->edid_payload = g_malloc(payload_size);
          size = EDID_BLOCK_SIZE * msg->display_edid->edid_block_count * 2;
          for (; i < size; j++) {
            int k = 0, kk = 0;
            if (edid_string[i] > 0x29 && edid_string[i] < 0x40) k = edid_string[i] - 48;
            else if (edid_string[i] > 0x60 && edid_string[i] < 0x67) k = edid_string[i] - 87;
            else if (edid_string[i] > 0x40 && edid_string[i] < 0x47) k = edid_string[i] - 55;

            if (edid_string[i + 1] > 0x29 && edid_string[i + 1] < 0x40) kk = edid_string[i + 1] - 48;
            else if (edid_string[i + 1] > 0x60 && edid_string[i + 1] < 0x67) kk = edid_string[i + 1] - 87;
            else if (edid_string[i + 1] > 0x40 && edid_string[i + 1] < 0x47) kk = edid_string[i + 1] - 55;

            msg->display_edid->edid_payload[j] = (k << 4) | kk;
            i += 2;
          }
          /*memcpy(msg->display_edid->edid_payload, v, payload_size); */
          v += (payload_size * 2);
        } else v += strlen(v);
      }
    }
  } else if (!g_strcmp0(type, GST_STRING_WFD_COUPLED_SINK)) {
    msg->coupled_sink = g_new0(GstWFDCoupledSink, 1);
    if (strlen(v)) {
      msg->coupled_sink->coupled_sink_cap = g_new0(GstWFDCoupledSinkCap, 1);
      GST_WFD_SKIP_SPACE(v);
      GST_WFD_READ_UINT32(msg->coupled_sink->coupled_sink_cap->status);
      GST_WFD_SKIP_SPACE(v);
      GST_WFD_READ_STRING(msg->coupled_sink->coupled_sink_cap->sink_address);
    }
  } else if (!g_strcmp0(type, GST_STRING_WFD_TRIGGER_METHOD)) {
    msg->trigger_method = g_new0(GstWFDTriggerMethod, 1);
    if (strlen(v)) {
      GST_WFD_SKIP_SPACE(v);
      GST_WFD_READ_STRING(msg->trigger_method->wfd_trigger_method);
    }
  } else if (!g_strcmp0(type, GST_STRING_WFD_PRESENTATION_URL)) {
    msg->presentation_url = g_new0(GstWFDPresentationUrl, 1);
    if (strlen(v)) {
      GST_WFD_SKIP_SPACE(v);
      GST_WFD_READ_STRING(msg->presentation_url->wfd_url0);
      GST_WFD_SKIP_SPACE(v);
      GST_WFD_READ_STRING(msg->presentation_url->wfd_url1);
    }
  } else if (!g_strcmp0(type, GST_STRING_WFD_CLIENT_RTP_PORTS)) {
    msg->client_rtp_ports = g_new0(GstWFDClientRtpPorts, 1);
    if (strlen(v)) {
      GST_WFD_SKIP_SPACE(v);
      GST_WFD_READ_STRING(msg->client_rtp_ports->profile);
      GST_WFD_SKIP_SPACE(v);
      GST_WFD_READ_UINT32_DIGIT(msg->client_rtp_ports->rtp_port0);
      GST_WFD_SKIP_SPACE(v);
      GST_WFD_READ_UINT32_DIGIT(msg->client_rtp_ports->rtp_port1);
      GST_WFD_SKIP_SPACE(v);
      GST_WFD_READ_STRING(msg->client_rtp_ports->mode);
    }
  } else if (!g_strcmp0(type, GST_STRING_WFD_ROUTE)) {
    msg->route = g_new0(GstWFDRoute, 1);
    if (strlen(v)) {
      GST_WFD_SKIP_SPACE(v);
      GST_WFD_READ_STRING(msg->route->destination);
    }
  } else if (!g_strcmp0(type, GST_STRING_WFD_I2C)) {
    msg->I2C = g_new0(GstWFDI2C, 1);
    if (strlen(v)) {
      msg->I2C->I2CPresent = TRUE;
      GST_WFD_SKIP_SPACE(v);
      GST_WFD_READ_UINT32_DIGIT(msg->I2C->I2C_port);
      if (msg->I2C->I2C_port) msg->I2C->I2CPresent = TRUE;
    }
  } else if (!g_strcmp0(type, GST_STRING_WFD_AV_FORMAT_CHANGE_TIMING)) {
    msg->av_format_change_timing = g_new0(GstWFDAVFormatChangeTiming, 1);
    if (strlen(v)) {
      GST_WFD_SKIP_SPACE(v);
      GST_WFD_READ_UINT32(msg->av_format_change_timing->PTS);
      GST_WFD_SKIP_SPACE(v);
      GST_WFD_READ_UINT32(msg->av_format_change_timing->DTS);
    }
  } else if (!g_strcmp0(type, GST_STRING_WFD_PREFERRED_DISPLAY_MODE)) {
    msg->preferred_display_mode = g_new0(GstWFDPreferredDisplayMode, 1);
    if (strlen(v)) {
      GST_WFD_SKIP_SPACE(v);
      if (!strstr(v, GST_STRING_WFD_NONE)) {
        msg->preferred_display_mode->displaymodesupported = FALSE;
      } else {
        GST_WFD_READ_UINT32(msg->preferred_display_mode->p_clock);
        GST_WFD_SKIP_SPACE(v);
        GST_WFD_READ_UINT32(msg->preferred_display_mode->H);
        GST_WFD_SKIP_SPACE(v);
        GST_WFD_READ_UINT32(msg->preferred_display_mode->HB);
        GST_WFD_SKIP_SPACE(v);
        GST_WFD_READ_UINT32(msg->preferred_display_mode->HSPOL_HSOFF);
        GST_WFD_SKIP_SPACE(v);
        GST_WFD_READ_UINT32(msg->preferred_display_mode->HSW);
        GST_WFD_SKIP_SPACE(v);
        GST_WFD_READ_UINT32(msg->preferred_display_mode->V);
        GST_WFD_SKIP_SPACE(v);
        GST_WFD_READ_UINT32(msg->preferred_display_mode->VB);
        GST_WFD_SKIP_SPACE(v);
        GST_WFD_READ_UINT32(msg->preferred_display_mode->VSPOL_VSOFF);
        GST_WFD_SKIP_SPACE(v);
        GST_WFD_READ_UINT32(msg->preferred_display_mode->VSW);
        GST_WFD_SKIP_SPACE(v);
        GST_WFD_READ_UINT32(msg->preferred_display_mode->VBS3D);
        GST_WFD_SKIP_SPACE(v);
        GST_WFD_READ_UINT32(msg->preferred_display_mode->V2d_s3d_modes);
        GST_WFD_SKIP_SPACE(v);
        GST_WFD_READ_UINT32(msg->preferred_display_mode->P_depth);
        GST_WFD_SKIP_SPACE(v);
        GST_WFD_READ_UINT32(msg->preferred_display_mode->H264_codec.profile);
        GST_WFD_SKIP_SPACE(v);
        GST_WFD_READ_UINT32(msg->preferred_display_mode->H264_codec.level);
        GST_WFD_SKIP_SPACE(v);
        GST_WFD_READ_UINT32(msg->preferred_display_mode->H264_codec.misc_params.CEA_Support);
        GST_WFD_SKIP_SPACE(v);
        GST_WFD_READ_UINT32(msg->preferred_display_mode->H264_codec.misc_params.VESA_Support);
        GST_WFD_SKIP_SPACE(v);
        GST_WFD_READ_UINT32(msg->preferred_display_mode->H264_codec.misc_params.HH_Support);
        GST_WFD_SKIP_SPACE(v);
        GST_WFD_READ_UINT32(msg->preferred_display_mode->H264_codec.misc_params.latency);
        GST_WFD_SKIP_SPACE(v);
        GST_WFD_READ_UINT32(msg->preferred_display_mode->H264_codec.misc_params.min_slice_size);
        GST_WFD_SKIP_SPACE(v);
        GST_WFD_READ_UINT32(msg->preferred_display_mode->H264_codec.misc_params.slice_enc_params);
        GST_WFD_SKIP_SPACE(v);
        GST_WFD_READ_UINT32(msg->preferred_display_mode->H264_codec.misc_params.frame_rate_control_support);
        GST_WFD_SKIP_SPACE(v);
        GST_WFD_READ_UINT32(msg->preferred_display_mode->H264_codec.max_hres);
        GST_WFD_SKIP_SPACE(v);
        GST_WFD_READ_UINT32(msg->preferred_display_mode->H264_codec.max_vres);
        GST_WFD_SKIP_SPACE(v);
      }
    }
  } else if (!g_strcmp0(type, GST_STRING_WFD_STANDBY_RESUME_CAPABILITY)) {
    msg->standby_resume_capability = g_new0(GstWFDStandbyResumeCapability, 1);
    if (strlen(v)) {
      GST_WFD_SKIP_SPACE(v);
      if (!g_strcmp0(v, GST_STRING_WFD_SUPPORTED))
        msg->standby_resume_capability->standby_resume_cap = TRUE;
      else
        msg->standby_resume_capability->standby_resume_cap = FALSE;
    }
  } else if (!g_strcmp0(type, GST_STRING_WFD_STANDBY)) {
    msg->standby = g_new0(GstWFDStandby, 1);
    msg->standby->wfd_standby = TRUE;
  } else if (!g_strcmp0(type, GST_STRING_WFD_CONNECTOR_TYPE)) {
    msg->connector_type = g_new0(GstWFDConnectorType, 1);
    if (strlen(v)) {
      msg->connector_type->supported = TRUE;
      GST_WFD_SKIP_SPACE(v);
      GST_WFD_READ_UINT32(msg->connector_type->connector_type);
    }
  } else if (!g_strcmp0(type, GST_STRING_WFD_IDR_REQUEST)) {
    msg->idr_request = g_new0(GstWFDIdrRequest, 1);
    msg->idr_request->idr_request = TRUE;
  }

  return TRUE;
}

/**
* gst_wfd_message_parse_buffer:
* @data: the start of the buffer
* @size: the size of the buffer
* @msg: the result #GstWFDMessage
*
* Parse the contents of @size bytes pointed to by @data and store the result in
* @msg.
*
* Returns: #GST_WFD_OK on success.
*/
GstWFDResult
gst_wfd_message_parse_buffer(const guint8 *data, guint size, GstWFDMessage *msg)
{
  const gchar *p;
  gchar buffer[MAX_LINE_LEN] = {0};
  guint idx = 0;

  g_return_val_if_fail(msg != NULL, GST_WFD_EINVAL);
  g_return_val_if_fail(data != NULL, GST_WFD_EINVAL);
  g_return_val_if_fail(size != 0, GST_WFD_EINVAL);

  p = (const gchar *) data;
  while (TRUE) {

    if (*p == '\0')
      break;

    idx = 0;
    while (*p != '\n' && *p != '\r' && *p != '\0') {
      if (idx < sizeof(buffer) - 1)
        buffer[idx++] = *p;
      p++;
    }
    buffer[idx] = '\0';
    gst_wfd_parse_line(msg, buffer);

    if (*p == '\0')
      break;
    p += 2;
  }

  return GST_WFD_OK;
}

/**
* gst_wfd_message_dump:
* @msg: a #GstWFDMessage
*
* Dump the parsed contents of @msg to stdout.
*
* Returns: a #GstWFDResult.
*/
GstWFDResult
gst_wfd_message_dump(const GstWFDMessage *msg)
{
  g_return_val_if_fail(msg != NULL, GST_WFD_EINVAL);
  g_print("===========WFD Message dump=========");

  if (msg->audio_codecs) {
    guint i = 0;
    g_print("Audio supported formats : ");
    for (; i < msg->audio_codecs->count; i++) {
      g_print("Codec: %s", msg->audio_codecs->list[i].audio_format);
      if (!strcmp(msg->audio_codecs->list[i].audio_format, GST_STRING_WFD_LPCM)) {
        if (msg->audio_codecs->list[i].modes & GST_WFD_FREQ_44100)
          g_print("  Freq: %d", 44100);
        if (msg->audio_codecs->list[i].modes & GST_WFD_FREQ_48000)
          g_print("  Freq: %d", 48000);
        g_print("  Channels: %d", 2);
      }
      if (!strcmp(msg->audio_codecs->list[i].audio_format, GST_STRING_WFD_AAC)) {
        g_print("  Freq: %d", 48000);
        if (msg->audio_codecs->list[i].modes & GST_WFD_CHANNEL_2)
          g_print("  Channels: %d", 2);
        if (msg->audio_codecs->list[i].modes & GST_WFD_CHANNEL_4)
          g_print("  Channels: %d", 4);
        if (msg->audio_codecs->list[i].modes & GST_WFD_CHANNEL_6)
          g_print("  Channels: %d", 6);
        if (msg->audio_codecs->list[i].modes & GST_WFD_CHANNEL_8)
          g_print("  Channels: %d", 8);
      }
      if (!strcmp(msg->audio_codecs->list[i].audio_format, GST_STRING_WFD_AC3)) {
        g_print("  Freq: %d", 48000);
        if (msg->audio_codecs->list[i].modes & GST_WFD_CHANNEL_2)
          g_print("  Channels: %d", 2);
        if (msg->audio_codecs->list[i].modes & GST_WFD_CHANNEL_4)
          g_print("  Channels: %d", 4);
        if (msg->audio_codecs->list[i].modes & GST_WFD_CHANNEL_6)
          g_print("  Channels: %d", 6);
      }
      g_print("  Bitwidth: %d", 16);
      g_print("  Latency: %d", msg->audio_codecs->list[i].latency);
    }
  }


  if (msg->video_formats) {
    g_print("Video supported formats : ");
    if (msg->video_formats->list) {
      g_print("Codec: H264");
      guint nativeindex = 0;
      if ((msg->video_formats->list->native & 0x7) == GST_WFD_VIDEO_CEA_RESOLUTION) {
        g_print("  Native type: CEA");
      } else if ((msg->video_formats->list->native & 0x7) == GST_WFD_VIDEO_VESA_RESOLUTION) {
        g_print("  Native type: VESA");
      } else if ((msg->video_formats->list->native & 0x7) == GST_WFD_VIDEO_HH_RESOLUTION) {
        g_print("  Native type: HH");
      }
      nativeindex = msg->video_formats->list->native >> 3;
      g_print("  Resolution: %d", (1 << nativeindex));

      if (msg->video_formats->list->H264_codec.profile & GST_WFD_H264_BASE_PROFILE) {
        g_print("  Profile: BASE");
      } else if (msg->video_formats->list->H264_codec.profile & GST_WFD_H264_HIGH_PROFILE) {
        g_print("  Profile: HIGH");
      }
      if (msg->video_formats->list->H264_codec.level & GST_WFD_H264_LEVEL_3_1) {
        g_print("  Level: 3.1");
      } else if (msg->video_formats->list->H264_codec.level & GST_WFD_H264_LEVEL_3_2) {
        g_print("  Level: 3.2");
      } else if (msg->video_formats->list->H264_codec.level & GST_WFD_H264_LEVEL_4) {
        g_print("  Level: 4");
      } else if (msg->video_formats->list->H264_codec.level & GST_WFD_H264_LEVEL_4_1) {
        g_print("  Level: 4.1");
      } else if (msg->video_formats->list->H264_codec.level & GST_WFD_H264_LEVEL_4_2) {
        g_print("  Level: 4.2");
      }
      g_print("  Latency: %d", msg->video_formats->list->H264_codec.misc_params.latency);
      g_print("  min_slice_size: %x", msg->video_formats->list->H264_codec.misc_params.min_slice_size);
      g_print("  slice_enc_params: %x", msg->video_formats->list->H264_codec.misc_params.slice_enc_params);
      g_print("  frame_rate_control_support: %x", msg->video_formats->list->H264_codec.misc_params.frame_rate_control_support);
      if (msg->video_formats->list->H264_codec.max_hres) {
        g_print("  Max Width: %04d", msg->video_formats->list->H264_codec.max_hres);
      }
      if (msg->video_formats->list->H264_codec.max_vres) {
        g_print("  Max Height: %04d", msg->video_formats->list->H264_codec.max_vres);
      }
    }
  }

  if (msg->video_3d_formats) {
    g_print("wfd_3d_formats");
  }

  if (msg->content_protection) {
    g_print(GST_STRING_WFD_CONTENT_PROTECTION);
  }

  if (msg->display_edid) {
    g_print(GST_STRING_WFD_DISPLAY_EDID);
  }

  if (msg->coupled_sink) {
    g_print(GST_STRING_WFD_COUPLED_SINK);
  }

  if (msg->trigger_method) {
    g_print("  Trigger type: %s", msg->trigger_method->wfd_trigger_method);
  }

  if (msg->presentation_url) {
    g_print(GST_STRING_WFD_PRESENTATION_URL);
  }

  if (msg->client_rtp_ports) {
    g_print(" Client RTP Ports : ");
    if (msg->client_rtp_ports->profile) {
      g_print("%s", msg->client_rtp_ports->profile);
      g_print("  %d", msg->client_rtp_ports->rtp_port0);
      g_print("  %d", msg->client_rtp_ports->rtp_port1);
      g_print("  %s", msg->client_rtp_ports->mode);
    }
  }

  if (msg->route) {
    g_print(GST_STRING_WFD_ROUTE);
  }

  if (msg->I2C) {
    g_print(GST_STRING_WFD_I2C);
  }

  if (msg->av_format_change_timing) {
    g_print(GST_STRING_WFD_AV_FORMAT_CHANGE_TIMING);
  }

  if (msg->preferred_display_mode) {
    g_print(GST_STRING_WFD_PREFERRED_DISPLAY_MODE);
  }

  if (msg->standby_resume_capability) {
    g_print(GST_STRING_WFD_STANDBY_RESUME_CAPABILITY);
  }

  if (msg->standby) {
    g_print(GST_STRING_WFD_STANDBY);
  }

  if (msg->connector_type) {
    g_print(GST_STRING_WFD_CONNECTOR_TYPE);
  }

  if (msg->idr_request) {
    g_print(GST_STRING_WFD_IDR_REQUEST);
  }

  g_print("===============================================");
  return GST_WFD_OK;
}

GstWFDResult gst_wfd_message_set_supported_audio_format(GstWFDMessage *msg, GstWFDAudioFormats aCodec, guint aFreq, guint aChanels,
                                               guint aBitwidth, guint32 aLatency)
{
  guint temp = aCodec;
  guint i = 0;
  guint pcm = 0, aac = 0, ac3 = 0;

  g_return_val_if_fail(msg != NULL, GST_WFD_EINVAL);

  if (!msg->audio_codecs)
    msg->audio_codecs = g_new0(GstWFDAudioCodeclist, 1);

  if (aCodec != GST_WFD_AUDIO_UNKNOWN) {
    while (temp) {
      msg->audio_codecs->count++;
      temp >>= 1;
    }
    msg->audio_codecs->list = g_new0(GstWFDAudioCodec, msg->audio_codecs->count);
    for (; i < msg->audio_codecs->count; i++) {
      if ((aCodec & GST_WFD_AUDIO_LPCM) && (!pcm)) {
        msg->audio_codecs->list[i].audio_format = g_strdup(GST_STRING_WFD_LPCM);
        msg->audio_codecs->list[i].modes = aFreq;
        msg->audio_codecs->list[i].latency = aLatency;
        pcm = 1;
      } else if ((aCodec & GST_WFD_AUDIO_AAC) && (!aac)) {
        msg->audio_codecs->list[i].audio_format = g_strdup(GST_STRING_WFD_AAC);
        msg->audio_codecs->list[i].modes = aChanels;
        msg->audio_codecs->list[i].latency = aLatency;
        aac = 1;
      } else if ((aCodec & GST_WFD_AUDIO_AC3) && (!ac3)) {
        msg->audio_codecs->list[i].audio_format = g_strdup(GST_STRING_WFD_AC3);
        msg->audio_codecs->list[i].modes = aChanels;
        msg->audio_codecs->list[i].latency = aLatency;
        ac3 = 1;
      }
    }
  }
  return GST_WFD_OK;
}

GstWFDResult gst_wfd_message_set_prefered_audio_format(GstWFDMessage *msg, GstWFDAudioFormats aCodec, GstWFDAudioFreq aFreq, GstWFDAudioChannels aChanels,
                                              guint aBitwidth, guint32 aLatency)
{

  g_return_val_if_fail(msg != NULL, GST_WFD_EINVAL);

  if (!msg->audio_codecs)
    msg->audio_codecs = g_new0(GstWFDAudioCodeclist, 1);

  msg->audio_codecs->list = g_new0(GstWFDAudioCodec, 1);
  msg->audio_codecs->count = 1;
  if (aCodec == GST_WFD_AUDIO_LPCM) {
    msg->audio_codecs->list->audio_format = g_strdup(GST_STRING_WFD_LPCM);
    msg->audio_codecs->list->modes = aFreq;
    msg->audio_codecs->list->latency = aLatency;
  } else if (aCodec == GST_WFD_AUDIO_AAC) {
    msg->audio_codecs->list->audio_format = g_strdup(GST_STRING_WFD_AAC);
    msg->audio_codecs->list->modes = aChanels;
    msg->audio_codecs->list->latency = aLatency;
  } else if (aCodec == GST_WFD_AUDIO_AC3) {
    msg->audio_codecs->list->audio_format = g_strdup(GST_STRING_WFD_AC3);
    msg->audio_codecs->list->modes = aChanels;
    msg->audio_codecs->list->latency = aLatency;
  }
  return GST_WFD_OK;
}

GstWFDResult gst_wfd_message_get_supported_audio_format(GstWFDMessage *msg, guint *aCodec, guint *aFreq, guint *aChanels,
                                               guint *aBitwidth, guint32 *aLatency)
{
  guint i = 0;
  g_return_val_if_fail(msg != NULL, GST_WFD_EINVAL);
  g_return_val_if_fail(msg->audio_codecs != NULL, GST_WFD_EINVAL);

  for (; i < msg->audio_codecs->count; i++) {
    if (!g_strcmp0(msg->audio_codecs->list[i].audio_format, GST_STRING_WFD_LPCM)) {
      *aCodec |= GST_WFD_AUDIO_LPCM;
      *aFreq |= msg->audio_codecs->list[i].modes;
      *aChanels |= GST_WFD_CHANNEL_2;
      *aBitwidth = 16;
      *aLatency = msg->audio_codecs->list[i].latency;
    } else if (!g_strcmp0(msg->audio_codecs->list[i].audio_format, GST_STRING_WFD_AAC)) {
      *aCodec |= GST_WFD_AUDIO_AAC;
      *aFreq |= GST_WFD_FREQ_48000;
      *aChanels |= msg->audio_codecs->list[i].modes;
      *aBitwidth = 16;
      *aLatency = msg->audio_codecs->list[i].latency;
    } else if (!g_strcmp0(msg->audio_codecs->list[i].audio_format, GST_STRING_WFD_AC3)) {
      *aCodec |= GST_WFD_AUDIO_AC3;
      *aFreq |= GST_WFD_FREQ_48000;
      *aChanels |= msg->audio_codecs->list[i].modes;
      *aBitwidth = 16;
      *aLatency = msg->audio_codecs->list[i].latency;
    }
  }
  return GST_WFD_OK;
}

GstWFDResult gst_wfd_message_get_prefered_audio_format(GstWFDMessage *msg, GstWFDAudioFormats *aCodec, GstWFDAudioFreq *aFreq, GstWFDAudioChannels *aChanels,
                                              guint *aBitwidth, guint32 *aLatency)
{
  g_return_val_if_fail(msg != NULL, GST_WFD_EINVAL);

  if (!g_strcmp0(msg->audio_codecs->list->audio_format, GST_STRING_WFD_LPCM)) {
    *aCodec = GST_WFD_AUDIO_LPCM;
    *aFreq = msg->audio_codecs->list->modes;
    *aChanels = GST_WFD_CHANNEL_2;
    *aBitwidth = 16;
    *aLatency = msg->audio_codecs->list->latency;
  } else if (!g_strcmp0(msg->audio_codecs->list->audio_format, GST_STRING_WFD_AAC)) {
    *aCodec = GST_WFD_AUDIO_AAC;
    *aFreq = GST_WFD_FREQ_48000;
    *aChanels = msg->audio_codecs->list->modes;
    *aBitwidth = 16;
    *aLatency = msg->audio_codecs->list->latency;
  } else if (!g_strcmp0(msg->audio_codecs->list->audio_format, GST_STRING_WFD_AC3)) {
    *aCodec = GST_WFD_AUDIO_AC3;
    *aFreq = GST_WFD_FREQ_48000;
    *aChanels = msg->audio_codecs->list->modes;
    *aBitwidth = 16;
    *aLatency = msg->audio_codecs->list->latency;
  }
  return GST_WFD_OK;
}

GstWFDResult gst_wfd_message_set_supported_video_format(GstWFDMessage *msg, GstWFDVideoCodecs vCodec,
                                               GstWFDVideoNativeResolution vNative, guint64 vNativeResolution,
                                               guint64 vCEAResolution, guint64 vVESAResolution, guint64 vHHResolution,
                                               guint vProfile, guint vLevel, guint32 vLatency, guint32 vMaxHeight,
                                               guint32 vMaxWidth, guint32 min_slice_size, guint32 slice_enc_params, guint frame_rate_control,
                                               guint preferred_display_mode)
{
  guint nativeindex = 0;
  guint64 temp = vNativeResolution;

  g_return_val_if_fail(msg != NULL, GST_WFD_EINVAL);

  if (!msg->video_formats)
    msg->video_formats = g_new0(GstWFDVideoCodeclist, 1);

  if (vCodec != GST_WFD_VIDEO_UNKNOWN) {
    msg->video_formats->list = g_new0(GstWFDVideoCodec, 1);
    while (temp) {
      nativeindex++;
      temp >>= 1;
    }

    msg->video_formats->list->native = nativeindex - 1;
    msg->video_formats->list->native <<= 3;

    if (vNative == GST_WFD_VIDEO_VESA_RESOLUTION)
      msg->video_formats->list->native |= 1;
    else if (vNative == GST_WFD_VIDEO_HH_RESOLUTION)
      msg->video_formats->list->native |= 2;

    msg->video_formats->list->preferred_display_mode_supported = preferred_display_mode;
    msg->video_formats->list->H264_codec.profile = vProfile;
    msg->video_formats->list->H264_codec.level = vLevel;
    msg->video_formats->list->H264_codec.max_hres = vMaxWidth;
    msg->video_formats->list->H264_codec.max_vres = vMaxHeight;
    msg->video_formats->list->H264_codec.misc_params.CEA_Support = vCEAResolution;
    msg->video_formats->list->H264_codec.misc_params.VESA_Support = vVESAResolution;
    msg->video_formats->list->H264_codec.misc_params.HH_Support = vHHResolution;
    msg->video_formats->list->H264_codec.misc_params.latency = vLatency;
    msg->video_formats->list->H264_codec.misc_params.min_slice_size = min_slice_size;
    msg->video_formats->list->H264_codec.misc_params.slice_enc_params = slice_enc_params;
    msg->video_formats->list->H264_codec.misc_params.frame_rate_control_support = frame_rate_control;
  }
  return GST_WFD_OK;
}

GstWFDResult gst_wfd_message_set_prefered_video_format(GstWFDMessage *msg, GstWFDVideoCodecs vCodec,
                                              GstWFDVideoNativeResolution vNative, guint64 vNativeResolution,
                                              GstWFDVideoCEAResolution vCEAResolution, GstWFDVideoVESAResolution vVESAResolution,
                                              GstWFDVideoHHResolution vHHResolution,  GstWFDVideoH264Profile vProfile,
                                              GstWFDVideoH264Level vLevel, guint32 vLatency, guint32 vMaxHeight,
                                              guint32 vMaxWidth, guint32 min_slice_size, guint32 slice_enc_params, guint frame_rate_control)
{
  guint nativeindex = 0;
  guint64 temp = vNativeResolution;

  g_return_val_if_fail(msg != NULL, GST_WFD_EINVAL);

  if (!msg->video_formats)
    msg->video_formats = g_new0(GstWFDVideoCodeclist, 1);
  msg->video_formats->list = g_new0(GstWFDVideoCodec, 1);

  while (temp) {
    nativeindex++;
    temp >>= 1;
  }

  if (nativeindex) msg->video_formats->list->native = nativeindex - 1;
  msg->video_formats->list->native <<= 3;

  if (vNative == GST_WFD_VIDEO_VESA_RESOLUTION)
    msg->video_formats->list->native |= 1;
  else if (vNative == GST_WFD_VIDEO_HH_RESOLUTION)
    msg->video_formats->list->native |= 2;

  msg->video_formats->list->preferred_display_mode_supported = GST_WFD_PREFERRED_DISPLAY_MODE_NOT_SUPPORTED;
  msg->video_formats->list->H264_codec.profile = vProfile;
  msg->video_formats->list->H264_codec.level = vLevel;
  msg->video_formats->list->H264_codec.max_hres = vMaxWidth;
  msg->video_formats->list->H264_codec.max_vres = vMaxHeight;
  msg->video_formats->list->H264_codec.misc_params.CEA_Support = vCEAResolution;
  msg->video_formats->list->H264_codec.misc_params.VESA_Support = vVESAResolution;
  msg->video_formats->list->H264_codec.misc_params.HH_Support = vHHResolution;
  msg->video_formats->list->H264_codec.misc_params.latency = vLatency;
  msg->video_formats->list->H264_codec.misc_params.min_slice_size = min_slice_size;
  msg->video_formats->list->H264_codec.misc_params.slice_enc_params = slice_enc_params;
  msg->video_formats->list->H264_codec.misc_params.frame_rate_control_support = frame_rate_control;
  return GST_WFD_OK;
}

GstWFDResult gst_wfd_message_get_supported_video_format(GstWFDMessage *msg, GstWFDVideoCodecs *vCodec,
                                               GstWFDVideoNativeResolution *vNative, guint64 *vNativeResolution,
                                               guint64 *vCEAResolution, guint64 *vVESAResolution, guint64 *vHHResolution,
                                               guint *vProfile, guint *vLevel, guint32 *vLatency, guint32 *vMaxHeight,
                                               guint32 *vMaxWidth, guint32 *min_slice_size, guint32 *slice_enc_params, guint *frame_rate_control)
{
  guint nativeindex = 0;

  g_return_val_if_fail(msg != NULL, GST_WFD_EINVAL);
  g_return_val_if_fail(msg->video_formats != NULL, GST_WFD_EINVAL);
  g_return_val_if_fail(msg->video_formats->list != NULL, GST_WFD_EINVAL);

  *vCodec = GST_WFD_VIDEO_H264;
  *vNative = msg->video_formats->list->native & 0x7;
  nativeindex = msg->video_formats->list->native >> 3;
  *vNativeResolution = (guint64)1 << nativeindex;
  *vProfile = msg->video_formats->list->H264_codec.profile;
  *vLevel = msg->video_formats->list->H264_codec.level;
  *vMaxWidth = msg->video_formats->list->H264_codec.max_hres;
  *vMaxHeight = msg->video_formats->list->H264_codec.max_vres;
  *vCEAResolution = msg->video_formats->list->H264_codec.misc_params.CEA_Support;
  *vVESAResolution = msg->video_formats->list->H264_codec.misc_params.VESA_Support;
  *vHHResolution = msg->video_formats->list->H264_codec.misc_params.HH_Support;
  *vLatency = msg->video_formats->list->H264_codec.misc_params.latency;
  *min_slice_size = msg->video_formats->list->H264_codec.misc_params.min_slice_size;
  *slice_enc_params = msg->video_formats->list->H264_codec.misc_params.slice_enc_params;
  *frame_rate_control = msg->video_formats->list->H264_codec.misc_params.frame_rate_control_support;
  return GST_WFD_OK;
}

GstWFDResult gst_wfd_message_get_prefered_video_format(GstWFDMessage *msg, GstWFDVideoCodecs *vCodec,
                                              GstWFDVideoNativeResolution *vNative, guint64 *vNativeResolution,
                                              GstWFDVideoCEAResolution *vCEAResolution, GstWFDVideoVESAResolution *vVESAResolution,
                                              GstWFDVideoHHResolution *vHHResolution,  GstWFDVideoH264Profile *vProfile,
                                              GstWFDVideoH264Level *vLevel, guint32 *vLatency, guint32 *vMaxHeight,
                                              guint32 *vMaxWidth, guint32 *min_slice_size, guint32 *slice_enc_params, guint *frame_rate_control)
{
  guint nativeindex = 0;
  g_return_val_if_fail(msg != NULL, GST_WFD_EINVAL);
  g_return_val_if_fail(msg->video_formats != NULL, GST_WFD_EINVAL);
  g_return_val_if_fail(msg->video_formats->list != NULL, GST_WFD_EINVAL);

  *vCodec = GST_WFD_VIDEO_H264;
  *vNative = msg->video_formats->list->native & 0x7;
  nativeindex = msg->video_formats->list->native >> 3;
  *vNativeResolution = (guint64)1 << nativeindex;
  *vProfile = msg->video_formats->list->H264_codec.profile;
  *vLevel = msg->video_formats->list->H264_codec.level;
  *vMaxWidth = msg->video_formats->list->H264_codec.max_hres;
  *vMaxHeight = msg->video_formats->list->H264_codec.max_vres;
  *vCEAResolution = msg->video_formats->list->H264_codec.misc_params.CEA_Support;
  *vVESAResolution = msg->video_formats->list->H264_codec.misc_params.VESA_Support;
  *vHHResolution = msg->video_formats->list->H264_codec.misc_params.HH_Support;
  *vLatency = msg->video_formats->list->H264_codec.misc_params.latency;
  *min_slice_size = msg->video_formats->list->H264_codec.misc_params.min_slice_size;
  *slice_enc_params = msg->video_formats->list->H264_codec.misc_params.slice_enc_params;
  *frame_rate_control = msg->video_formats->list->H264_codec.misc_params.frame_rate_control_support;
  return GST_WFD_OK;
}

GstWFDResult gst_wfd_message_set_contentprotection_type(GstWFDMessage *msg, GstWFDHDCPProtection hdcpversion, guint32 TCPPort)
{
  g_return_val_if_fail(msg != NULL, GST_WFD_EINVAL);
  g_return_val_if_fail(TCPPort <= MAX_PORT_SIZE, GST_WFD_EINVAL);

  if (!msg->content_protection) msg->content_protection = g_new0(GstWFDContentProtection, 1);
  if (hdcpversion == GST_WFD_HDCP_NONE) return GST_WFD_OK;

  msg->content_protection->hdcp2_spec = g_new0(GstWFDHdcp2Spec, 1);
  if (hdcpversion == GST_WFD_HDCP_2_0) msg->content_protection->hdcp2_spec->hdcpversion = g_strdup(GST_STRING_WFD_HDCP2_0);
  else if (hdcpversion == GST_WFD_HDCP_2_1) msg->content_protection->hdcp2_spec->hdcpversion = g_strdup(GST_STRING_WFD_HDCP2_1);
  char str[11] = {0, };
  snprintf(str, 11, "port=%d", TCPPort);
  msg->content_protection->hdcp2_spec->TCPPort = g_strdup(str);
  return GST_WFD_OK;
}

GstWFDResult gst_wfd_message_get_contentprotection_type(GstWFDMessage *msg, GstWFDHDCPProtection *hdcpversion, guint32 *TCPPort)
{
  g_return_val_if_fail(msg != NULL, GST_WFD_EINVAL);
  if (msg->content_protection && msg->content_protection->hdcp2_spec) {
    char *result = NULL;
    char *ptr = NULL;
    if (!g_strcmp0(msg->content_protection->hdcp2_spec->hdcpversion, GST_STRING_WFD_NONE)) {
      g_print("HDCP none");
      *hdcpversion = GST_WFD_HDCP_NONE;
      *TCPPort = 0;
      return GST_WFD_OK;
    }
    if (!g_strcmp0(msg->content_protection->hdcp2_spec->hdcpversion, GST_STRING_WFD_HDCP2_0)) *hdcpversion = GST_WFD_HDCP_2_0;
    else if (!g_strcmp0(msg->content_protection->hdcp2_spec->hdcpversion, GST_STRING_WFD_HDCP2_1)) *hdcpversion = GST_WFD_HDCP_2_1;
    else {
      g_print("Unknown protection type");
      *hdcpversion = GST_WFD_HDCP_NONE;
      *TCPPort = 0;
      return GST_WFD_OK;
    }

    result = strtok_r(msg->content_protection->hdcp2_spec->TCPPort, GST_STRING_WFD_EQUALS, &ptr);
    while (result != NULL) {
      result = strtok_r(NULL, GST_STRING_WFD_EQUALS, &ptr);
      *TCPPort = atoi(result);
      break;
    }
  } else *hdcpversion = GST_WFD_HDCP_NONE;
  return GST_WFD_OK;
}

GstWFDResult gst_wfd_message_set_display_EDID(GstWFDMessage *msg, gboolean edid_supported, guint32 edid_blockcount, gchar *edid_playload)
{
  g_return_val_if_fail(msg != NULL, GST_WFD_EINVAL);
  if (!msg->display_edid) msg->display_edid = g_new0(GstWFDDisplayEdid, 1);
  msg->display_edid->edid_supported = edid_supported;
  if (!edid_supported) return GST_WFD_OK;
  if (edid_blockcount > 0 && edid_blockcount <= EDID_BLOCK_COUNT_MAX_SIZE) {
    msg->display_edid->edid_block_count = edid_blockcount;
    msg->display_edid->edid_payload = g_malloc(EDID_BLOCK_SIZE * edid_blockcount);
    if (msg->display_edid->edid_payload)
      memcpy(msg->display_edid->edid_payload, edid_playload, EDID_BLOCK_SIZE * edid_blockcount);
    else
      msg->display_edid->edid_supported = FALSE;
  } else
    msg->display_edid->edid_supported = FALSE;

  return GST_WFD_OK;
}

GstWFDResult gst_wfd_message_get_display_EDID(GstWFDMessage *msg, gboolean *edid_supported, guint32 *edid_blockcount, gchar **edid_playload)
{
  g_return_val_if_fail(msg != NULL, GST_WFD_EINVAL);
  g_return_val_if_fail(edid_supported != NULL, GST_WFD_EINVAL);
  g_return_val_if_fail(edid_blockcount != NULL, GST_WFD_EINVAL);
  g_return_val_if_fail(edid_playload != NULL, GST_WFD_EINVAL);

  *edid_supported = FALSE;
  if (msg->display_edid) {
    if (msg->display_edid->edid_supported) {
      *edid_blockcount = msg->display_edid->edid_block_count;
      if (msg->display_edid->edid_block_count > 0 && msg->display_edid->edid_block_count <= EDID_BLOCK_COUNT_MAX_SIZE) {
        char *temp;
        temp = g_malloc0(EDID_BLOCK_SIZE * msg->display_edid->edid_block_count);
        if (temp) {
          memcpy(temp, msg->display_edid->edid_payload, EDID_BLOCK_SIZE * msg->display_edid->edid_block_count);
          *edid_playload = temp;
          *edid_supported = TRUE;
        }
      }
    }
  }
  return GST_WFD_OK;
}

GstWFDResult gst_wfd_message_set_coupled_sink(GstWFDMessage *msg, GstWFDCoupledSinkStatus status, gchar *sink_address)
{
  g_return_val_if_fail(msg != NULL, GST_WFD_EINVAL);
  if (!msg->coupled_sink) msg->coupled_sink = g_new0(GstWFDCoupledSink, 1);
  if (status == GST_WFD_SINK_UNKNOWN) return GST_WFD_OK;
  msg->coupled_sink->coupled_sink_cap = g_new0(GstWFDCoupledSinkCap, 1);
  msg->coupled_sink->coupled_sink_cap->status = status;
  msg->coupled_sink->coupled_sink_cap->sink_address = g_strdup(sink_address);
  return GST_WFD_OK;
}

GstWFDResult gst_wfd_message_get_coupled_sink(GstWFDMessage *msg, GstWFDCoupledSinkStatus *status, gchar **sink_address)
{
  g_return_val_if_fail(msg != NULL, GST_WFD_EINVAL);
  if (msg->coupled_sink && msg->coupled_sink->coupled_sink_cap) {
    *status = msg->coupled_sink->coupled_sink_cap->status;
    *sink_address = g_strdup(msg->coupled_sink->coupled_sink_cap->sink_address);
  } else *status = GST_WFD_SINK_UNKNOWN;
  return GST_WFD_OK;
}

GstWFDResult gst_wfd_message_set_trigger_type(GstWFDMessage *msg, GstWFDTrigger trigger)
{
  g_return_val_if_fail(msg != NULL, GST_WFD_EINVAL);

  if (!msg->trigger_method)
    msg->trigger_method = g_new0(GstWFDTriggerMethod, 1);
  if (trigger == GST_WFD_TRIGGER_SETUP)
    msg->trigger_method->wfd_trigger_method = g_strdup(GST_STRING_WFD_SETUP);
  else if (trigger == GST_WFD_TRIGGER_PAUSE)
    msg->trigger_method->wfd_trigger_method = g_strdup(GST_STRING_WFD_PAUSE);
  else if (trigger == GST_WFD_TRIGGER_TEARDOWN)
    msg->trigger_method->wfd_trigger_method = g_strdup(GST_STRING_WFD_TEARDOWN);
  else if (trigger == GST_WFD_TRIGGER_PLAY)
    msg->trigger_method->wfd_trigger_method = g_strdup(GST_STRING_WFD_PLAY);
  else
    return GST_WFD_EINVAL;
  return GST_WFD_OK;
}

GstWFDResult gst_wfd_message_get_trigger_type(GstWFDMessage *msg, GstWFDTrigger *trigger)
{
  g_return_val_if_fail(msg != NULL, GST_WFD_EINVAL);
  if (!g_strcmp0(msg->trigger_method->wfd_trigger_method, GST_STRING_WFD_SETUP))
    *trigger = GST_WFD_TRIGGER_SETUP;
  else if (!g_strcmp0(msg->trigger_method->wfd_trigger_method, GST_STRING_WFD_PAUSE))
    *trigger = GST_WFD_TRIGGER_PAUSE;
  else if (!g_strcmp0(msg->trigger_method->wfd_trigger_method, GST_STRING_WFD_TEARDOWN))
    *trigger = GST_WFD_TRIGGER_TEARDOWN;
  else if (!g_strcmp0(msg->trigger_method->wfd_trigger_method, GST_STRING_WFD_PLAY))
    *trigger = GST_WFD_TRIGGER_PLAY;
  else {
    *trigger = GST_WFD_TRIGGER_UNKNOWN;
    return GST_WFD_EINVAL;
  }
  return GST_WFD_OK;
}

GstWFDResult gst_wfd_message_set_presentation_url(GstWFDMessage *msg, gchar *wfd_url0, gchar *wfd_url1)
{
  g_return_val_if_fail(msg != NULL, GST_WFD_EINVAL);
  if (!msg->presentation_url) msg->presentation_url = g_new0(GstWFDPresentationUrl, 1);
  if (wfd_url0) msg->presentation_url->wfd_url0 = g_strdup(wfd_url0);
  if (wfd_url1) msg->presentation_url->wfd_url1 = g_strdup(wfd_url1);
  return GST_WFD_OK;
}

GstWFDResult gst_wfd_message_get_presentation_url(GstWFDMessage *msg, gchar **wfd_url0, gchar **wfd_url1)
{
  g_return_val_if_fail(msg != NULL, GST_WFD_EINVAL);
  if (msg->presentation_url) {
    *wfd_url0 = g_strdup(msg->presentation_url->wfd_url0);
    *wfd_url1 = g_strdup(msg->presentation_url->wfd_url1);
  }
  return GST_WFD_OK;
}

GstWFDResult gst_wfd_message_set_prefered_RTP_ports(GstWFDMessage *msg, GstWFDRTSPTransMode trans, GstWFDRTSPProfile profile,
                                           GstWFDRTSPLowerTrans lowertrans, guint32 rtp_port0, guint32 rtp_port1)
{
  GString *lines;
  g_return_val_if_fail(msg != NULL, GST_WFD_EINVAL);

  if (!msg->client_rtp_ports)
    msg->client_rtp_ports = g_new0(GstWFDClientRtpPorts, 1);

  if (trans != GST_WFD_RTSP_TRANS_UNKNOWN) {
    lines = g_string_new("");
    if (trans == GST_WFD_RTSP_TRANS_RTP)  g_string_append_printf(lines, GST_STRING_WFD_RTP);
    else if (trans == GST_WFD_RTSP_TRANS_RDT) g_string_append_printf(lines, GST_STRING_WFD_RDT);

    if (profile != GST_WFD_RTSP_PROFILE_UNKNOWN) g_string_append_printf(lines, GST_STRING_WFD_SLASH);

    if (profile == GST_WFD_RTSP_PROFILE_AVP) g_string_append_printf(lines, GST_STRING_WFD_AVP);
    else if (profile == GST_WFD_RTSP_PROFILE_SAVP) g_string_append_printf(lines, GST_STRING_WFD_SAVP);

    if (lowertrans != GST_WFD_RTSP_LOWER_TRANS_UNKNOWN) g_string_append_printf(lines, GST_STRING_WFD_SLASH);

    if (lowertrans == GST_WFD_RTSP_LOWER_TRANS_UDP) {
      g_string_append_printf(lines, GST_STRING_WFD_UDP);
      g_string_append_printf(lines, GST_STRING_WFD_SEMI_COLON);
      g_string_append_printf(lines, GST_STRING_WFD_UNICAST);
    } else if (lowertrans == GST_WFD_RTSP_LOWER_TRANS_UDP_MCAST) {
      g_string_append_printf(lines, GST_STRING_WFD_UDP);
      g_string_append_printf(lines, GST_STRING_WFD_SEMI_COLON);
      g_string_append_printf(lines, GST_STRING_WFD_MULTICAST);
    } else if (lowertrans == GST_WFD_RTSP_LOWER_TRANS_TCP) {
      g_string_append_printf(lines, GST_STRING_WFD_TCP);
      g_string_append_printf(lines, GST_STRING_WFD_SEMI_COLON);
      g_string_append_printf(lines, GST_STRING_WFD_UNICAST);
    } else if (lowertrans == GST_WFD_RTSP_LOWER_TRANS_HTTP) {
      g_string_append_printf(lines, GST_STRING_WFD_TCP_HTTP);
    }

    msg->client_rtp_ports->profile = g_string_free(lines, FALSE);
    msg->client_rtp_ports->rtp_port0 = rtp_port0;
    msg->client_rtp_ports->rtp_port1 = rtp_port1;
    msg->client_rtp_ports->mode = g_strdup("mode=play");
  }
  return GST_WFD_OK;
}

GstWFDResult gst_wfd_message_get_prefered_RTP_ports(GstWFDMessage *msg, GstWFDRTSPTransMode *trans, GstWFDRTSPProfile *profile,
                                           GstWFDRTSPLowerTrans *lowertrans, guint32 *rtp_port0, guint32 *rtp_port1)
{
  g_return_val_if_fail(msg != NULL, GST_WFD_EINVAL);
  g_return_val_if_fail(msg->client_rtp_ports != NULL, GST_WFD_EINVAL);

  if (g_strrstr(msg->client_rtp_ports->profile, GST_STRING_WFD_RTP)) *trans = GST_WFD_RTSP_TRANS_RTP;
  if (g_strrstr(msg->client_rtp_ports->profile, GST_STRING_WFD_RDT)) *trans = GST_WFD_RTSP_TRANS_RDT;
  if (g_strrstr(msg->client_rtp_ports->profile, GST_STRING_WFD_AVP)) *profile = GST_WFD_RTSP_PROFILE_AVP;
  if (g_strrstr(msg->client_rtp_ports->profile, GST_STRING_WFD_SAVP)) *profile = GST_WFD_RTSP_PROFILE_SAVP;
  if (g_strrstr(msg->client_rtp_ports->profile, GST_STRING_WFD_UDP) &&
    g_strrstr(msg->client_rtp_ports->profile, GST_STRING_WFD_UNICAST)) *lowertrans = GST_WFD_RTSP_LOWER_TRANS_UDP;
  if (g_strrstr(msg->client_rtp_ports->profile, GST_STRING_WFD_UDP) &&
    g_strrstr(msg->client_rtp_ports->profile, GST_STRING_WFD_MULTICAST)) *lowertrans = GST_WFD_RTSP_LOWER_TRANS_UDP_MCAST;
  if (g_strrstr(msg->client_rtp_ports->profile, GST_STRING_WFD_TCP) &&
    g_strrstr(msg->client_rtp_ports->profile, GST_STRING_WFD_UNICAST)) *lowertrans = GST_WFD_RTSP_LOWER_TRANS_TCP;
  if (g_strrstr(msg->client_rtp_ports->profile, GST_STRING_WFD_TCP_HTTP)) *lowertrans = GST_WFD_RTSP_LOWER_TRANS_HTTP;

  *rtp_port0 = msg->client_rtp_ports->rtp_port0;
  *rtp_port1 = msg->client_rtp_ports->rtp_port1;

  return GST_WFD_OK;
}

GstWFDResult gst_wfd_message_set_audio_sink_type(GstWFDMessage *msg, GstWFDSinkType sinktype)
{
  g_return_val_if_fail(msg != NULL, GST_WFD_EINVAL);
  if (!msg->route) msg->route = g_new0(GstWFDRoute, 1);
  if (sinktype == GST_WFD_PRIMARY_SINK) msg->route->destination = g_strdup(GST_STRING_WFD_PRIMARY);
  else if (sinktype == GST_WFD_SECONDARY_SINK) msg->route->destination = g_strdup(GST_STRING_WFD_SECONDARY);
  return GST_WFD_OK;
}

GstWFDResult gst_wfd_message_get_audio_sink_type(GstWFDMessage *msg, GstWFDSinkType *sinktype)
{
  g_return_val_if_fail(msg != NULL, GST_WFD_EINVAL);
  if (msg->route) {
    if (!g_strcmp0(msg->route->destination, GST_STRING_WFD_PRIMARY)) *sinktype = GST_WFD_PRIMARY_SINK;
    else if (!g_strcmp0(msg->route->destination, GST_STRING_WFD_SECONDARY)) *sinktype = GST_WFD_SECONDARY_SINK;
  }
  return GST_WFD_OK;
}

GstWFDResult gst_wfd_message_set_I2C_port(GstWFDMessage *msg, gboolean i2csupport, guint32 i2cport)
{
  g_return_val_if_fail(msg != NULL, GST_WFD_EINVAL);
  if (!msg->I2C) msg->I2C = g_new0(GstWFDI2C, 1);
  msg->I2C->I2CPresent = i2csupport;
  msg->I2C->I2C_port = i2cport;
  return GST_WFD_OK;
}

GstWFDResult gst_wfd_message_get_I2C_port(GstWFDMessage *msg, gboolean *i2csupport, guint32 *i2cport)
{
  g_return_val_if_fail(msg != NULL, GST_WFD_EINVAL);
  if (msg->I2C && msg->I2C->I2CPresent) {
    *i2csupport = msg->I2C->I2CPresent;
    *i2cport = msg->I2C->I2C_port;
  } else *i2csupport = FALSE;
  return GST_WFD_OK;
}

GstWFDResult gst_wfd_message_set_av_format_change_timing(GstWFDMessage *msg, guint64 PTS, guint64 DTS)
{
  g_return_val_if_fail(msg != NULL, GST_WFD_EINVAL);
  if (!msg->av_format_change_timing) msg->av_format_change_timing = g_new0(GstWFDAVFormatChangeTiming, 1);
  msg->av_format_change_timing->PTS = PTS;
  msg->av_format_change_timing->DTS = DTS;
  return GST_WFD_OK;
}

GstWFDResult gst_wfd_message_get_av_format_change_timing(GstWFDMessage *msg, guint64 *PTS, guint64 *DTS)
{
  g_return_val_if_fail(msg != NULL, GST_WFD_EINVAL);
  if (msg->av_format_change_timing) {
    *PTS = msg->av_format_change_timing->PTS;
    *DTS = msg->av_format_change_timing->DTS;
  }
  return GST_WFD_OK;
}

#ifdef STANDBY_RESUME_CAPABILITY
GstWFDResult gst_wfd_message_set_standby_resume_capability(GstWFDMessage *msg, gboolean supported)
{
  g_return_val_if_fail(msg != NULL, GST_WFD_EINVAL);
  if (!msg->standby_resume_capability) msg->standby_resume_capability = g_new0(GstWFDStandbyResumeCapability, 1);
  msg->standby_resume_capability->standby_resume_cap = supported;
  return GST_WFD_OK;
}

GstWFDResult gst_wfd_message_get_standby_resume_capability(GstWFDMessage *msg, gboolean *supported)
{
  g_return_val_if_fail(msg != NULL, GST_WFD_EINVAL);
  if (msg->standby_resume_capability) *supported = msg->standby_resume_capability->standby_resume_cap;
  return GST_WFD_OK;
}
#endif
GstWFDResult gst_wfd_message_set_standby(GstWFDMessage *msg, gboolean standby_enable)
{
  g_return_val_if_fail(msg != NULL, GST_WFD_EINVAL);
  if (!msg->standby) msg->standby = g_new0(GstWFDStandby, 1);
  msg->standby->wfd_standby = standby_enable;
  return GST_WFD_OK;
}

GstWFDResult gst_wfd_message_get_standby(GstWFDMessage *msg, gboolean *standby_enable)
{
  g_return_val_if_fail(msg != NULL, GST_WFD_EINVAL);
  if (msg->standby) *standby_enable = msg->standby->wfd_standby;
  return GST_WFD_OK;
}

GstWFDResult gst_wfd_message_set_connector_type(GstWFDMessage *msg, GstWFDConnector connector)
{
  g_return_val_if_fail(msg != NULL, GST_WFD_EINVAL);
  if (!msg->connector_type) msg->connector_type = g_new0(GstWFDConnectorType, 1);
  msg->connector_type->connector_type = connector;
  return GST_WFD_OK;
}

GstWFDResult gst_wfd_message_get_connector_type(GstWFDMessage *msg, GstWFDConnector *connector)
{
  g_return_val_if_fail(msg != NULL, GST_WFD_EINVAL);
  if (msg->connector_type) *connector = msg->connector_type->connector_type;
  return GST_WFD_OK;
}

GstWFDResult gst_wfd_message_set_idr_request(GstWFDMessage *msg)
{
  g_return_val_if_fail(msg != NULL, GST_WFD_EINVAL);
  if (!msg->idr_request) msg->idr_request = g_new0(GstWFDIdrRequest, 1);
  msg->idr_request->idr_request = TRUE;
  return GST_WFD_OK;
}
