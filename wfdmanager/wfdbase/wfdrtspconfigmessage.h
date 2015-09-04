/*
 * wfdconfig messages
 *
 * Copyright (c) 2011 - 2013 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: JongHyuk Choi <jhchoi.choi@samsung.com>, ByungWook Jang <bw.jang@samsung.com>,
 * Manoj Kumar K <manojkumar.k@samsung.com>, Abhishek Bajaj <abhi.bajaj@samsung.com>, Nikhilesh Mittal <nikhilesh.m@samsung.com>
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

#ifndef __GST_WFD_CONFIG_MESSAGE_H__
#define __GST_WFD_CONFIG_MESSAGE_H__

#include <glib.h>

G_BEGIN_DECLS

#define STRING_WFD_AUDIO_CODECS               "wfd_audio_codecs"
#define STRING_WFD_VIDEO_FORMATS              "wfd_video_formats"
#define STRING_WFD_3D_VIDEO_FORMATS           "wfd_3d_video_formats"
#define STRING_WFD_CONTENT_PROTECTION         "wfd_content_protection"
#define STRING_WFD_DISPLAY_EDID               "wfd_display_edid"
#define STRING_WFD_COUPLED_SINK               "wfd_coupled_sink"
#define STRING_WFD_TRIGGER_METHOD             "wfd_trigger_method"
#define STRING_WFD_PRESENTATION_URL           "wfd_presentation_URL"
#define STRING_WFD_CLIENT_RTP_PORTS           "wfd_client_rtp_ports"
#define STRING_WFD_ROUTE                      "wfd_route"
#define STRING_WFD_I2C                        "wfd_I2C"
#define STRING_WFD_AV_FORMAT_CHANGE_TIMING    "wfd_av_format_change_timing"
#define STRING_WFD_PREFERRED_DISPLAY_MODE     "wfd_preferred_display_mode"
#define STRING_WFD_STANDBY_RESUME_CAPABILITY  "wfd_standby_resume_capability"
#define STRING_WFD_STANDBY                    "wfd_standby"
#define STRING_WFD_CONNECTOR_TYPE             "wfd_connector_type"
#define STRING_WFD_IDR_REQUEST                "wfd_idr_request"
#define STRING_WFD_NONE                       "none"
#define STRING_WFD_ENABLE                     "enable"
#define STRING_WFD_DISABLE                    "disable"
#define STRING_WFD_CRLF                       "\r\n"
#define STRING_WFD_SPACE                      " "
#define STRING_WFD_INPUT_CATEGORY_LIST        "input_category_list"
#define STRING_WFD_GENERIC                    "GENERIC"
#define STRING_WFD_HIDC                       "HIDC"
#define STRING_WFD_GENERIC_CAP_LIST           "generic_cap_list"
#define STRING_WFD_KEYBOARD                   "Keyboard"
#define STRING_WFD_MOUSE                      "Mouse"
#define STRING_WFD_SINGLE_TOUCH               "SingleTouch"
#define STRING_WFD_MULTI_TOUCH                "MultiTouch"
#define STRING_WFD_JOYSTICK                   "Joystick"
#define STRING_WFD_CAMERA                     "Camera"
#define STRING_WFD_GESTURE                    "Gesture"
#define STRING_WFD_REMOTE_CONTROL             "RemoteControl"
#define STRING_WFD_HIDC_CAP_LIST              "hidc_cap_list"
#define STRING_WFD_INFRARED                   "Infrared"
#define STRING_WFD_USB                        "USB"
#define STRING_WFD_BT                         "BT"
#define STRING_WFD_ZIGBEE                     "Zigbee"
#define STRING_WFD_WIFI                       "Wi-Fi"
#define STRING_WFD_NO_SP                      "No-SP"
#define STRING_WFD_PORT                       "port"
#define STRING_WFD_SUPPORTED                  "supported"
#define STRING_WFD_LPCM                       "LPCM"
#define STRING_WFD_AAC                        "AAC"
#define STRING_WFD_AC3                        "AC3"
#define STRING_WFD_HDCP2_0                    "HDCP2.0"
#define STRING_WFD_HDCP2_1                    "HDCP2.1"
#define STRING_WFD_SETUP                      "SETUP"
#define STRING_WFD_PAUSE                      "PAUSE"
#define STRING_WFD_TEARDOWN                   "TEARDOWN"
#define STRING_WFD_PLAY                       "PLAY"
#define STRING_WFD_RTP                        "RTP"
#define STRING_WFD_RDT                        "RDT"
#define STRING_WFD_AVP                        "AVP"
#define STRING_WFD_SAVP                       "SAVP"
#define STRING_WFD_UDP                        "UDP"
#define STRING_WFD_TCP                        "TCP"
#define STRING_WFD_UNICAST                    "unicast"
#define STRING_WFD_MULTICAST                  "multicast"
#define STRING_WFD_TCP_HTTP                   "HTTP"
#define STRING_WFD_PRIMARY                    "primary"
#define STRING_WFD_SECONDARY                  "secondary"
#define STRING_WFD_COMMA                      ","
#define STRING_WFD_EQUALS                     "="
#define STRING_WFD_COLON                      ":"
#define STRING_WFD_SEMI_COLON                 ";"
#define STRING_WFD_SLASH                      "/"

/**
 * WFDResult:
 * @WFD_OK: A successful return value
 * @WFD_EINVAL: a function was given invalid parameters
 *
 * Return values for the WFD_CONFIG functions.
 */
