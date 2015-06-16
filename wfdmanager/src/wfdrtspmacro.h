/*
 * wfdrtspmacro
 *
 * Copyright (c) 2011 - 2013 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: ByungWook Jang <bw.jang@samsung.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */


#ifndef __WFD_RTSP_MACRO_H__
#define __WFD_RTSP_MACRO_H__

#define WFD_MESSAGE_FEATURES_PATH "/usr/lib/libmmfwfdconfig.so.0"
#define WFD_MESSAGE_NEW "wfdconfig_message_new"
#define WFD_MESSAGE_INIT "wfdconfig_message_init"
#define WFD_MESSAGE_SET_AUDIO_SINK_TYPE "wfdconfig_set_audio_sink_type"
#define WFD_MESSAGE_SET_CONNECTOR_TYPE "wfdconfig_set_connector_type"
#define WFD_MESSAGE_SET_STANDBY "wfdconfig_set_standby"
#define WFD_MESSAGE_SET_IDR_REQUEST "wfdconfig_set_idr_request"
#define WFD_MESSAGE_SET_UIBC_CAPABILITY "wfdconfig_set_uibc_capability"
#define WFD_MESSAGE_SET_UIBC_STATUS "wfdconfig_set_uibc_status"
#define WFD_MESSAGE_FREE "wfdconfig_message_free"
#define WFD_MESSAGE_DUMP "wfdconfig_message_dump"
#define WFD_MESSAGE_AS_TEXT "wfdconfig_message_as_text"
#define WFD_MESSAGE_PARSE_BUFFER "wfdconfig_message_parse_buffer"
#define WFD_MESSAGE_SET_SUPPORTED_AUDIO_FORMAT "wfdconfig_set_supported_audio_format"
#define WFD_MESSAGE_SET_SUPPORTED_VIDEO_FORMAT "wfdconfig_set_supported_video_format"
#define WFD_MESSAGE_SET_PREFERD_RTP_PORT "wfdconfig_set_prefered_RTP_ports"
#define WFD_MESSAGE_GET_PREFERD_RTP_PORT "wfdconfig_get_prefered_RTP_ports"
#define WFD_MESSAGE_SET_CONTPROTECTION_TYPE "wfdconfig_set_contentprotection_type"
#define WFD_MESSAGE_SET_COUPLED_SINK "wfdconfig_set_coupled_sink"
#define WFD_MESSAGE_GET_PRESENTATION_URL "wfdconfig_get_presentation_url"
#define WFD_MESSAGE_GET_STANDBY "wfdconfig_get_standby"
#define WFD_MESSAGE_GET_UIBC_CAPABILITY "wfdconfig_get_uibc_capability"
#define WFD_MESSAGE_GET_UIBC_STATUS "wfdconfig_get_uibc_status"
#define WFD_MESSAGE_GET_PREFERED_AUDIO_FORMAT "wfdconfig_get_prefered_audio_format"
#define WFD_MESSAGE_GET_PREFERED_VIDEO_FORMAT "wfdconfig_get_prefered_video_format"
#define WFD_MESSAGE_GET_TRIGGER_TYPE "wfdconfig_get_trigger_type"
#define WFD_MESSAGE_GET_AV_FORMAT_CHANGE_TIMING "wfdconfig_get_av_format_change_timing"

#define WFDCONFIG_MESSAGE_NEW(msg, label)\
G_STMT_START { \
  if (src->extended_wfd_message_support) {\
    WFDResult (*func)(WFDMessage **) = __wfd_config_message_func(src, WFD_MESSAGE_NEW);\
    if(func == NULL) {\
      wfd_res = WFD_NOT_IMPLEMENTED;\
      goto label;\
    }\
    wfd_res = func(msg);\
    if (G_UNLIKELY (wfd_res != WFD_OK) || G_LIKELY((*(msg)) == NULL)) \
      goto label; \
  }\
} G_STMT_END

#define WFDCONFIG_MESSAGE_INIT(msg, label)\
G_STMT_START { \
  if (src->extended_wfd_message_support) {\
    WFDResult (*func)(WFDMessage *) = __wfd_config_message_func(src, WFD_MESSAGE_INIT);\
    if(func == NULL) {\
      wfd_res = WFD_NOT_IMPLEMENTED;\
      goto label;\
    }\
    wfd_res = func(msg);\
    if (G_UNLIKELY (wfd_res != WFD_OK)) \
      goto label; \
  }\
} G_STMT_END

