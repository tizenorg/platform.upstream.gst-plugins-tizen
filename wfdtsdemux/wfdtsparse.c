/*
 * wfdtsparse.c -
 * Copyright (C) 2007 Alessandro Decina
 *
 * Authors:
 *   Alessandro Decina <alessandro@nnva.org>
 *   Zaheer Abbas Merali <zaheerabbas at merali dot org>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wfdtsbase.h"
#include "wfdtsparse.h"
#include "gstwfdtsdesc.h"

/* latency in mseconds */
#define TS_LATENCY 700

#define TABLE_ID_UNSET 0xFF
#define RUNNING_STATUS_RUNNING 4

GST_DEBUG_CATEGORY_STATIC (wfd_ts_parse_debug);
#define GST_CAT_DEFAULT wfd_ts_parse_debug

typedef struct _WFDTSParsePad WFDTSParsePad;

typedef struct
{
  WFDTSBaseProgram program;
  WFDTSParsePad *tspad;
} WFDTSParseProgram;

struct _WFDTSParsePad
{
  GstPad *pad;

  /* the program number that the peer wants on this pad */
  gint program_number;
  WFDTSParseProgram *program;

  /* set to FALSE before a push and TRUE after */
  gboolean pushed;

  /* the return of the latest push */
  GstFlowReturn flow_return;
};

static GstStaticPadTemplate src_template =
GST_STATIC_PAD_TEMPLATE ("src", GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/mpegts, " "systemstream = (boolean) true ")
    );

static GstStaticPadTemplate program_template =
GST_STATIC_PAD_TEMPLATE ("program_%u", GST_PAD_SRC,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS ("video/mpegts, " "systemstream = (boolean) true ")
    );

enum
{
  ARG_0,
  /* FILL ME */
};

static void
wfd_ts_parse_program_started (WFDTSBase * base, WFDTSBaseProgram * program);
static void
wfd_ts_parse_program_stopped (WFDTSBase * base, WFDTSBaseProgram * program);

static GstFlowReturn
wfd_ts_parse_push (WFDTSBase * base, WFDTSPacketizerPacket * packet,
    GstWFDTSSection * section);

static WFDTSParsePad *wfd_ts_parse_create_tspad (WFDTSParse * parse,
    const gchar * name);
static void wfd_ts_parse_destroy_tspad (WFDTSParse * parse,
    WFDTSParsePad * tspad);

static void wfd_ts_parse_pad_removed (GstElement * element, GstPad * pad);
static GstPad *wfd_ts_parse_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * name, const GstCaps * caps);
static void wfd_ts_parse_release_pad (GstElement * element, GstPad * pad);
static gboolean wfd_ts_parse_src_pad_query (GstPad * pad, GstObject * parent,
    GstQuery * query);
static gboolean push_event (WFDTSBase * base, GstEvent * event);

#define wfd_ts_parse_parent_class parent_class
G_DEFINE_TYPE (WFDTSParse, wfd_ts_parse, GST_TYPE_WFD_TS_BASE);
static void wfd_ts_parse_reset (WFDTSBase * base);
static GstFlowReturn wfd_ts_parse_input_done (WFDTSBase * base,
    GstBuffer * buffer);

static void
wfd_ts_parse_class_init (WFDTSParseClass * klass)
{
  GstElementClass *element_class;
  WFDTSBaseClass *ts_class;

  element_class = GST_ELEMENT_CLASS (klass);
  element_class->pad_removed = wfd_ts_parse_pad_removed;
  element_class->request_new_pad = wfd_ts_parse_request_new_pad;
  element_class->release_pad = wfd_ts_parse_release_pad;

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&program_template));

  gst_element_class_set_static_metadata (element_class,
      "MPEG transport stream parser", "Codec/Parser",
      "Parses MPEG2 transport streams",
      "Alessandro Decina <alessandro@nnva.org>, "
      "Zaheer Abbas Merali <zaheerabbas at merali dot org>");

  ts_class = GST_WFD_TS_BASE_CLASS (klass);
  ts_class->push = GST_DEBUG_FUNCPTR (wfd_ts_parse_push);
  ts_class->push_event = GST_DEBUG_FUNCPTR (push_event);
  ts_class->program_started = GST_DEBUG_FUNCPTR (wfd_ts_parse_program_started);
  ts_class->program_stopped = GST_DEBUG_FUNCPTR (wfd_ts_parse_program_stopped);
  ts_class->reset = GST_DEBUG_FUNCPTR (wfd_ts_parse_reset);
  ts_class->input_done = GST_DEBUG_FUNCPTR (wfd_ts_parse_input_done);
}