typedef enum {
  WFD_OK     = 0,
  WFD_EINVAL = -1,
  WFD_NOT_IMPLEMENTED = -2,
  WFD_NOT_SUPPORTED = -3
} WFDResult;

typedef enum {
  WFD_AUDIO_UNKNOWN 	= 0,
  WFD_AUDIO_LPCM		= (1 << 0),
  WFD_AUDIO_AAC		= (1 << 1),
  WFD_AUDIO_AC3		= (1 << 2)
}WFDAudioFormats;

typedef enum {
  WFD_FREQ_UNKNOWN = 0,
  WFD_FREQ_44100 	 = (1 << 0),
  WFD_FREQ_48000	 = (1 << 1)
}WFDAudioFreq;

typedef enum {
  WFD_CHANNEL_UNKNOWN = 0,
  WFD_CHANNEL_2 	 	= (1 << 0),
  WFD_CHANNEL_4		= (1 << 1),
  WFD_CHANNEL_6		= (1 << 2),
  WFD_CHANNEL_8		= (1 << 3)
}WFDAudioChannels;


typedef enum {
  WFD_VIDEO_UNKNOWN = 0,
  WFD_VIDEO_H264	  = (1 << 0)
}WFDVideoCodecs;

typedef enum {
  WFD_VIDEO_CEA_RESOLUTION = 0,
  WFD_VIDEO_VESA_RESOLUTION,
  WFD_VIDEO_HH_RESOLUTION
}WFDVideoNativeResolution;

typedef enum {
  WFD_CEA_UNKNOWN		= 0,
  WFD_CEA_640x480P60 	= (1 << 0),
  WFD_CEA_720x480P60	= (1 << 1),
  WFD_CEA_720x480I60	= (1 << 2),
  WFD_CEA_720x576P50	= (1 << 3),
  WFD_CEA_720x576I50	= (1 << 4),
  WFD_CEA_1280x720P30	= (1 << 5),
  WFD_CEA_1280x720P60	= (1 << 6),
  WFD_CEA_1920x1080P30= (1 << 7),
  WFD_CEA_1920x1080P60= (1 << 8),
  WFD_CEA_1920x1080I60= (1 << 9),
  WFD_CEA_1280x720P25	= (1 << 10),
  WFD_CEA_1280x720P50	= (1 << 11),
  WFD_CEA_1920x1080P25= (1 << 12),
  WFD_CEA_1920x1080P50= (1 << 13),
  WFD_CEA_1920x1080I50= (1 << 14),
  WFD_CEA_1280x720P24	= (1 << 15),
  WFD_CEA_1920x1080P24= (1 << 16)
}WFDVideoCEAResolution;

