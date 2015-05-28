/*
 * wfdtspacketizer.h -
 * Copyright (C) 2013 Edward Hervey
 *
 * Authors:
 *   Edward Hervey <edward@collabora.com>
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef GST_WFD_TS_SECTION_H
#define GST_WFD_TS_SECTION_H

#include <gst/gst.h>
//#include "gstwfdtsdescriptor.h"

G_BEGIN_DECLS

typedef struct _GstWFDTSSection GstWFDTSSection;

#define GST_TYPE_WFD_TS_SECTION (gst_wfd_ts_section_get_type())
#define GST_WFD_TS_SECTION(section) ((GstWFDTSSection*) section)

#define GST_WFD_TS_SECTION_TYPE(section) (GST_WFD_TS_SECTION (section)->section_type)

GType gst_wfd_ts_section_get_type (void);

/**
 * GstWFDTSSectionType:
 * @GST_WFD_TS_SECTION_UNKNOWN: Unknown section type
 * @GST_WFD_TS_SECTION_PAT: Program Association Table (ISO/IEC 13818-1)
 * @GST_WFD_TS_SECTION_PMT: Program Map Table (ISO/IEC 13818-1)
 * @GST_WFD_TS_SECTION_CAT: Conditional Access Table (ISO/IEC 13818-1)
 * @GST_WFD_TS_SECTION_TSDT: Transport Stream Description Table (ISO/IEC 13818-1)
 * @GST_WFD_TS_SECTION_EIT: Event Information Table (EN 300 468)
 * @GST_WFD_TS_SECTION_NIT: Network Information Table (ISO/IEC 13818-1 / EN 300 468)
 * @GST_WFD_TS_SECTION_BAT: Bouquet Association Table ((EN 300 468)
 * @GST_WFD_TS_SECTION_SDT: Service Description Table (EN 300 468)
 * @GST_WFD_TS_SECTION_TDT: Time and Date Table (EN 300 468)
 * @GST_WFD_TS_SECTION_TOT: Time Offset Table (EN 300 468)
 * @GST_WFD_TS_SECTION_ATSC_TVCT: ATSC Terrestrial Virtual Channel Table (A65)
 * @GST_WFD_TS_SECTION_ATSC_CVCT: ATSC Cable Virtual Channel Table (A65)
 * @GST_WFD_TS_SECTION_ATSC_MGT: ATSC Master Guide Table (A65)
 * @GST_WFD_TS_SECTION_ATSC_ETT: ATSC Extended Text Table (A65)
 * @GST_WFD_TS_SECTION_ATSC_EIT: ATSC Event Information Table (A65)
 * @GST_WFD_TS_SECTION_ATSC_STT: ATSC System Time Table (A65)
 *
 * Types of #GstWFDTSSection that the library handles.
 */
typedef enum {
  GST_WFD_TS_SECTION_UNKNOWN           = 0,
  GST_WFD_TS_SECTION_PAT,
  GST_WFD_TS_SECTION_PMT,
  GST_WFD_TS_SECTION_CAT,
  GST_WFD_TS_SECTION_TSDT,
  GST_WFD_TS_SECTION_EIT,
  GST_WFD_TS_SECTION_NIT,
  GST_WFD_TS_SECTION_BAT,
  GST_WFD_TS_SECTION_SDT,
  GST_WFD_TS_SECTION_TDT,
  GST_WFD_TS_SECTION_TOT,
  GST_WFD_TS_SECTION_ATSC_TVCT,
  GST_WFD_TS_SECTION_ATSC_CVCT,
  GST_WFD_TS_SECTION_ATSC_MGT,
  GST_WFD_TS_SECTION_ATSC_ETT,
  GST_WFD_TS_SECTION_ATSC_EIT,
  GST_WFD_TS_SECTION_ATSC_STT
} GstWFDTSSectionType;