static void
wfd_ts_parse_init (WFDTSParse * parse)
{
  WFDTSBase *base = (WFDTSBase *) parse;

  base->program_size = sizeof (WFDTSParseProgram);
  /* We will only need to handle data/section if we have request pads */
  base->push_data = FALSE;
  base->push_section = FALSE;

  parse->srcpad = gst_pad_new_from_static_template (&src_template, "src");
  parse->first = TRUE;
  gst_element_add_pad (GST_ELEMENT (parse), parse->srcpad);

  parse->have_group_id = FALSE;
  parse->group_id = G_MAXUINT;
}

static void
wfd_ts_parse_reset (WFDTSBase * base)
{
  /* Set the various know PIDs we are interested in */

  /* CAT */
  WFD_TS_BIT_SET (base->known_psi, 1);
  /* NIT, ST */
  WFD_TS_BIT_SET (base->known_psi, 0x10);
  /* SDT, BAT, ST */
  WFD_TS_BIT_SET (base->known_psi, 0x11);
  /* EIT, ST, CIT (TS 102 323) */
  WFD_TS_BIT_SET (base->known_psi, 0x12);
  /* RST, ST */
  WFD_TS_BIT_SET (base->known_psi, 0x13);
  /* RNT (TS 102 323) */
  WFD_TS_BIT_SET (base->known_psi, 0x16);
  /* inband signalling */
  WFD_TS_BIT_SET (base->known_psi, 0x1c);
  /* measurement */
  WFD_TS_BIT_SET (base->known_psi, 0x1d);
  /* DIT */
  WFD_TS_BIT_SET (base->known_psi, 0x1e);
  /* SIT */
  WFD_TS_BIT_SET (base->known_psi, 0x1f);

  GST_WFD_TS_PARSE (base)->first = TRUE;
  GST_WFD_TS_PARSE (base)->have_group_id = FALSE;
  GST_WFD_TS_PARSE (base)->group_id = G_MAXUINT;

  g_list_free_full (GST_WFD_TS_PARSE (base)->pending_buffers,
      (GDestroyNotify) gst_buffer_unref);
  GST_WFD_TS_PARSE (base)->pending_buffers = NULL;;
}

static void
wfd_prepare_src_pad (WFDTSBase * base, WFDTSParse * parse)
{
  if (base->packetizer->packet_size) {
    GstEvent *event;
    gchar *stream_id;
    GstCaps *caps;

    stream_id =
        gst_pad_create_stream_id (parse->srcpad, GST_ELEMENT_CAST (base),
        "multi-program");

    event =
        gst_pad_get_sticky_event (parse->parent.sinkpad, GST_EVENT_STREAM_START,
        0);
    if (event) {
      if (gst_event_parse_group_id (event, &parse->group_id))
        parse->have_group_id = TRUE;
      else
        parse->have_group_id = FALSE;
      gst_event_unref (event);
    } else if (!parse->have_group_id) {
      parse->have_group_id = TRUE;
      parse->group_id = gst_util_group_id_next ();
    }
    event = gst_event_new_stream_start (stream_id);
    if (parse->have_group_id)
      gst_event_set_group_id (event, parse->group_id);

    gst_pad_push_event (parse->srcpad, event);
    g_free (stream_id);

    caps = gst_caps_new_simple ("video/mpegts",
        "systemstream", G_TYPE_BOOLEAN, TRUE,
        "packetsize", G_TYPE_INT, base->packetizer->packet_size, NULL);

    gst_pad_set_caps (parse->srcpad, caps);
    gst_caps_unref (caps);

    gst_pad_push_event (parse->srcpad, gst_event_new_segment (&base->segment));

    parse->first = FALSE;
  }
}

static gboolean
push_event (WFDTSBase * base, GstEvent * event)
{
  WFDTSParse *parse = (WFDTSParse *) base;
  GList *tmp;

  if (G_UNLIKELY (parse->first)) {
    /* We will send the segment when really starting  */
    if (G_UNLIKELY (GST_EVENT_TYPE (event) == GST_EVENT_SEGMENT)) {
      gst_event_unref (event);
      return TRUE;
    }
    wfd_prepare_src_pad (base, parse);
  }

  for (tmp = parse->srcpads; tmp; tmp = tmp->next) {
    GstPad *pad = (GstPad *) tmp->data;
    if (pad) {
      gst_event_ref (event);
      gst_pad_push_event (pad, event);
    }
  }

  gst_pad_push_event (parse->srcpad, event);

  return TRUE;
}