typedef enum {
  WFD_VESA_UNKNOWN		= 0,
  WFD_VESA_800x600P30 	= (1 << 0),
  WFD_VESA_800x600P60		= (1 << 1),
  WFD_VESA_1024x768P30	= (1 << 2),
  WFD_VESA_1024x768P60	= (1 << 3),
  WFD_VESA_1152x864P30	= (1 << 4),
  WFD_VESA_1152x864P60	= (1 << 5),
  WFD_VESA_1280x768P30	= (1 << 6),
  WFD_VESA_1280x768P60	= (1 << 7),
  WFD_VESA_1280x800P30	= (1 << 8),
  WFD_VESA_1280x800P60	= (1 << 9),
  WFD_VESA_1360x768P30	= (1 << 10),
  WFD_VESA_1360x768P60	= (1 << 11),
  WFD_VESA_1366x768P30	= (1 << 12),
  WFD_VESA_1366x768P60	= (1 << 13),
  WFD_VESA_1280x1024P30	= (1 << 14),
  WFD_VESA_1280x1024P60	= (1 << 15),
  WFD_VESA_1400x1050P30	= (1 << 16),
  WFD_VESA_1400x1050P60	= (1 << 17),
  WFD_VESA_1440x900P30	= (1 << 18),
  WFD_VESA_1440x900P60	= (1 << 19),
  WFD_VESA_1600x900P30	= (1 << 20),
  WFD_VESA_1600x900P60	= (1 << 21),
  WFD_VESA_1600x1200P30	= (1 << 22),
  WFD_VESA_1600x1200P60	= (1 << 23),
  WFD_VESA_1680x1024P30	= (1 << 24),
  WFD_VESA_1680x1024P60	= (1 << 25),
  WFD_VESA_1680x1050P30	= (1 << 26),
  WFD_VESA_1680x1050P60	= (1 << 27),
  WFD_VESA_1920x1200P30	= (1 << 28),
  WFD_VESA_1920x1200P60	= (1 << 29)
}WFDVideoVESAResolution;

typedef enum {
  WFD_HH_UNKNOWN		= 0,
  WFD_HH_800x480P30 	= (1 << 0),
  WFD_HH_800x480P60	= (1 << 1),
  WFD_HH_854x480P30	= (1 << 2),
  WFD_HH_854x480P60	= (1 << 3),
  WFD_HH_864x480P30	= (1 << 4),
  WFD_HH_864x480P60	= (1 << 5),
  WFD_HH_640x360P30	= (1 << 6),
  WFD_HH_640x360P60	= (1 << 7),
  WFD_HH_960x540P30	= (1 << 8),
  WFD_HH_960x540P60	= (1 << 9),
  WFD_HH_848x480P30	= (1 << 10),
  WFD_HH_848x480P60	= (1 << 11)
}WFDVideoHHResolution;

typedef enum {
  WFD_H264_UNKNOWN_PROFILE= 0,
  WFD_H264_BASE_PROFILE	= (1 << 0),
  WFD_H264_HIGH_PROFILE	= (1 << 1)
}WFDVideoH264Profile;

typedef enum {
  WFD_H264_LEVEL_UNKNOWN = 0,
  WFD_H264_LEVEL_3_1   = (1 << 0),
  WFD_H264_LEVEL_3_2   = (1 << 1),
  WFD_H264_LEVEL_4       = (1 << 2),
  WFD_H264_LEVEL_4_1   = (1 << 3),
  WFD_H264_LEVEL_4_2   = (1 << 4)
}WFDVideoH264Level;

typedef enum {
  WFD_HDCP_NONE	= 0,
  WFD_HDCP_2_0	= (1 << 0),
  WFD_HDCP_2_1	= (1 << 1)
}WFDHDCPProtection;

typedef enum {
  WFD_SINK_UNKNOWN = -1,
  WFD_SINK_NOT_COUPLED	= 0,
  WFD_SINK_COUPLED,
  WFD_SINK_TEARDOWN_COUPLING,
  WFD_SINK_RESERVED
}WFDCoupledSinkStatus;

typedef enum {
  WFD_TRIGGER_UNKNOWN = 0,
  WFD_TRIGGER_SETUP,
  WFD_TRIGGER_PAUSE,
  WFD_TRIGGER_TEARDOWN,
  WFD_TRIGGER_PLAY
}WFDTrigger;

typedef enum {
  WFD_RTSP_TRANS_UNKNOWN =  0,
  WFD_RTSP_TRANS_RTP     = (1 << 0),
  WFD_RTSP_TRANS_RDT     = (1 << 1)
} WFDRTSPTransMode;

typedef enum {
  WFD_RTSP_PROFILE_UNKNOWN =  0,
  WFD_RTSP_PROFILE_AVP     = (1 << 0),
  WFD_RTSP_PROFILE_SAVP    = (1 << 1)
} WFDRTSPProfile;

typedef enum {
  WFD_RTSP_LOWER_TRANS_UNKNOWN   = 0,
  WFD_RTSP_LOWER_TRANS_UDP       = (1 << 0),
  WFD_RTSP_LOWER_TRANS_UDP_MCAST = (1 << 1),
  WFD_RTSP_LOWER_TRANS_TCP       = (1 << 2),
  WFD_RTSP_LOWER_TRANS_HTTP      = (1 << 3)
} WFDRTSPLowerTrans;

