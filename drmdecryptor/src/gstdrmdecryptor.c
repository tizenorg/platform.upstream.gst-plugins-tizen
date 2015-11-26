/*
 * drmdecryptor
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd. All rights reserved.
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
#include <config.h>
#endif

#include <gst/gst.h>
#include <gst/gstelement.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#include "gstdrmdecryptor.h"

/* TODO replace with input caps */
#define GST_STATIC_CAPS_SINK GST_STATIC_CAPS("application/encrypted-xxx")

static GstStaticPadTemplate sinktemplate =
    GST_STATIC_PAD_TEMPLATE ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
    GST_STATIC_CAPS_SINK);

static GstStaticPadTemplate srctemplate =
    GST_STATIC_PAD_TEMPLATE ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

GST_DEBUG_CATEGORY_STATIC (gst_drm_decryptor_debug);
#define GST_CAT_DEFAULT gst_drm_decryptor_debug
static void
_do_init (void)
{
  /* TODO change to the debug category of your element */
  GST_DEBUG_CATEGORY_INIT (gst_drm_decryptor_debug, "DrmDecryptor", 0,
      "DrmDecryptor element");
}

static gboolean
drmdecryptor_init (GstPlugin * drm)
{
  /* TODO change to the name of your element */
  return gst_element_register (drm, "drm_decryptor", GST_RANK_PRIMARY,
      GST_DRM_DECRYPTOR_TYPE);
}

#define gst_drm_decryptor_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstDrmDecryptor, gst_drm_decryptor, GST_TYPE_ELEMENT,
    _do_init ());


static gboolean gst_drm_decryptor_handle_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static GstStateChangeReturn gst_drm_decryptor_change_state (GstElement * element,
    GstStateChange transition);

static void gst_drm_decryptor_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_drm_decryptor_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_drm_decryptor_finalize (GObject * object);

static GstFlowReturn gst_drm_decryptor_chain (GstPad * pad, GstObject * parent, GstBuffer * buffer);

static void
gst_drm_decryptor_class_init (GstDrmDecryptorClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sinktemplate));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&srctemplate));

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_drm_decryptor_change_state);

  gobject_class->finalize = gst_drm_decryptor_finalize;
  gobject_class->set_property = gst_drm_decryptor_set_property;
  gobject_class->get_property = gst_drm_decryptor_get_property;

  /* TODO install any needed properties */

  gst_element_class_set_static_metadata (gstelement_class,
      "drm decryptor template plugin",
      "DRM/Decryptor", "Decryption Plugin", "xxx@samsung.com>");
}

static void
gst_drm_decryptor_init (GstDrmDecryptor * drm)
{
  drm->sinkpad = gst_pad_new_from_static_template (&sinktemplate, "sink");
  drm->srcpad = gst_pad_new_from_static_template (&srctemplate, "src");
  gst_pad_set_chain_function (drm->sinkpad, gst_drm_decryptor_chain);
  gst_pad_set_event_function (drm->sinkpad, gst_drm_decryptor_handle_sink_event);
  gst_pad_set_active (drm->sinkpad, TRUE);
  gst_pad_set_active (drm->srcpad, TRUE);
  GST_PAD_SET_ACCEPT_TEMPLATE (drm->sinkpad);
  gst_element_add_pad (GST_ELEMENT (drm), drm->sinkpad);
  gst_element_add_pad (GST_ELEMENT (drm), drm->srcpad);

  /* TODO initialize your element's data */
}

static gboolean
gst_drm_decryptor_set_format (GstDrmDecryptor * drm, const GstCaps * caps)
{
  GstCaps *output_caps;
  const GstStructure *structure;
  const gchar *original_type;
  gboolean ret;

  structure = gst_caps_get_structure (caps, 0);

  /* It is also possible that we store a full caps inside the caps, in this
   * case, it would be better to name it 'original-media-caps' */
  original_type = gst_structure_get_string (structure, "original-media-type");

  /* TODO get any other needed parameter from caps */

  /* TODO If possible, set the output caps */
  output_caps = gst_caps_new_simple (original_type, NULL);

  ret = gst_pad_set_caps (drm->srcpad, output_caps);
  gst_caps_unref (output_caps);

  return ret;
}