/**
 * GstWFDTSSectionTableID:
 *
 * Values for a #GstWFDTSSection table_id
 *
 * These are the registered ITU H.222.0 | ISO/IEC 13818-1 table_id variants.
 *
 * see also #GstWFDTSSectionATSCTableID, #GstWFDTSSectionDVBTableID, and
 * #GstWFDTSSectionSCTETableID
 */
typedef enum {
  /* ITU H.222.0 / IEC 13818-1 */
  GST_MTS_TABLE_ID_PROGRAM_ASSOCIATION		= 0x00,
  GST_MTS_TABLE_ID_CONDITIONAL_ACCESS		= 0x01,
  GST_MTS_TABLE_ID_TS_PROGRAM_MAP		= 0x02,
  GST_MTS_TABLE_ID_TS_DESCRIPTION		= 0x03,
  GST_MTS_TABLE_ID_14496_SCENE_DESCRIPTION	= 0x04,
  GST_MTS_TABLE_ID_14496_OBJET_DESCRIPTOR	= 0x05,
  GST_MTS_TABLE_ID_METADATA			= 0x06,
  GST_MTS_TABLE_ID_IPMP_CONTROL_INFORMATION	= 0x07,

  /* 0x08 - 0x39 : ITU H.222.0 | ISO/IEC 13818-1 reserved */

  /* IEC 13818-6 (DSM-CC) */
  GST_MTS_TABLE_ID_DSM_CC_MULTIPROTO_ENCAPSULATED_DATA	= 0x3A,
  GST_MTS_TABLE_ID_DSM_CC_U_N_MESSAGES			= 0x3B,
  GST_MTS_TABLE_ID_DSM_CC_DOWNLOAD_DATA_MESSAGES	= 0x3C,
  GST_MTS_TABLE_ID_DSM_CC_STREAM_DESCRIPTORS		= 0x3D,
  GST_MTS_TABLE_ID_DSM_CC_PRIVATE_DATA			= 0x3E,
  GST_MTS_TABLE_ID_DSM_CC_ADDRESSABLE_SECTIONS		= 0x3F,

  /* Unset */
  GST_MTS_TABLE_ID_UNSET = 0xFF

} GstWFDTSSectionTableID;

typedef gboolean (*GstWFDTSPacketizeFunc) (GstWFDTSSection *section);

/**
 * GstWFDTSSection:
 * @section_type: The type of section
 * @pid: The pid on which this section was found
 * @table_id: The table id of this section
 * @subtable_extension: This meaning differs per section. See the documentation
 * of the parsed section type for the meaning of this field
 * @version_number: Version of the section.
 * @current_next_indicator: Applies to current/next stream or not
 * @section_number: Number of the section (if multiple)
 * @last_section_number: Number of the last expected section (if multiple)
 * @crc: CRC
 *
 * Mpeg-TS Section Information (SI) (ISO/IEC 13818-1)
 */
struct _GstWFDTSSection
{
  /*< private >*/
  GstMiniObject parent;

  /*< public >*/
  GstWFDTSSectionType   section_type;

  guint16       pid;
  guint8        table_id;

  guint16       subtable_extension;
  guint8        version_number;

  gboolean      current_next_indicator;

  guint8        section_number;
  guint8        last_section_number;

  guint32       crc;

  /*< private >*/
  /* data: Points to beginning of section data
   * i.e. the first byte is the table_id field */
  guint8       *data;
  /* section_length: length of data (including final CRC if present) */
  guint		section_length;
  /* cached_parsed: cached copy of parsed section */
  gpointer     *cached_parsed;
  /* destroy_parsed: function to clear cached_parsed */
  GDestroyNotify destroy_parsed;
  /* offset: offset of the section within the container stream */
  guint64       offset;
  /* short_section: TRUE if section_syntax_indicator == 0
   * FIXME : Maybe make public later on when allowing creation of
   * sections to that people can create private short sections ? */
  gboolean      short_section;
  GstWFDTSPacketizeFunc packetizer;