#define WFDCONFIG_SET_AUDIO_SINK_TYPE(msg, sink_number, label)\
G_STMT_START { \
  if (src->extended_wfd_message_support) {\
    WFDResult (*func)(WFDMessage *, WFDSinkType) = __wfd_config_message_func(src, WFD_MESSAGE_SET_AUDIO_SINK_TYPE);\
    if(func == NULL) {\
      wfd_res = WFD_NOT_IMPLEMENTED;\
      goto label;\
    }\
    wfd_res = func(msg, sink_number);\
    if (G_UNLIKELY (wfd_res != WFD_OK)) \
      goto label; \
  }\
} G_STMT_END

#define WFDCONFIG_SET_CONNECTOR_TYPE(msg, connector, label)\
G_STMT_START { \
  if (src->extended_wfd_message_support){\
    WFDResult (*func)(WFDMessage *, WFDConnector) = __wfd_config_message_func(src, WFD_MESSAGE_SET_CONNECTOR_TYPE);\
    if(func == NULL) {\
      wfd_res = WFD_NOT_IMPLEMENTED;\
      goto label;\
    }\
    wfd_res = func(msg, connector);\
    if (G_UNLIKELY (wfd_res != WFD_OK)) \
      goto label; \
  }\
} G_STMT_END

#define WFDCONFIG_SET_STANDBY(msg, standby_enable, label)\
G_STMT_START { \
  if (src->extended_wfd_message_support){\
    WFDResult (*func)(WFDMessage *, gboolean) = __wfd_config_message_func(src, WFD_MESSAGE_SET_STANDBY);\
    if(func == NULL) {\
      wfd_res = WFD_NOT_IMPLEMENTED;\
      goto label;\
    }\
    wfd_res = func(msg, standby_enable);\
    if (G_UNLIKELY (wfd_res != WFD_OK)) \
      goto label; \
  }\
} G_STMT_END

#define WFDCONFIG_SET_IDR_REQUEST(msg, label)\
G_STMT_START { \
  if (src->extended_wfd_message_support){\
    WFDResult (*func)(WFDMessage *) = __wfd_config_message_func(src, WFD_MESSAGE_SET_IDR_REQUEST);\
    if(func == NULL) {\
      wfd_res = WFD_NOT_IMPLEMENTED;\
      goto label;\
    }\
    wfd_res = func(msg);\
    if (G_UNLIKELY (wfd_res != WFD_OK)) \
      goto label; \
  }\
} G_STMT_END

#define WFDCONFIG_SET_UIBC_CAPABILITY(msg, input_category, inp_type, inp_pair, inp_type_path_count, tcp_port, label)\
G_STMT_START { \
  if (src->extended_wfd_message_support){\
    WFDResult (*func)(WFDMessage *, guint32 , guint32 , WFDHIDCTypePathPair *, guint32 , guint32 ) = __wfd_config_message_func(src, WFD_MESSAGE_SET_UIBC_CAPABILITY);\
    if(func == NULL) {\
      wfd_res = WFD_NOT_IMPLEMENTED;\
      goto label;\
    }\
    wfd_res = func(msg, input_category, inp_type, inp_pair, inp_type_path_count, tcp_port);\
    if (G_UNLIKELY (wfd_res != WFD_OK)) \
      goto label; \
  }\
} G_STMT_END

#define WFDCONFIG_SET_UIBC_STATUS(msg, uibc_enable, label)\
G_STMT_START { \
  if (src->extended_wfd_message_support){\
    WFDResult (*func)(WFDMessage *, gboolean) = __wfd_config_message_func(src, WFD_MESSAGE_SET_UIBC_STATUS);\
    if(func == NULL) {\
      wfd_res = WFD_NOT_IMPLEMENTED;\
      goto label;\
    }\
    wfd_res = func(msg, uibc_enable);\
    if (G_UNLIKELY (wfd_res != WFD_OK)) \
      goto label; \
  }\
} G_STMT_END

#define WFDCONFIG_MESSAGE_FREE(msg)\
G_STMT_START { \
  if (src->extended_wfd_message_support && msg != NULL){\
    WFDResult (*func)(WFDMessage *) = __wfd_config_message_func(src, WFD_MESSAGE_FREE);\
    if(func != NULL) {\
      func(msg);\
      msg = NULL;\
    } else {\
      wfd_res = WFD_NOT_IMPLEMENTED;\
    }\
  }\
} G_STMT_END