typedef enum {
  WFD_PRIMARY_SINK   = 0,
  WFD_SECONDARY_SINK
}WFDSinkType;

typedef enum {
  WFD_CONNECTOR_VGA           = 0,
  WFD_CONNECTOR_S,
  WFD_CONNECTOR_COMPOSITE,
  WFD_CONNECTOR_COMPONENT,
  WFD_CONNECTOR_DVI,
  WFD_CONNECTOR_HDMI,
  WFD_CONNECTOR_LVDS,
  WFD_CONNECTOR_RESERVED_7,
  WFD_CONNECTOR_JAPANESE_D,
  WFD_CONNECTOR_SDI,
  WFD_CONNECTOR_DP,
  WFD_CONNECTOR_RESERVED_11,
  WFD_CONNECTOR_UDI,
  WFD_CONNECTOR_NO           = 254,
  WFD_CONNECTOR_PHYSICAL     = 255
}WFDConnector;

typedef enum {
  WFD_PREFERRED_DISPLAY_MODE_NOT_SUPPORTED = 0,
  WFD_PREFERRED_DISPLAY_MODE_SUPPORTED = 1
} WFDPreferredDisplayModeEnum;

typedef struct {
  gchar	*audio_format;
  guint32 modes;
  guint latency;
} WFDAudioCodec;

typedef struct {
  guint	count;
  WFDAudioCodec *list;
} WFDAudioCodeclist;


typedef struct {
  guint CEA_Support;
  guint VESA_Support;
  guint HH_Support;
  guint latency;
  guint min_slice_size;
  guint slice_enc_params;
  guint frame_rate_control_support;
} WFDVideoH264MiscParams;

typedef struct {
  guint profile;
  guint level;
  guint max_hres;
  guint max_vres;
  WFDVideoH264MiscParams misc_params;
} WFDVideoH264Codec;

typedef struct {
  guint	native;
  guint preferred_display_mode_supported;
  WFDVideoH264Codec H264_codec;
} WFDVideoCodec;

typedef struct {
  guint			count;
  WFDVideoCodec *list;
} WFDVideoCodeclist;

typedef struct {
  guint64 video_3d_capability;
  guint latency;
  guint min_slice_size;
  guint slice_enc_params;
  guint frame_rate_control_support;
} WFD3DVideoH264MiscParams;

typedef struct {
  guint profile;
  guint level;
  WFD3DVideoH264MiscParams misc_params;
  guint max_hres;
  guint max_vres;
} WFD3DVideoH264Codec;

typedef struct {
  guint native;
  guint preferred_display_mode_supported;
  WFD3DVideoH264Codec H264_codec;
} WFD3dCapList;

typedef struct {
  guint			count;
  WFD3dCapList *list;
} WFD3DFormats;

typedef struct {
  gchar *hdcpversion;
  gchar *TCPPort;
} WFDHdcp2Spec;

typedef struct {
  WFDHdcp2Spec *hdcp2_spec;
} WFDContentProtection;

typedef struct {
  guint edid_supported;
  guint edid_block_count;
  gchar *edid_payload;
} WFDDisplayEdid;


typedef struct {
  guint status;
  gchar *sink_address;
} WFDCoupled_sink_cap;

typedef struct {
	WFDCoupled_sink_cap *coupled_sink_cap;
} WFDCoupledSink;

typedef struct {
  gchar *wfd_trigger_method;
} WFDTriggerMethod;

typedef struct {
  gchar *wfd_url0;
  gchar *wfd_url1;
} WFDPresentationUrl;

typedef struct {
  gchar *profile;
  guint32 rtp_port0;
  guint32 rtp_port1;
  gchar *mode;
} WFDClientRtpPorts;

typedef struct {
 gchar *destination;
} WFDRoute;

typedef struct {
  gboolean I2CPresent;
  guint32 I2C_port;
} WFDI2C;

typedef struct {
  guint64 PTS;
  guint64 DTS;
} WFDAVFormatChangeTiming;

typedef struct {
  gboolean displaymodesupported;
  guint64 p_clock;
  guint32 H;
  guint32 HB;
  guint32 HSPOL_HSOFF;
  guint32 HSW;
  guint32 V;
  guint32 VB;
  guint32 VSPOL_VSOFF;
  guint32 VSW;
  guint VBS3D;
  guint R;
  guint V2d_s3d_modes;
  guint P_depth;
  WFDVideoH264Codec H264_codec;
} WFDPreferredDisplayMode;