  /* Padding for future extension */
  gpointer _gst_reserved[GST_PADDING];
};

GBytes *gst_wfd_ts_section_get_data (GstWFDTSSection *section);

/* PAT */
#define GST_TYPE_MPEGTS_PAT_PROGRAM (gst_wfd_ts_pat_program_get_type())

typedef struct _GstMpegtsPatProgram GstMpegtsPatProgram;
/**
 * GstMpegtsPatProgram:
 * @program_number: the program number
 * @network_or_program_map_PID: the network of program map PID
 *
 * A program entry from a Program Association Table (ITU H.222.0, ISO/IEC 13818-1).
 */
struct _GstMpegtsPatProgram
{
  guint16 program_number;
  guint16 network_or_program_map_PID;
};

GPtrArray *gst_wfd_ts_section_get_pat (GstWFDTSSection *section);
GType gst_wfd_ts_pat_program_get_type (void);

GPtrArray *gst_wfd_ts_pat_new (void);
GstMpegtsPatProgram *gst_wfd_ts_pat_program_new (void);
GstWFDTSSection *gst_wfd_ts_section_from_pat (GPtrArray * programs,
    guint16 ts_id);

/* CAT */

GPtrArray *gst_wfd_ts_section_get_cat (GstWFDTSSection *section);

/* PMT */
typedef struct _GstWFDTSPMTStream GstWFDTSPMTStream;
typedef struct _GstWFDTSPMT GstWFDTSPMT;
#define GST_TYPE_WFD_TS_PMT (gst_wfd_ts_pmt_get_type())
#define GST_TYPE_WFD_TS_PMT_STREAM (gst_wfd_ts_pmt_stream_get_type())

