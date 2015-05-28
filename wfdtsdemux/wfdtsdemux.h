/*
 * tsdemux - GStreamer MPEG transport stream demuxer
 * Copyright (C) 2009 Zaheer Abbas Merali
 *               2010 Edward Hervey
 *
 * Authors:
 *   Zaheer Abbas Merali <zaheerabbas at merali dot org>
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


#ifndef GST_WFD_TS_DEMUX_H
#define GST_WFD_TS_DEMUX_H

#include <gst/gst.h>
#include <gst/base/gstbytereader.h>
#include <gst/base/gstflowcombiner.h>
#include "wfdtsbase.h"
#include "wfdtspacketizer.h"

G_BEGIN_DECLS
#define GST_TYPE_WFD_TS_DEMUX \
  (gst_wfd_ts_demux_get_type())
#define GST_WFD_TS_DEMUX(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_WFD_TS_DEMUX,GstWFDTSDemux))
#define GST_WFD_TS_DEMUX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_WFD_TS_DEMUX,GstWFDTSDemuxClass))
#define GST_IS_WFD_TS_DEMUX(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_WFD_TS_DEMUX))
#define GST_IS_WFD_TS_DEMUX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_WFD_TS_DEMUX))
#define GST_WFD_TS_DEMUX_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_WFD_TS_DEMUX, GstWFDTSDemuxClass))
#define GST_WFD_TS_DEMUX_CAST(obj) ((GstWFDTSDemux*) obj)
typedef struct _GstWFDTSDemux GstWFDTSDemux;
typedef struct _GstWFDTSDemuxClass GstWFDTSDemuxClass;

struct _GstWFDTSDemux
{
  WFDTSBase parent;

  gboolean have_group_id;
  guint group_id;

  /* the following vars must be protected with the OBJECT_LOCK as they can be
   * accessed from the application thread and the streaming thread */
  gint requested_program_number; /* Required program number (ignore:-1) */
  guint program_number;
  gboolean emit_statistics;

  /*< private >*/
  WFDTSBaseProgram *program;	/* Current program */

  /* segments to be sent */
  GstSegment segment;
  GstEvent *segment_event;

  /* global taglist */
  GstTagList *global_tags;

  /* Set when program change */
  gboolean calculate_update_segment;
  /* update segment is */
  GstEvent *update_segment;

  /* Full stream duration */
  GstClockTime duration;

  /* Pending seek rate (default 1.0) */
  gdouble rate;

  GstFlowCombiner *flowcombiner;

  /* Used when seeking for a keyframe to go backward in the stream */
  guint64 last_seek_offset;
};

struct _GstWFDTSDemuxClass
{
  WFDTSBaseClass parent_class;
};

G_GNUC_INTERNAL GType gst_wfd_ts_demux_get_type (void);

G_GNUC_INTERNAL gboolean gst_wfd_ts_demux_plugin_init (GstPlugin * plugin);

G_END_DECLS
#endif /* GST_WFD_TS_DEMUX_H */