typedef struct {
  gboolean standby_resume_cap;
} WFDStandbyResumeCapability;

typedef struct {
  gboolean wfd_standby;
} WFDStandby;

typedef struct {
  gboolean supported;
  gint32 connector_type;
} WFDConnectorType;

typedef struct {
  gboolean idr_request;
} WFDIdrRequest;

/***********************************************************/

typedef struct {

  WFDAudioCodeclist *audio_codecs;
  WFDVideoCodeclist *video_formats;
  WFD3DFormats *video_3d_formats;
  WFDContentProtection *content_protection;
  WFDDisplayEdid *display_edid;
  WFDCoupledSink *coupled_sink;
  WFDTriggerMethod *trigger_method;
  WFDPresentationUrl *presentation_url;
  WFDClientRtpPorts *client_rtp_ports;
  WFDRoute *route;
  WFDI2C *I2C;
  WFDAVFormatChangeTiming *av_format_change_timing;
  WFDPreferredDisplayMode *preferred_display_mode;
  WFDStandbyResumeCapability *standby_resume_capability;
  WFDStandby *standby;
  WFDConnectorType *connector_type;
  WFDIdrRequest *idr_request;
} WFDMessage;

/* Session descriptions */
WFDResult wfdconfig_message_new                 (WFDMessage **msg);
WFDResult wfdconfig_message_init                (WFDMessage *msg);
WFDResult wfdconfig_message_uninit              (WFDMessage *msg);
WFDResult wfdconfig_message_free                (WFDMessage *msg);
WFDResult wfdconfig_message_parse_buffer        (const guint8 *data, guint size, WFDMessage *msg);
gchar* wfdconfig_message_as_text                (const WFDMessage *msg);
gchar* wfdconfig_parameter_names_as_text        (const WFDMessage *msg);
WFDResult wfdconfig_message_dump                (const WFDMessage *msg);


WFDResult wfdconfig_set_supported_audio_format(WFDMessage *msg,
												guint aCodec, guint aFreq, guint aChanels,
												guint aBitwidth, guint32 aLatency);
WFDResult wfdconfig_set_prefered_audio_format(WFDMessage *msg,
												WFDAudioFormats aCodec, WFDAudioFreq aFreq, WFDAudioChannels aChanels,
												guint aBitwidth, guint32 aLatency);
WFDResult wfdconfig_get_supported_audio_format(WFDMessage *msg,
												guint *aCodec, guint *aFreq, guint *aChanels,
												guint *aBitwidth, guint32 *aLatency);
WFDResult wfdconfig_get_prefered_audio_format(WFDMessage *msg,
												WFDAudioFormats *aCodec, WFDAudioFreq *aFreq, WFDAudioChannels *aChanels,
												guint *aBitwidth, guint32 *aLatency);

WFDResult wfdconfig_set_supported_video_format(WFDMessage *msg, WFDVideoCodecs vCodec,
												WFDVideoNativeResolution vNative, guint64 vNativeResolution,
												guint64 vCEAResolution, guint64 vVESAResolution, guint64 vHHResolution,
												guint vProfile, guint vLevel, guint32 vLatency, guint32 vMaxHeight,
												guint32 vMaxWidth, guint32 min_slice_size, guint32 slice_enc_params, guint frame_rate_control,
												guint preferred_display_mode);
WFDResult wfdconfig_set_prefered_video_format(WFDMessage *msg, WFDVideoCodecs vCodec,
												WFDVideoNativeResolution vNative, guint64 vNativeResolution,
												WFDVideoCEAResolution vCEAResolution, WFDVideoVESAResolution vVESAResolution,
												WFDVideoHHResolution vHHResolution,	WFDVideoH264Profile vProfile,
												WFDVideoH264Level vLevel, guint32 vLatency, guint32 vMaxHeight,
												guint32 vMaxWidth, guint32 min_slice_size, guint32 slice_enc_params, guint frame_rate_control);
WFDResult wfdconfig_get_supported_video_format(WFDMessage *msg, WFDVideoCodecs *vCodec,
												WFDVideoNativeResolution *vNative, guint64 *vNativeResolution,
												guint64 *vCEAResolution, guint64 *vVESAResolution, guint64 *vHHResolution,
												guint *vProfile, guint *vLevel, guint32 *vLatency, guint32 *vMaxHeight,
												guint32 *vMaxWidth, guint32 *min_slice_size, guint32 *slice_enc_params, guint *frame_rate_control);