/**
 * GstMpegtsStreamType:
 * @GST_WFD_TS_STREAM_TYPE_RESERVED_00: ITU-T | ISO/IEC Reserved
 * @GST_WFD_TS_STREAM_TYPE_VIDEO_MPEG1: ISO/IEC 11172-2 Video
 * @GST_WFD_TS_STREAM_TYPE_VIDEO_MPEG2: Rec. ITU-T H.262 | ISO/IEC 13818-2
 * Video or ISO/IEC 11172-2 constrained parameter video stream
 * @GST_WFD_TS_STREAM_TYPE_AUDIO_MPEG1: ISO/IEC 11172-3 Audio
 * @GST_WFD_TS_STREAM_TYPE_AUDIO_MPEG2: ISO/IEC 13818-3 Audio
 * @GST_WFD_TS_STREAM_TYPE_PRIVATE_SECTIONS: private sections
 * @GST_WFD_TS_STREAM_TYPE_PRIVATE_PES_PACKETS: PES packets containing private data
 * @GST_WFD_TS_STREAM_TYPE_MHEG: ISO/IEC 13522 MHEG
 * @GST_WFD_TS_STREAM_TYPE_DSM_CC: Annex A DSM-CC
 * @GST_WFD_TS_STREAM_TYPE_H_222_1: Rec. ITU-T H.222.1
 * @GST_WFD_TS_STREAM_TYPE_DSMCC_A: ISO/IEC 13818-6 type A
 * @GST_WFD_TS_STREAM_TYPE_DSMCC_B: ISO/IEC 13818-6 type B
 * @GST_WFD_TS_STREAM_TYPE_DSMCC_C: ISO/IEC 13818-6 type C
 * @GST_WFD_TS_STREAM_TYPE_DSMCC_D: ISO/IEC 13818-6 type D
 * @GST_WFD_TS_STREAM_TYPE_AUXILIARY: auxiliary streams
 * @GST_WFD_TS_STREAM_TYPE_AUDIO_AAC_ADTS: ISO/IEC 13818-7 Audio with ADTS
 * transport syntax
 * @GST_WFD_TS_STREAM_TYPE_VIDEO_MPEG4: ISO/IEC 14496-2 Visual
 * @GST_WFD_TS_STREAM_TYPE_AUDIO_AAC_LATM: ISO/IEC 14496-3 Audio with the LATM
 * transport syntax as defined in ISO/IEC 14496-3
 * @GST_WFD_TS_STREAM_TYPE_SL_FLEXMUX_PES_PACKETS: ISO/IEC 14496-1
 * SL-packetized stream or FlexMux stream carried in PES packets
 * @GST_WFD_TS_STREAM_TYPE_SL_FLEXMUX_SECTIONS: ISO/IEC 14496-1 SL-packetized
 * stream or FlexMux stream carried in ISO/IEC 14496_sections
 * @GST_WFD_TS_STREAM_TYPE_SYNCHRONIZED_DOWNLOAD: ISO/IEC 13818-6 Synchronized
 * Download Protocol
 * @GST_WFD_TS_STREAM_TYPE_METADATA_PES_PACKETS: Metadata carried in PES packets
 * @GST_WFD_TS_STREAM_TYPE_METADATA_SECTIONS: Metadata carried in metadata_sections
 * @GST_WFD_TS_STREAM_TYPE_METADATA_DATA_CAROUSEL: Metadata carried in ISO/IEC
 * 13818-6 Data Carousel
 * @GST_WFD_TS_STREAM_TYPE_METADATA_OBJECT_CAROUSEL: Metadata carried in
 * ISO/IEC 13818-6 Object Carousel
 * @GST_WFD_TS_STREAM_TYPE_METADATA_SYNCHRONIZED_DOWNLOAD: Metadata carried in
 * ISO/IEC 13818-6 Synchronized Download Protocol
 * @GST_WFD_TS_STREAM_TYPE_MPEG2_IPMP: IPMP stream (defined in ISO/IEC 13818-11,
 * MPEG-2 IPMP)
 * @GST_WFD_TS_STREAM_TYPE_VIDEO_H264: AVC video stream conforming to one or
 * more profiles defined in Annex A of Rec. ITU-T H.264 | ISO/IEC 14496-10 or
 * AVC video sub-bitstream of SVC as defined in 2.1.78 or MVC base view
 * sub-bitstream, as defined in 2.1.85, or AVC video sub-bitstream of MVC, as
 * defined in 2.1.88
 * @GST_WFD_TS_STREAM_TYPE_AUDIO_AAC_CLEAN: ISO/IEC 14496-3 Audio, without
 * using any additional transport syntax, such as DST, ALS and SLS
 * @GST_WFD_TS_STREAM_TYPE_MPEG4_TIMED_TEXT: ISO/IEC 14496-17 Text
 * @GST_WFD_TS_STREAM_TYPE_VIDEO_RVC: Auxiliary video stream as defined in
 * ISO/IEC 23002-3
 * @GST_WFD_TS_STREAM_TYPE_VIDEO_H264_SVC_SUB_BITSTREAM: SVC video sub-bitstream
 * of an AVC video stream conforming to one or more profiles defined in Annex G
 * of Rec. ITU-T H.264 | ISO/IEC 14496-10
 * @GST_WFD_TS_STREAM_TYPE_VIDEO_H264_MVC_SUB_BITSTREAM: MVC video sub-bitstream
 * of an AVC video stream conforming to one or more profiles defined in Annex H
 * of Rec. ITU-T H.264 | ISO/IEC 14496-10
 * @GST_WFD_TS_STREAM_TYPE_VIDEO_JP2K: Video stream conforming to one or more
 * profiles as defined in Rec. ITU-T T.800 | ISO/IEC 15444-1
 * @GST_WFD_TS_STREAM_TYPE_VIDEO_MPEG2_STEREO_ADDITIONAL_VIEW: Additional view
 * Rec. ITU-T H.262 | ISO/IEC 13818-2 video stream for service-compatible
 * stereoscopic 3D services
 * @GST_WFD_TS_STREAM_TYPE_VIDEO_H264_STEREO_ADDITIONAL_VIEW: Additional view
 * Rec. ITU-T H.264 | ISO/IEC 14496-10 video stream conforming to one or more
 * profiles defined in Annex A for service-compatible stereoscopic 3D services
 * @GST_WFD_TS_STREAM_TYPE_IPMP_STREAM: IPMP stream
 *
 * Type of mpeg-ts stream type.
 *
 * These values correspond to the base standard registered types. Depending
 * on the variant of mpeg-ts being used (Bluray, ATSC, DVB, ...), other
 * types might also be used, but will not conflict with these.
 *
 * Corresponds to table 2-34 of ITU H.222.0 | ISO/IEC 13818-1
 */