static WFDTSParsePad *
wfd_ts_parse_create_tspad (WFDTSParse * parse, const gchar * pad_name)
{
  GstPad *pad;
  WFDTSParsePad *tspad;

  pad = gst_pad_new_from_static_template (&program_template, pad_name);
  gst_pad_set_query_function (pad,
      GST_DEBUG_FUNCPTR (wfd_ts_parse_src_pad_query));

  /* create our wrapper */
  tspad = g_new0 (WFDTSParsePad, 1);
  tspad->pad = pad;
  tspad->program_number = -1;
  tspad->program = NULL;
  tspad->pushed = FALSE;
  tspad->flow_return = GST_FLOW_NOT_LINKED;
  gst_pad_set_element_private (pad, tspad);

  return tspad;
}

static void
wfd_ts_parse_destroy_tspad (WFDTSParse * parse, WFDTSParsePad * tspad)
{
  /* free the wrapper */
  g_free (tspad);
}

static void
wfd_ts_parse_pad_removed (GstElement * element, GstPad * pad)
{
  WFDTSParsePad *tspad;
  WFDTSBase *base = (WFDTSBase *) element;
  WFDTSParse *parse = GST_WFD_TS_PARSE (element);

  if (gst_pad_get_direction (pad) == GST_PAD_SINK)
    return;

  tspad = (WFDTSParsePad *) gst_pad_get_element_private (pad);
  if (tspad) {
    wfd_ts_parse_destroy_tspad (parse, tspad);

    parse->srcpads = g_list_remove_all (parse->srcpads, pad);
  }
  if (parse->srcpads == NULL) {
    base->push_data = FALSE;
    base->push_section = FALSE;
  }

  if (GST_ELEMENT_CLASS (parent_class)->pad_removed)
    GST_ELEMENT_CLASS (parent_class)->pad_removed (element, pad);
}

static GstPad *
wfd_ts_parse_request_new_pad (GstElement * element, GstPadTemplate * template,
    const gchar * padname, const GstCaps * caps)
{
  WFDTSBase *base = (WFDTSBase *) element;
  WFDTSParse *parse;
  WFDTSParsePad *tspad;
  WFDTSParseProgram *parseprogram;
  GstPad *pad;
  gint program_num = -1;
  GstEvent *event;
  gchar *stream_id;

  g_return_val_if_fail (template != NULL, NULL);
  g_return_val_if_fail (GST_IS_WFD_TS_PARSE (element), NULL);
  g_return_val_if_fail (padname != NULL, NULL);

  sscanf (padname + 8, "%d", &program_num);

  GST_DEBUG_OBJECT (element, "padname:%s, program:%d", padname, program_num);

  parse = GST_WFD_TS_PARSE (element);

  tspad = wfd_ts_parse_create_tspad (parse, padname);
  tspad->program_number = program_num;

  /* Find if the program is already active */
  parseprogram =
      (WFDTSParseProgram *) wfd_ts_base_get_program (GST_WFD_TS_BASE (parse),
      program_num);
  if (parseprogram) {
    tspad->program = parseprogram;
    parseprogram->tspad = tspad;
  }

  pad = tspad->pad;
  parse->srcpads = g_list_append (parse->srcpads, pad);
  base->push_data = TRUE;
  base->push_section = TRUE;

  gst_pad_set_active (pad, TRUE);

  stream_id = gst_pad_create_stream_id (pad, element, padname + 8);

  event =
      gst_pad_get_sticky_event (parse->parent.sinkpad, GST_EVENT_STREAM_START,
      0);
  if (event) {
    if (gst_event_parse_group_id (event, &parse->group_id))
      parse->have_group_id = TRUE;
    else
      parse->have_group_id = FALSE;
    gst_event_unref (event);
  } else if (!parse->have_group_id) {
    parse->have_group_id = TRUE;
    parse->group_id = gst_util_group_id_next ();
  }
  event = gst_event_new_stream_start (stream_id);
  if (parse->have_group_id)
    gst_event_set_group_id (event, parse->group_id);

  gst_pad_push_event (pad, event);
  g_free (stream_id);

  gst_element_add_pad (element, pad);

  return pad;
}

static void
wfd_ts_parse_release_pad (GstElement * element, GstPad * pad)
{
  gst_pad_set_active (pad, FALSE);
  /* we do the cleanup in GstElement::pad-removed */
  gst_element_remove_pad (element, pad);
}

