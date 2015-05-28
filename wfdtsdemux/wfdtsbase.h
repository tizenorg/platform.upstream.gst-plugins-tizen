/*
 * wfdtsbase.h - GStreamer MPEG transport stream base class
 * Copyright (C) 2009 Edward Hervey <edward.hervey@collabora.co.uk>
 *               2007 Alessandro Decina
 * Copyright (C) 2011, Hewlett-Packard Development Company, L.P.
 *  Author: Youness Alaoui <youness.alaoui@collabora.co.uk>, Collabora Ltd.
 *  Author: Sebastian Dröge <sebastian.droege@collabora.co.uk>, Collabora Ltd.
 *
 * Authors:
 *   Alessandro Decina <alessandro@nnva.org>
 *   Edward Hervey <edward.hervey@collabora.co.uk>
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


#ifndef GST_WFD_TS_BASE_H
#define GST_WFD_TS_BASE_H

#include <gst/gst.h>
#include "wfdtspacketizer.h"

G_BEGIN_DECLS

#define GST_TYPE_WFD_TS_BASE \
  (wfd_ts_base_get_type())
#define GST_WFD_TS_BASE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_WFD_TS_BASE,WFDTSBase))
#define GST_WFD_TS_BASE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_WFD_TS_BASE,WFDTSBaseClass))
#define GST_IS_WFD_TS_BASE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_WFD_TS_BASE))
#define GST_IS_WFD_TS_BASE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_WFD_TS_BASE))
#define GST_WFD_TS_BASE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_WFD_TS_BASE, WFDTSBaseClass))

#define MPEG_TS_BASE_PACKETIZER(b) (((WFDTSBase*)b)->packetizer)

typedef struct _WFDTSBase WFDTSBase;
typedef struct _WFDTSBaseClass WFDTSBaseClass;
typedef struct _WFDTSBaseStream WFDTSBaseStream;
typedef struct _WFDTSBaseProgram WFDTSBaseProgram;

struct _WFDTSBaseStream
{
  guint16             pid;
  guint8              stream_type;

  /* Content of the registration descriptor (if present) */
  guint32             registration_id;

  GstWFDTSPMTStream *stream;
};

struct _WFDTSBaseProgram
{
  gint                program_number;
  guint16             pmt_pid;
  guint16             pcr_pid;

  /* Content of the registration descriptor (if present) */
  guint32             registration_id;

  GstWFDTSSection   *section;
  const GstWFDTSPMT *pmt;

  WFDTSBaseStream  **streams;
  GList              *stream_list;
  gint                patcount;

  /* Pending Tags for the program */
  GstTagList *tags;
  guint event_id;

  /* TRUE if the program is currently being used */
  gboolean active;
  /* TRUE if this is the first program created */
  gboolean initial_program;
};

typedef enum {
  /* PULL MODE */
  BASE_MODE_SCANNING,		/* Looking for PAT/PMT */
  BASE_MODE_SEEKING,		/* Seeking */
  BASE_MODE_STREAMING,		/* Normal mode (pushing out data) */

  /* PUSH MODE */
  BASE_MODE_PUSHING
} WFDTSBaseMode;

struct _WFDTSBase {
  GstElement element;

  GstPad *sinkpad;

  /* pull-based behaviour */
  WFDTSBaseMode mode;

  /* Current pull offset (also set by seek handler) */
  guint64	seek_offset;

  /* Cached packetsize */
  guint16	packetsize;

  /* the following vars must be protected with the OBJECT_LOCK as they can be
   * accessed from the application thread and the streaming thread */
  GHashTable *programs;

  GPtrArray  *pat;
  WFDTSPacketizer *packetizer;

  /* arrays that say whether a pid is a known psi pid or a pes pid */
  /* Use WFD_TS_BIT_* to set/unset/check the values */
  guint8 *known_psi;
  guint8 *is_pes;

  gboolean disposed;

  /* size of the WFDTSBaseProgram structure, can be overridden
   * by subclasses if they have their own WFDTSBaseProgram subclasses. */
  gsize program_size;

  /* size of the WFDTSBaseStream structure, can be overridden
   * by subclasses if they have their own WFDTSBaseStream subclasses */
  gsize stream_size;

  /* Whether we saw a PAT yet */
  gboolean seen_pat;