#define WFDCONFIG_MESSAGE_DUMP(msg)\
G_STMT_START { \
  if (src->extended_wfd_message_support){\
    WFDResult (*func)(WFDMessage *) = __wfd_config_message_func(src, WFD_MESSAGE_DUMP);\
    if(func != NULL) {\
      func(msg);\
    } else {\
      wfd_res = WFD_NOT_IMPLEMENTED;\
    }\
  }\
} G_STMT_END

#define WFDCONFIG_MESSAGE_AS_TEXT(msg, text, label)\
G_STMT_START { \
  if (src->extended_wfd_message_support){\
    gchar* (*func)(WFDMessage *) = __wfd_config_message_func(src, WFD_MESSAGE_AS_TEXT);\
    if(func == NULL) {\
      wfd_res = WFD_NOT_IMPLEMENTED;\
      goto label;\
    }\
    text = func(msg);\
    if (!text)\
      goto label;\
  }\
} G_STMT_END

#define WFDCONFIG_MESSAGE_PARSE_BUFFER(data, size, msg, label)\
G_STMT_START { \
  if (src->extended_wfd_message_support){\
    WFDResult (*func)(const guint8 * , guint , WFDMessage *) = __wfd_config_message_func(src, WFD_MESSAGE_PARSE_BUFFER);\
    if(func == NULL) {\
      wfd_res = WFD_NOT_IMPLEMENTED;\
      goto label;\
    }\
    wfd_res = func(data, size, msg);\
    if (G_UNLIKELY (wfd_res != WFD_OK)) \
      goto label; \
  }\
} G_STMT_END

#define WFDCONFIG_SET_SUPPORTED_AUDIO_FORMAT(msg, audio_codec, audio_sampling_frequency, audio_channels, audio_bit_width, audio_latency, label)\
G_STMT_START { \
  if (src->extended_wfd_message_support){\
    WFDResult (*func)(WFDMessage *, WFDAudioFormats , guint , guint , guint , guint32 ) = __wfd_config_message_func(src, WFD_MESSAGE_SET_SUPPORTED_AUDIO_FORMAT);\
    if(func == NULL) {\
      wfd_res = WFD_NOT_IMPLEMENTED;\
      goto label;\
    }\
    wfd_res = func(msg, audio_codec, audio_sampling_frequency, audio_channels, audio_bit_width, audio_latency);\
     if (G_UNLIKELY (wfd_res != WFD_OK)) \
      goto label; \
  }\
} G_STMT_END

#define WFDCONFIG_SET_SUPPORTED_VIDEO_FORMAT(msg, video_codec, video_native, video_native_resolution, video_cea_support,\
  video_vesa_support, video_hh_support, video_profile, video_level, video_latency, video_vertical_resolution,\
  video_horizontal_resolution, video_minimum_slicing, video_slice_enc_param, video_framerate_control_support, label)\
G_STMT_START { \
  if (src->extended_wfd_message_support){\
    WFDResult (*func)(WFDMessage *, WFDVideoCodecs ,\
                  WFDVideoNativeResolution , guint64 ,\
                  guint64 , guint64 , guint64 ,\
                  guint , guint , guint32 , guint32 ,\
                  guint32 , guint32 , guint32 , guint ) = __wfd_config_message_func(src, WFD_MESSAGE_SET_SUPPORTED_VIDEO_FORMAT);\
    if(func == NULL) {\
      wfd_res = WFD_NOT_IMPLEMENTED;\
      goto label;\
    }\
    wfd_res = func(msg,\
          video_codec,\
          video_native,\
          video_native_resolution,\
          video_cea_support,\
          video_vesa_support,\
          video_hh_support,\
          video_profile,\
          video_level,\
          video_latency,\
          video_vertical_resolution,\
          video_horizontal_resolution,\
          video_minimum_slicing,\
          video_slice_enc_param,\
          video_framerate_control_support);\
    if (G_UNLIKELY (wfd_res != WFD_OK)) \
      goto label; \
  }\
} G_STMT_END

#define WFDCONFIG_SET_PREFERD_RTP_PORT(msg, trans, profile, lowertrans, rtp_port0, rtp_port1, label)\
G_STMT_START { \
  if (src->extended_wfd_message_support){\
    WFDResult (*func)(WFDMessage *, WFDRTSPTransMode , WFDRTSPProfile , WFDRTSPLowerTrans , guint32 , guint32 ) = __wfd_config_message_func(src, WFD_MESSAGE_SET_PREFERD_RTP_PORT);\
    if(func == NULL) {\
      wfd_res = WFD_NOT_IMPLEMENTED;\
      goto label;\
    }\
    wfd_res = func(msg, trans, profile, lowertrans, rtp_port0, rtp_port1);\
    if (G_UNLIKELY (wfd_res != WFD_OK)) \
      goto label; \
  }\
} G_STMT_END