static GstFlowReturn
wfd_ts_parse_tspad_push_section (WFDTSParse * parse, WFDTSParsePad * tspad,
    GstWFDTSSection * section, WFDTSPacketizerPacket * packet)
{
  GstFlowReturn ret = GST_FLOW_OK;
  gboolean to_push = TRUE;

  if (tspad->program_number != -1) {
    if (tspad->program) {
      /* we push all sections to all pads except PMTs which we
       * only push to pads meant to receive that program number */
      if (section->table_id == 0x02) {
        /* PMT */
        if (section->subtable_extension != tspad->program_number)
          to_push = FALSE;
      }
    } else {
      /* there's a program filter on the pad but the PMT for the program has not
       * been parsed yet, ignore the pad until we get a PMT */
      to_push = FALSE;
    }
  }

  GST_DEBUG_OBJECT (parse,
      "pushing section: %d program number: %d table_id: %d", to_push,
      tspad->program_number, section->table_id);

  if (to_push) {
    GstBuffer *buf =
        gst_buffer_new_and_alloc (packet->data_end - packet->data_start);
    gst_buffer_fill (buf, 0, packet->data_start,
        packet->data_end - packet->data_start);
    ret = gst_pad_push (tspad->pad, buf);
  }

  return ret;
}

static GstFlowReturn
wfd_ts_parse_tspad_push (WFDTSParse * parse, WFDTSParsePad * tspad,
    WFDTSPacketizerPacket * packet)
{
  GstFlowReturn ret = GST_FLOW_OK;
  WFDTSBaseStream **pad_pids = NULL;

  if (tspad->program_number != -1) {
    if (tspad->program) {
      WFDTSBaseProgram *bp = (WFDTSBaseProgram *) tspad->program;
      pad_pids = bp->streams;
    } else {
      /* there's a program filter on the pad but the PMT for the program has not
       * been parsed yet, ignore the pad until we get a PMT */
      goto out;
    }
  }

  if (pad_pids == NULL || pad_pids[packet->pid]) {
    GstBuffer *buf =
        gst_buffer_new_and_alloc (packet->data_end - packet->data_start);
    gst_buffer_fill (buf, 0, packet->data_start,
        packet->data_end - packet->data_start);
    /* push if there's no filter or if the pid is in the filter */
    ret = gst_pad_push (tspad->pad, buf);
  }

out:
  return ret;
}

static void
pad_clear_for_push (GstPad * pad, WFDTSParse * parse)
{
  WFDTSParsePad *tspad = (WFDTSParsePad *) gst_pad_get_element_private (pad);

  tspad->flow_return = GST_FLOW_NOT_LINKED;
  tspad->pushed = FALSE;
}

static GstFlowReturn
wfd_ts_parse_push (WFDTSBase * base, WFDTSPacketizerPacket * packet,
    GstWFDTSSection * section)
{
  WFDTSParse *parse = (WFDTSParse *) base;
  guint32 pads_cookie;
  gboolean done = FALSE;
  GstPad *pad = NULL;
  WFDTSParsePad *tspad;
  GstFlowReturn ret;
  GList *srcpads;

  GST_OBJECT_LOCK (parse);
  srcpads = parse->srcpads;

  /* clear tspad->pushed on pads */
  g_list_foreach (srcpads, (GFunc) pad_clear_for_push, parse);
  if (srcpads)
    ret = GST_FLOW_NOT_LINKED;
  else
    ret = GST_FLOW_OK;

  /* Get cookie and source pads list */
  pads_cookie = GST_ELEMENT_CAST (parse)->pads_cookie;
  if (G_LIKELY (srcpads)) {
    pad = GST_PAD_CAST (srcpads->data);
    g_object_ref (pad);
  }
  GST_OBJECT_UNLOCK (parse);

  while (pad && !done) {
    tspad = gst_pad_get_element_private (pad);

    if (G_LIKELY (!tspad->pushed)) {
      if (section) {
        tspad->flow_return =
            wfd_ts_parse_tspad_push_section (parse, tspad, section, packet);
      } else {
        tspad->flow_return = wfd_ts_parse_tspad_push (parse, tspad, packet);
      }
      tspad->pushed = TRUE;

      if (G_UNLIKELY (tspad->flow_return != GST_FLOW_OK
              && tspad->flow_return != GST_FLOW_NOT_LINKED)) {
        /* return the error upstream */
        ret = tspad->flow_return;
        done = TRUE;
      }

    }

    if (ret == GST_FLOW_NOT_LINKED)
      ret = tspad->flow_return;

    g_object_unref (pad);

    if (G_UNLIKELY (!done)) {
      GST_OBJECT_LOCK (parse);
      if (G_UNLIKELY (pads_cookie != GST_ELEMENT_CAST (parse)->pads_cookie)) {
        /* resync */
        GST_DEBUG ("resync");
        pads_cookie = GST_ELEMENT_CAST (parse)->pads_cookie;
        srcpads = parse->srcpads;
      } else {
        GST_DEBUG ("getting next pad");
        /* Get next pad */
        srcpads = g_list_next (srcpads);
      }

      if (srcpads) {
        pad = GST_PAD_CAST (srcpads->data);
        g_object_ref (pad);
      } else
        done = TRUE;
      GST_OBJECT_UNLOCK (parse);
    }
  }

  return ret;
}