WFDResult wfdconfig_get_prefered_video_format(WFDMessage *msg, WFDVideoCodecs *vCodec,
												WFDVideoNativeResolution *vNative, guint64 *vNativeResolution,
												WFDVideoCEAResolution *vCEAResolution, WFDVideoVESAResolution *vVESAResolution,
												WFDVideoHHResolution *vHHResolution,	WFDVideoH264Profile *vProfile,
												WFDVideoH264Level *vLevel, guint32 *vLatency, guint32 *vMaxHeight,
												guint32 *vMaxWidth, guint32 *min_slice_size, guint32 *slice_enc_params, guint *frame_rate_control);

// Todo wfd-3d-formats

WFDResult wfdconfig_set_contentprotection_type(WFDMessage *msg, WFDHDCPProtection hdcpversion, guint32 TCPPort);
WFDResult wfdconfig_get_contentprotection_type(WFDMessage *msg, WFDHDCPProtection *hdcpversion, guint32 *TCPPort);

WFDResult wfdconfig_set_display_EDID(WFDMessage *msg, gboolean edid_supported, guint32 edid_blockcount, gchar *edid_playload);
WFDResult wfdconfig_get_display_EDID(WFDMessage *msg, gboolean *edid_supported, guint32 *edid_blockcount, gchar **edid_playload);


WFDResult wfdconfig_set_coupled_sink(WFDMessage *msg, WFDCoupledSinkStatus status, gchar *sink_address);
WFDResult wfdconfig_get_coupled_sink(WFDMessage *msg, WFDCoupledSinkStatus *status, gchar **sink_address);

WFDResult wfdconfig_set_trigger_type(WFDMessage *msg, WFDTrigger trigger);
WFDResult wfdconfig_get_trigger_type(WFDMessage *msg, WFDTrigger *trigger);

WFDResult wfdconfig_set_presentation_url(WFDMessage *msg, gchar *wfd_url0, gchar *wfd_url1);
WFDResult wfdconfig_get_presentation_url(WFDMessage *msg, gchar **wfd_url0, gchar **wfd_url1);

WFDResult wfdconfig_set_prefered_RTP_ports(WFDMessage *msg, WFDRTSPTransMode trans, WFDRTSPProfile profile,
												WFDRTSPLowerTrans lowertrans, guint32 rtp_port0, guint32 rtp_port1);
WFDResult wfdconfig_get_prefered_RTP_ports(WFDMessage *msg, WFDRTSPTransMode *trans, WFDRTSPProfile *profile,
												WFDRTSPLowerTrans *lowertrans, guint32 *rtp_port0, guint32 *rtp_port1);

WFDResult wfdconfig_set_audio_sink_type(WFDMessage *msg, WFDSinkType sinktype);
WFDResult wfdconfig_get_audio_sink_type(WFDMessage *msg, WFDSinkType *sinktype);

WFDResult wfdconfig_set_I2C_port(WFDMessage *msg, gboolean i2csupport, guint32 i2cport);
WFDResult wfdconfig_get_I2C_port(WFDMessage *msg, gboolean *i2csupport, guint32 *i2cport);

WFDResult wfdconfig_set_av_format_change_timing(WFDMessage *msg, guint64 PTS, guint64 DTS);
WFDResult wfdconfig_get_av_format_change_timing(WFDMessage *msg, guint64 *PTS, guint64 *DTS);

// Todo wfd-preferred-display-mode

WFDResult wfdconfig_set_standby_resume_capability(WFDMessage *msg, gboolean supported);
WFDResult wfdconfig_get_standby_resume_capability(WFDMessage *msg, gboolean *supported);

WFDResult wfdconfig_set_standby(WFDMessage *msg, gboolean standby_enable);
WFDResult wfdconfig_get_standby(WFDMessage *msg, gboolean *standby_enable);

WFDResult wfdconfig_set_connector_type(WFDMessage *msg, WFDConnector connector);
WFDResult wfdconfig_get_connector_type(WFDMessage *msg, WFDConnector *connector);

WFDResult wfdconfig_set_idr_request(WFDMessage *msg);

G_END_DECLS

#endif /* __GST_WFD_CONFIG_MESSAGE_H__ */