static void
gst_drm_decryptor_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  switch (prop_id) {
    /* TODO fill with properties */
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_drm_decryptor_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  switch (prop_id) {
    /* TODO fill with properties */
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_drm_decryptor_open (GstDrmDecryptor * drm)
{
  /* TODO Open any resources / library that your element requires
   * If none is needed, just leave this as blank or remove this function */
}

static void
gst_drm_decryptor_close (GstDrmDecryptor * drm)
{
  /* TODO close resources / library
   * (should be symetrical to _open() */
}

static gboolean
gst_drm_decryptor_start (GstDrmDecryptor * drm)
{
  /* TODO Start your decryption engine, after this call it must be ready
   * for keys configuration and starting to decrypt. */
}

static void
gst_drm_decryptor_stop (GstDrmDecryptor * drm)
{
  /* TODO Stop the decryption engine (symetrical to _start()) */
}

static GstStateChangeReturn
gst_drm_decryptor_change_state (GstElement * element, GstStateChange transition)
{
  GstDrmDecryptor *drm = (GstDrmDecryptor *) element;
  GstStateChangeReturn result = GST_STATE_CHANGE_FAILURE;

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (!gst_drm_decryptor_open (drm))
        result = GST_STATE_CHANGE_FAILURE;
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      if (!gst_drm_decryptor_start (drm))
        result = GST_STATE_CHANGE_FAILURE;
      break;
    default:
      break;
  }
  if (result == GST_STATE_CHANGE_FAILURE)
    return result;

  result = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (result == GST_STATE_CHANGE_FAILURE)
    return result;

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_NULL:
      gst_drm_decryptor_close (drm);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_drm_decryptor_stop (drm);
      break;
  }

  return result;
}

static gboolean
gst_drm_decryptor_finish (GstDrmDecryptor * drm)
{
  /* TODO finish decryption of any pending data and push it downstream */
  return TRUE;
}

static void
gst_drm_decryptor_flush (GstDrmDecryptor * drm)
{
 /* TODO Flush any data in the decryptor */
}

static gboolean
gst_drm_decryptor_handle_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstDrmDecryptor *drm;
  gboolean ret = TRUE;

  drm = GST_DRM_DECRYPTOR (parent);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_PROTECTION:
    {
      const gchar *system_id;
      GstBuffer *data;
      const gchar *origin;

      gst_event_parse_protection (event, &system_id, &data, &origin);

      /* TODO the protection event contains the keys for decryption that
       * upstream elements might have found
       *
       * The system_id contains information about the type of encryption
       * data contains binary data needed for decryption (e.g. keys)
       * origin is a string describing where this data was found (e.g. mpd,
       * playlist, container ...)
       *
       * This data should be used as parameters for the decryptor
       *
       * In some situations multiple keys are needed, like AES128 that needs
       * the IV and the key. Multiple protection events can be used to signal
       * different decryption configuration parameters. It might also
       * be possible to extend the protection event to hold a structure to
       * contain all needed configuration in a single event but this is not
       * yet implemented.
       *
       * For now, one could (ab)use system_id to be AES128:key and AES128:IV
       * in 2 separate events to signal the parameters.
       */
      break;
    }
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;
      gst_event_parse_caps (event, &caps);
      ret = gst_drm_decryptor_set_format (drm, caps);
      break;
    }
    case GST_EVENT_EOS:
    {
      gst_drm_decryptor_finish (drm);
      ret = gst_pad_push_event (drm->srcpad, event);
      break;
    }
    case GST_EVENT_FLUSH_STOP:
    {
      gst_drm_decryptor_flush (drm);
      ret = gst_pad_push_event (drm->srcpad, event);
      break;
    }
    default:
    {
      ret = gst_pad_event_default (drm->sinkpad, parent, event);
      break;
    }
  }

  return ret;
}

static GstFlowReturn
gst_drm_decryptor_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  GstDrmDecryptor *drm;
  GstBuffer *out_buf = NULL;

  drm = GST_DRM_DECRYPTOR (parent);

  /* TODO do your decryption that will create output data into
   * out_buf */

  /* TODO it might be required to signal per-buffer decryption information.
   * For this purpose, the protection meta can be used and attached to buffers.
   * If your element supports this, it can do:
   *
   * protection_meta = gst_buffer_get_meta (buf, GST_PROTECTION_META_INFO);
   *
   * The protection meta has a structure containing cryptographic parameters
   * for this buffer
   */

  return gst_pad_push (drm->srcpad, out_buf);

}

static void
gst_drm_decryptor_finalize (GObject * object)
{
  /* TODO final instance cleanup */
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR, GST_VERSION_MINOR, "drmdecryptordecrypt",
    "Gstreamer DrmDecryptor plugin", drmdecryptor_init, VERSION, "Some License",
    "gst-plugins-drmdecryptor", "Unknown package origin")