#define WFDCONFIG_GET_PREFERD_RTP_PORT(msg, trans, profile, lowertrans, rtp_port0, rtp_port1, label)\
G_STMT_START { \
  if (src->extended_wfd_message_support){\
    WFDResult (*func)(WFDMessage *, WFDRTSPTransMode *, WFDRTSPProfile  *, WFDRTSPLowerTrans  *, guint32  *, guint32  *) = __wfd_config_message_func(src, WFD_MESSAGE_GET_PREFERD_RTP_PORT);\
    if(func == NULL) {\
      wfd_res = WFD_NOT_IMPLEMENTED;\
      goto label;\
    }\
    wfd_res = func(msg, trans, profile, lowertrans, rtp_port0, rtp_port1);\
    if (G_UNLIKELY (wfd_res != WFD_OK)) \
      goto label; \
  }\
} G_STMT_END


#define WFDCONFIG_SET_CONTENT_PROTECTION_TYPE(msg, hdcp_version, hdcp_port_no, label)\
G_STMT_START { \
  if (src->extended_wfd_message_support){\
    WFDResult (*func)(WFDMessage *, WFDHDCPProtection , guint32) = __wfd_config_message_func(src, WFD_MESSAGE_SET_CONTPROTECTION_TYPE);\
    if(func == NULL) {\
      wfd_res = WFD_NOT_IMPLEMENTED;\
      goto label;\
    }\
    wfd_res = func(msg, hdcp_version, (guint32)hdcp_port_no);\
    if (G_UNLIKELY (wfd_res != WFD_OK)) \
      goto label; \
  }\
} G_STMT_END

#define WFDCONFIG_SET_COUPLED_SINK(msg, status, sink_address, label)\
G_STMT_START { \
  if (src->extended_wfd_message_support){\
    WFDResult (*func)(WFDMessage *, WFDCoupledSinkStatus , gchar *) = __wfd_config_message_func(src, WFD_MESSAGE_SET_COUPLED_SINK);\
    if(func == NULL) {\
      wfd_res = WFD_NOT_IMPLEMENTED;\
      goto label;\
    }\
    wfd_res = func(msg, status,(gchar *)sink_address);\
    if (G_UNLIKELY (wfd_res != WFD_OK)) \
      goto label; \
  }\
} G_STMT_END

#define WFDCONFIG_GET_PRESENTATION_URL(msg, wfd_url0, wfd_url1, label)\
G_STMT_START { \
  if (src->extended_wfd_message_support){\
    WFDResult (*func)(WFDMessage *, gchar **, gchar **) = __wfd_config_message_func(src, WFD_MESSAGE_GET_PRESENTATION_URL);\
    if(func == NULL) {\
      wfd_res = WFD_NOT_IMPLEMENTED;\
      goto label;\
    }\
    wfd_res = func(msg, wfd_url0, wfd_url1);\
    if (G_UNLIKELY (wfd_res != WFD_OK)) \
      goto label; \
  }\
} G_STMT_END

#define WFDCONFIG_GET_STANDBY(msg, standby_enable, label)\
G_STMT_START { \
  if (src->extended_wfd_message_support){\
    WFDResult (*func)(WFDMessage *, gboolean *) = __wfd_config_message_func(src, WFD_MESSAGE_GET_STANDBY);\
    if(func == NULL) {\
      wfd_res = WFD_NOT_IMPLEMENTED;\
      goto label;\
    }\
    wfd_res = func(msg, standby_enable);\
    if (G_UNLIKELY (wfd_res != WFD_OK)) \
      goto label; \
  }\
} G_STMT_END

#define WFDCONFIG_GET_UIBC_CAPABILITY(msg, input_category, inp_type, inp_pair, inp_type_path_count, tcp_port, label)\
G_STMT_START { \
  if (src->extended_wfd_message_support){\
    WFDResult (*func)(WFDMessage *, guint32 *, guint32 *, WFDHIDCTypePathPair **,guint32 *, guint32 *) = __wfd_config_message_func(src, WFD_MESSAGE_GET_UIBC_CAPABILITY);\
    if(func == NULL) {\
      wfd_res = WFD_NOT_IMPLEMENTED;\
      goto label;\
    }\
    wfd_res = func(msg, input_category, inp_type, inp_pair, inp_type_path_count, tcp_port);\
    if (G_UNLIKELY (wfd_res != WFD_OK)) \
      goto label; \
  }\
} G_STMT_END