static GstFlowReturn
wfd_ts_parse_input_done (WFDTSBase * base, GstBuffer * buffer)
{
  WFDTSParse *parse = GST_WFD_TS_PARSE (base);
  GstFlowReturn ret = GST_FLOW_OK;

  if (G_UNLIKELY (parse->first))
    wfd_prepare_src_pad (base, parse);

  if (G_UNLIKELY (parse->first)) {
    parse->pending_buffers = g_list_append (parse->pending_buffers, buffer);
    return GST_FLOW_OK;
  }

  if (G_UNLIKELY (parse->pending_buffers)) {
    GList *l;

    for (l = parse->pending_buffers; l; l = l->next) {
      if (ret == GST_FLOW_OK)
        ret = gst_pad_push (parse->srcpad, l->data);
      else
        gst_buffer_unref (l->data);
    }
    g_list_free (parse->pending_buffers);
    parse->pending_buffers = NULL;

    if (ret != GST_FLOW_OK) {
      gst_buffer_unref (buffer);
      return ret;
    }
  }

  return gst_pad_push (parse->srcpad, buffer);
}

static WFDTSParsePad *
wfd_find_pad_for_program (WFDTSParse * parse, guint program_number)
{
  GList *tmp;

  for (tmp = parse->srcpads; tmp; tmp = tmp->next) {
    WFDTSParsePad *tspad = gst_pad_get_element_private ((GstPad *) tmp->data);

    if (tspad->program_number == program_number)
      return tspad;
  }

  return NULL;
}

static void
wfd_ts_parse_program_started (WFDTSBase * base, WFDTSBaseProgram * program)
{
  WFDTSParse *parse = GST_WFD_TS_PARSE (base);
  WFDTSParseProgram *parseprogram = (WFDTSParseProgram *) program;
  WFDTSParsePad *tspad;

  /* If we have a request pad for that program, activate it */
  tspad = wfd_find_pad_for_program (parse, program->program_number);

  if (tspad) {
    tspad->program = parseprogram;
    parseprogram->tspad = tspad;
  }
}

static void
wfd_ts_parse_program_stopped (WFDTSBase * base, WFDTSBaseProgram * program)
{
  WFDTSParse *parse = GST_WFD_TS_PARSE (base);
  WFDTSParseProgram *parseprogram = (WFDTSParseProgram *) program;
  WFDTSParsePad *tspad;

  /* If we have a request pad for that program, activate it */
  tspad = wfd_find_pad_for_program (parse, program->program_number);

  if (tspad) {
    tspad->program = NULL;
    parseprogram->tspad = NULL;
  }
}

static gboolean
wfd_ts_parse_src_pad_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  WFDTSParse *parse = GST_WFD_TS_PARSE (parent);
  gboolean res;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_LATENCY:
    {
      if ((res = gst_pad_peer_query (((WFDTSBase *) parse)->sinkpad, query))) {
        gboolean is_live;
        GstClockTime min_latency, max_latency;

        gst_query_parse_latency (query, &is_live, &min_latency, &max_latency);
        if (is_live) {
          min_latency += TS_LATENCY * GST_MSECOND;
          if (max_latency != GST_CLOCK_TIME_NONE)
            max_latency += TS_LATENCY * GST_MSECOND;
        }

        gst_query_set_latency (query, is_live, min_latency, max_latency);
      }

      break;
    }
    default:
      res = gst_pad_query_default (pad, parent, query);
  }
  return res;
}

gboolean
gst_wfdtsparse_plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (wfd_ts_parse_debug, "wfdtsparse", 0,
      "Wi-Fi Display transport stream parser");

  return gst_element_register (plugin, "wfdtsparse",
      GST_RANK_NONE, GST_TYPE_WFD_TS_PARSE);
}
