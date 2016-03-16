/*
 * toggle
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: JongHyuk Choi <jhchoi.choi@samsung.com>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdlib.h>

#include "gsttoggle.h"

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

GST_DEBUG_CATEGORY_STATIC (gst_mytoggle_debug);
#define GST_CAT_DEFAULT gst_mytoggle_debug

enum
{
  LAST_SIGNAL
};


#define DEFAULT_BLOCK_DATA        FALSE

enum
{
  PROP_0,
  PROP_BLOCK_DATA

};


#define _do_init(bla) \
    GST_DEBUG_CATEGORY_INIT (gst_mytoggle_debug, "toggle", 0, "toggle element");

G_DEFINE_TYPE_WITH_CODE (GstMytoggle, gst_mytoggle,
    GST_TYPE_BASE_TRANSFORM, _do_init(G_TYPE_INVALID));

static void gst_mytoggle_finalize (GObject * object);
static void gst_mytoggle_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_mytoggle_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GstFlowReturn gst_mytoggle_transform_ip (GstBaseTransform * trans,
    GstBuffer * buf);
static gboolean gst_mytoggle_start (GstBaseTransform * trans);
static gboolean gst_mytoggle_stop (GstBaseTransform * trans);

#if 0
static void
gst_mytoggle_base_init (gpointer g_class)
{
}
#endif

static void
gst_mytoggle_finalize (GObject * object)
{
  G_OBJECT_CLASS (gst_mytoggle_parent_class)->finalize (object);
}

static void
gst_mytoggle_class_init (GstMytoggleClass * klass)
{
  GObjectClass *gobject_class;
  GstBaseTransformClass *gstbasetrans_class;
  GstElementClass *gstelement_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gstbasetrans_class = GST_BASE_TRANSFORM_CLASS (klass);
  gstelement_class = GST_ELEMENT_CLASS (klass);

  gobject_class->set_property = GST_DEBUG_FUNCPTR (gst_mytoggle_set_property);
  gobject_class->get_property = GST_DEBUG_FUNCPTR (gst_mytoggle_get_property);

  g_object_class_install_property (gobject_class,
      PROP_BLOCK_DATA, g_param_spec_boolean ("block_data",
          "Data Block",
          "Data Block",
          DEFAULT_BLOCK_DATA, G_PARAM_READWRITE));

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_mytoggle_finalize);

  gst_element_class_set_details_simple (gstelement_class,
      "toggle",
      "Generic",
      "Pass data without modification", "Rahul Mittal <mittal.rahul@samsung.com>");
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&srctemplate));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sinktemplate));

  gstbasetrans_class->transform_ip =
      GST_DEBUG_FUNCPTR (gst_mytoggle_transform_ip);
  gstbasetrans_class->start = GST_DEBUG_FUNCPTR (gst_mytoggle_start);
  gstbasetrans_class->stop = GST_DEBUG_FUNCPTR (gst_mytoggle_stop);
}

static void
gst_mytoggle_init (GstMytoggle * mytoggle)
{
  gst_base_transform_set_passthrough (GST_BASE_TRANSFORM (mytoggle), TRUE);

  mytoggle->block_data = DEFAULT_BLOCK_DATA;

}

static GstFlowReturn
gst_mytoggle_transform_ip (GstBaseTransform * trans, GstBuffer * buf)
{
  GstMytoggle *mytoggle = GST_MYTOGGLE (trans);

  if (mytoggle->block_data ==TRUE)
      return GST_BASE_TRANSFORM_FLOW_DROPPED;

  return GST_FLOW_OK;
}

static void
gst_mytoggle_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstMytoggle *mytoggle;

  mytoggle = GST_MYTOGGLE (object);

  switch (prop_id) {

    case PROP_BLOCK_DATA:
      mytoggle->block_data = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_mytoggle_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstMytoggle *mytoggle;

  mytoggle = GST_MYTOGGLE (object);

  switch (prop_id) {
      case PROP_BLOCK_DATA:
      g_value_set_boolean (value, mytoggle->block_data);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_mytoggle_start (GstBaseTransform * trans)
{
  return TRUE;
}

static gboolean
gst_mytoggle_stop (GstBaseTransform * trans)
{
  return TRUE;
}


static gboolean plugin_init(GstPlugin *plugin)
{

	GST_DEBUG_CATEGORY_INIT (gst_mytoggle_debug, "toggle",0, "toggle");
	return gst_element_register (plugin, "toggle",  GST_RANK_NONE, GST_TYPE_MYTOGGLE);
}
GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
	GST_VERSION_MINOR,
	toggle,
	"Base transform plugin template",
	plugin_init, VERSION, "LGPL", "Samsung Electronics Co", "http://www.samsung.com/")