#define WFDCONFIG_GET_UIBC_STATUS(msg, uibc_enable, label)\
G_STMT_START { \
  if (src->extended_wfd_message_support){\
    WFDResult (*func)(WFDMessage *, gboolean *) = __wfd_config_message_func(src, WFD_MESSAGE_GET_UIBC_STATUS);\
    if(func == NULL) {\
      wfd_res = WFD_NOT_IMPLEMENTED;\
      goto label;\
    }\
    wfd_res = func(msg, uibc_enable);\
    if (G_UNLIKELY (wfd_res != WFD_OK)) \
      goto label; \
  }\
} G_STMT_END

#define WFDCONFIG_GET_PREFERED_AUDIO_FORMAT(msg, audio_format, audio_frequency, audio_channels, audio_bitwidth, audio_latency)\
G_STMT_START { \
  if (src->extended_wfd_message_support){\
    WFDResult (*func)(WFDMessage *, WFDAudioFormats *, WFDAudioFreq *, WFDAudioChannels *, guint *, guint32 *) = __wfd_config_message_func(src, WFD_MESSAGE_GET_PREFERED_AUDIO_FORMAT);\
    if(func != NULL) {\
      wfd_res = func(msg, audio_format, audio_frequency, audio_channels, audio_bitwidth, audio_latency);\
    } else {\
      wfd_res = WFD_NOT_IMPLEMENTED;\
    }\
  }\
} G_STMT_END

#define WFDCONFIG_GET_PREFERED_VIDEO_FORMAT(msg, cvCodec, cNative, cNativeResolution,\
cCEAResolution, cVESAResolution, cHHResolution, cProfile, cLevel, cvLatency, cMaxHeight,\
cMaxWidth, cmin_slice_size, cslice_enc_params, cframe_rate_control)\
G_STMT_START { \
  if (src->extended_wfd_message_support){\
    WFDResult (*func)(WFDMessage *, WFDVideoCodecs *,\
      WFDVideoNativeResolution *, guint64 *,\
      WFDVideoCEAResolution *, WFDVideoVESAResolution *,\
      WFDVideoHHResolution *,	WFDVideoH264Profile *,\
      WFDVideoH264Level *, guint32 *, guint32 *,\
      guint32 *, guint32 *, guint32 *, guint *) = __wfd_config_message_func(src, WFD_MESSAGE_GET_PREFERED_VIDEO_FORMAT);\
    if(func != NULL) {\
      wfd_res = func(msg, cvCodec, cNative, cNativeResolution,\
        cCEAResolution, cVESAResolution, cHHResolution,\
        cProfile, cLevel, cvLatency, cMaxHeight,\
        cMaxWidth, cmin_slice_size, cslice_enc_params, cframe_rate_control);\
    } else {\
      wfd_res = WFD_NOT_IMPLEMENTED;\
    }\
  }\
} G_STMT_END

#define WFDCONFIG_GET_TRIGGER_TYPE(msg, trigger, label)\
G_STMT_START { \
  if (src->extended_wfd_message_support){\
    WFDResult (*func)(WFDMessage *, WFDTrigger *) = __wfd_config_message_func(src, WFD_MESSAGE_GET_TRIGGER_TYPE);\
    if(func == NULL) {\
      wfd_res = WFD_NOT_IMPLEMENTED;\
      goto label;\
    }\
    wfd_res = func(msg, trigger);\
    if (G_UNLIKELY (wfd_res != WFD_OK)) \
      goto label; \
  }\
} G_STMT_END

#define WFDCONFIG_GET_AV_FORMAT_CHANGE_TIMING(msg, PTS, DTS, label)\
G_STMT_START { \
  if (src->extended_wfd_message_support){\
    WFDResult (*func)(WFDMessage *, guint64 *, guint64 *) = __wfd_config_message_func(src, WFD_MESSAGE_GET_AV_FORMAT_CHANGE_TIMING);\
    if(func == NULL) {\
      wfd_res = WFD_NOT_IMPLEMENTED;\
      goto label;\
    }\
    wfd_res = func(msg, PTS, DTS);\
    if (G_UNLIKELY (wfd_res != WFD_OK)) \
      goto label; \
  }\
} G_STMT_END

#define WFDCONFIG_MESSAGE_CHECK(stmt, label)  \
G_STMT_START { \
  if (G_UNLIKELY ((wfd_res = (stmt)) != WFD_OK)) \
    goto label; \
} G_STMT_END


#endif /*__WFD_RTSP_MACRO_H__*/