typedef enum {
  GST_WFD_TS_STREAM_TYPE_RESERVED_00                  = 0x00,
  GST_WFD_TS_STREAM_TYPE_VIDEO_MPEG1                  = 0x01,
  GST_WFD_TS_STREAM_TYPE_VIDEO_MPEG2                  = 0x02,
  GST_WFD_TS_STREAM_TYPE_AUDIO_MPEG1                  = 0x03,
  GST_WFD_TS_STREAM_TYPE_AUDIO_MPEG2                  = 0x04,
  GST_WFD_TS_STREAM_TYPE_PRIVATE_SECTIONS             = 0x05,
  GST_WFD_TS_STREAM_TYPE_PRIVATE_PES_PACKETS          = 0x06,
  GST_WFD_TS_STREAM_TYPE_MHEG                         = 0x07,
  GST_WFD_TS_STREAM_TYPE_DSM_CC                       = 0x08,
  GST_WFD_TS_STREAM_TYPE_H_222_1                      = 0x09,
  GST_WFD_TS_STREAM_TYPE_DSMCC_A                      = 0x0a,
  GST_WFD_TS_STREAM_TYPE_DSMCC_B                      = 0x0b,
  GST_WFD_TS_STREAM_TYPE_DSMCC_C                      = 0x0c,
  GST_WFD_TS_STREAM_TYPE_DSMCC_D                      = 0x0d,
  GST_WFD_TS_STREAM_TYPE_AUXILIARY                    = 0x0e,
  GST_WFD_TS_STREAM_TYPE_AUDIO_AAC_ADTS               = 0x0f,
  GST_WFD_TS_STREAM_TYPE_VIDEO_MPEG4                  = 0x10,
  GST_WFD_TS_STREAM_TYPE_AUDIO_AAC_LATM               = 0x11,
  GST_WFD_TS_STREAM_TYPE_SL_FLEXMUX_PES_PACKETS       = 0x12,
  GST_WFD_TS_STREAM_TYPE_SL_FLEXMUX_SECTIONS          = 0x13,
  GST_WFD_TS_STREAM_TYPE_SYNCHRONIZED_DOWNLOAD        = 0x14,
  GST_WFD_TS_STREAM_TYPE_METADATA_PES_PACKETS         = 0x15,
  GST_WFD_TS_STREAM_TYPE_METADATA_SECTIONS            = 0x16,
  GST_WFD_TS_STREAM_TYPE_METADATA_DATA_CAROUSEL       = 0x17,
  GST_WFD_TS_STREAM_TYPE_METADATA_OBJECT_CAROUSEL     = 0x18,
  GST_WFD_TS_STREAM_TYPE_METADATA_SYNCHRONIZED_DOWNLOAD  = 0x19,
  GST_WFD_TS_STREAM_TYPE_MPEG2_IPMP                   = 0x1a,
  GST_WFD_TS_STREAM_TYPE_VIDEO_H264                   = 0x1b,
  GST_WFD_TS_STREAM_TYPE_AUDIO_AAC_CLEAN              = 0x1c,
  GST_WFD_TS_STREAM_TYPE_MPEG4_TIMED_TEXT             = 0x1d,
  GST_WFD_TS_STREAM_TYPE_VIDEO_RVC                    = 0x1e,
  GST_WFD_TS_STREAM_TYPE_VIDEO_H264_SVC_SUB_BITSTREAM = 0x1f,
  GST_WFD_TS_STREAM_TYPE_VIDEO_H264_MVC_SUB_BITSTREAM = 0x20,
  GST_WFD_TS_STREAM_TYPE_VIDEO_JP2K                   = 0x21,
  GST_WFD_TS_STREAM_TYPE_VIDEO_MPEG2_STEREO_ADDITIONAL_VIEW = 0x22,
  GST_WFD_TS_STREAM_TYPE_VIDEO_H264_STEREO_ADDITIONAL_VIEW  = 0x23,
  GST_WFD_TS_STREAM_TYPE_VIDEO_HEVC                   = 0x24,
  /* 0x24 - 0x7e : Rec. ITU-T H.222.0 | ISO/IEC 13818-1 Reserved */
  GST_WFD_TS_STREAM_TYPE_IPMP_STREAM                  = 0x7f
  /* 0x80 - 0xff : User Private (or defined in other specs) */
} GstMpegtsStreamType;