  /* Whether upstream is live or not */
  gboolean upstream_live;
  /* Whether we queried the upstream latency or not */
  gboolean queried_latency;

  /* Upstream segment */
  GstSegment segment;

  /* Last received seek event seqnum (default -1) */
  guint last_seek_seqnum;

  /* Whether to parse private section or not */
  gboolean parse_private_sections;

  /* Whether to push data and/or sections to subclasses */
  gboolean push_data;
  gboolean push_section;
};

struct _WFDTSBaseClass {
  GstElementClass parent_class;

  /* Virtual methods */
  void (*reset) (WFDTSBase *base);
  GstFlowReturn (*push) (WFDTSBase *base, WFDTSPacketizerPacket *packet, GstWFDTSSection * section);
  /* takes ownership of @event */
  gboolean (*push_event) (WFDTSBase *base, GstEvent * event);

  /* program_started gets called when program's pmt arrives for first time */
  void (*program_started) (WFDTSBase *base, WFDTSBaseProgram *program);
  /* program_stopped gets called when pat no longer has program's pmt */
  void (*program_stopped) (WFDTSBase *base, WFDTSBaseProgram *program);

  /* stream_added is called whenever a new stream has been identified */
  void (*stream_added) (WFDTSBase *base, WFDTSBaseStream *stream, WFDTSBaseProgram *program);
  /* stream_removed is called whenever a stream is no longer referenced */
  void (*stream_removed) (WFDTSBase *base, WFDTSBaseStream *stream);

  /* find_timestamps is called to find PCR */
  GstFlowReturn (*find_timestamps) (WFDTSBase * base, guint64 initoff, guint64 *offset);

  /* seek is called to wait for seeking */
  GstFlowReturn (*seek) (WFDTSBase * base, GstEvent * event);

  /* Drain all currently pending data */
  GstFlowReturn (*drain) (WFDTSBase * base);

  /* flush all streams
   * The hard inicator is used to flush completelly on FLUSH_STOP events
   * or partially in pull mode seeks of tsdemux */
  void (*flush) (WFDTSBase * base, gboolean hard);

  /* Notifies subclasses input buffer has been handled */
  GstFlowReturn (*input_done) (WFDTSBase *base, GstBuffer *buffer);

  /* signals */
  void (*pat_info) (GstStructure *pat);
  void (*pmt_info) (GstStructure *pmt);
  void (*nit_info) (GstStructure *nit);
  void (*sdt_info) (GstStructure *sdt);
  void (*eit_info) (GstStructure *eit);
};

#define WFD_TS_BIT_SET(field, offs)    ((field)[(offs) >> 3] |=  (1 << ((offs) & 0x7)))
#define WFD_TS_BIT_UNSET(field, offs)  ((field)[(offs) >> 3] &= ~(1 << ((offs) & 0x7)))
#define WFD_TS_BIT_IS_SET(field, offs) ((field)[(offs) >> 3] &   (1 << ((offs) & 0x7)))

G_GNUC_INTERNAL GType wfd_ts_base_get_type(void);

G_GNUC_INTERNAL WFDTSBaseProgram *wfd_ts_base_get_program (WFDTSBase * base, gint program_number);
G_GNUC_INTERNAL WFDTSBaseProgram *wfd_ts_base_add_program (WFDTSBase * base, gint program_number, guint16 pmt_pid);

G_GNUC_INTERNAL const GstWFDTSDescriptor *wfd_ts_get_descriptor_from_stream (WFDTSBaseStream * stream, guint8 tag);
G_GNUC_INTERNAL const GstWFDTSDescriptor *wfd_ts_get_descriptor_from_program (WFDTSBaseProgram * program, guint8 tag);

G_GNUC_INTERNAL gboolean
wfd_ts_base_handle_seek_event(WFDTSBase * base, GstPad * pad, GstEvent * event);

G_GNUC_INTERNAL gboolean gst_wfdtsbase_plugin_init (GstPlugin * plugin);

G_GNUC_INTERNAL void wfd_ts_base_program_remove_stream (WFDTSBase * base, WFDTSBaseProgram * program, guint16 pid);

G_GNUC_INTERNAL void wfd_ts_base_remove_program(WFDTSBase *base, gint program_number);
G_END_DECLS

#endif /* GST_WFD_TS_BASE_H */