/**
 * GstWFDTSPMTStream:
 * @stream_type: the type of stream. See #GstMpegtsStreamType
 * @pid: the PID of the stream
 * @descriptors: (element-type GstMpegtsDescriptor): the descriptors of the
 * stream
 *
 * An individual stream definition.
 */
struct _GstWFDTSPMTStream
{
  guint8      stream_type;
  guint16     pid;

  GPtrArray  *descriptors;
};

/**
 * GstWFDTSPMT:
 * @pcr_pid: PID of the stream containing PCR
 * @descriptors: (element-type GstMpegtsDescriptor): array of #GstMpegtsDescriptor
 * @streams: (element-type GstWFDTSPMTStream): Array of #GstWFDTSPMTStream
 *
 * Program Map Table (ISO/IEC 13818-1).
 *
 * The program_number is contained in the subtable_extension field of the
 * container #GstWFDTSSection.
 */
struct _GstWFDTSPMT
{
  guint16    pcr_pid;
  guint16    program_number;

  GPtrArray *descriptors;
  GPtrArray *streams;
};

GType gst_wfd_ts_pmt_get_type (void);
GType gst_wfd_ts_pmt_stream_get_type (void);

GstWFDTSPMT *gst_wfd_ts_pmt_new (void);
GstWFDTSPMTStream *gst_wfd_ts_pmt_stream_new (void);
const GstWFDTSPMT *gst_wfd_ts_section_get_pmt (GstWFDTSSection *section);
GstWFDTSSection *gst_wfd_ts_section_from_pmt (GstWFDTSPMT *pmt, guint16 pid);

/* TSDT */

GPtrArray *gst_wfd_ts_section_get_tsdt (GstWFDTSSection *section);


/* generic */

#define gst_wfd_ts_section_ref(section)   ((GstWFDTSSection*) gst_mini_object_ref (GST_MINI_OBJECT_CAST (section)))
#define gst_wfd_ts_section_unref(section) (gst_mini_object_unref (GST_MINI_OBJECT_CAST (section)))

GstMessage *gst_message_new_wfd_ts_section (GstObject *parent, GstWFDTSSection *section);
gboolean gst_wfd_ts_section_send_event (GstWFDTSSection * section, GstElement * element);
GstWFDTSSection *gst_event_parse_wfd_ts_section (GstEvent * event);

GstWFDTSSection *gst_message_parse_wfd_ts_section (GstMessage *message);

GstWFDTSSection *gst_wfd_ts_section_new (guint16 pid,
					   guint8 * data,
					   gsize data_size);

guint8 *gst_wfd_ts_section_packetize (GstWFDTSSection * section, gsize * output_size);


void gst_wfd_ts_initialize (void);


G_END_DECLS

#endif				/* GST_WFD_TS_SECTION_H */
