/* GStreamer Wayland video sink
 *
 * Copyright (C) 2011 Intel Corporation
 * Copyright (C) 2011 Sreerenj Balachandran <sreerenj.balachandran@intel.com>
 * Copyright (C) 2012 Wim Taymans <wim.taymans@gmail.com>
 * Copyright (C) 2014 Collabora Ltd.
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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 */

/**
 * SECTION:element-waylandsink
 *
 *  The waylandsink is creating its own window and render the decoded video frames to that.
 *  Setup the Wayland environment as described in
 *  <ulink url="http://wayland.freedesktop.org/building.html">Wayland</ulink> home page.
 *  The current implementaion is based on weston compositor.
 *
 * <refsect2>
 * <title>Example pipelines</title>
 * |[
 * gst-launch -v videotestsrc ! waylandsink
 * ]| test the video rendering in wayland
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstwaylandsink.h"
#ifdef GST_WLSINK_ENHANCEMENT
#include <mm_types.h>
#include "tizen-wlvideoformat.h"
#else
#include "wlvideoformat.h"
#endif
#include "waylandpool.h"

#include <gst/wayland/wayland.h>
#include <gst/video/videooverlay.h>

#ifdef GST_WLSINK_ENHANCEMENT
#define GST_TYPE_WAYLANDSINK_DISPLAY_GEOMETRY_METHOD (gst_waylandsink_display_geometry_method_get_type())
#define GST_TYPE_WAYLANDSINK_ROTATE_ANGLE (gst_waylandsink_rotate_angle_get_type())
#define GST_TYPE_WAYLANDSINK_FLIP (gst_waylandsink_flip_get_type())

static GType
gst_waylandsink_rotate_angle_get_type (void)
{
  static GType waylandsink_rotate_angle_type = 0;
  static const GEnumValue rotate_angle_type[] = {
    {0, "No rotate", "DEGREE_0"},
    {1, "Rotate 90 degree", "DEGREE_90"},
    {2, "Rotate 180 degree", "DEGREE_180"},
    {3, "Rotate 270 degree", "DEGREE_270"},
    {4, NULL, NULL},
  };

  if (!waylandsink_rotate_angle_type) {
    waylandsink_rotate_angle_type =
        g_enum_register_static ("GstWaylandSinkRotateAngleType",
        rotate_angle_type);
  }

  return waylandsink_rotate_angle_type;
}


static GType
gst_waylandsink_display_geometry_method_get_type (void)
{
  static GType waylandsink_display_geometry_method_type = 0;
  static const GEnumValue display_geometry_method_type[] = {
    {0, "Letter box", "LETTER_BOX"},
    {1, "Origin size", "ORIGIN_SIZE"},
    {2, "Full-screen", "FULL_SCREEN"},
    {3, "Cropped full-screen", "CROPPED_FULL_SCREEN"},
    {4, "Origin size(if screen size is larger than video size(width/height)) or Letter box(if video size(width/height) is larger than screen size)", "ORIGIN_SIZE_OR_LETTER_BOX"},
    {5, NULL, NULL},
  };

  if (!waylandsink_display_geometry_method_type) {
    waylandsink_display_geometry_method_type =
        g_enum_register_static ("GstWaylandSinkDisplayGeometryMethodType",
        display_geometry_method_type);
  }
  return waylandsink_display_geometry_method_type;
}

static GType
gst_waylandsink_flip_get_type (void)
{
  static GType waylandsink_flip_type = 0;
  static const GEnumValue flip_type[] = {
    {FLIP_NONE, "Flip NONE", "FLIP_NONE"},
    {FLIP_HORIZONTAL, "Flip HORIZONTAL", "FLIP_HORIZONTAL"},
    {FLIP_VERTICAL, "Flip VERTICAL", "FLIP_VERTICAL"},
    {FLIP_BOTH, "Flip BOTH", "FLIP_BOTH"},
    {FLIP_NUM, NULL, NULL},
  };

  if (!waylandsink_flip_type) {
    waylandsink_flip_type =
        g_enum_register_static ("GstWaylandSinkFlipType", flip_type);
  }

  return waylandsink_flip_type;
}

#endif


/* signals */
enum
{
  SIGNAL_0,
  LAST_SIGNAL
};

/* Properties */
enum
{
  PROP_0,
  PROP_DISPLAY,
#ifdef GST_WLSINK_ENHANCEMENT
  PROP_ROTATE_ANGLE,
  PROP_DISPLAY_GEOMETRY_METHOD,
  PROP_ORIENTATION,
  PROP_FLIP
#endif
};

GST_DEBUG_CATEGORY (gsttizenwl_debug);
#define GST_CAT_DEFAULT gsttizenwl_debug

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE
        ("{ BGRx, BGRA, RGBx, xBGR, xRGB, RGBA, ABGR, ARGB, RGB, BGR, "
            "RGB16, BGR16, YUY2, YVYU, UYVY, AYUV, NV12, NV21, NV16, "
#ifdef GST_WLSINK_ENHANCEMENT
            "SN12, ST12, "
#endif
            "YUV9, YVU9, Y41B, I420, YV12, Y42B, v308 }"))
    );

static void gst_wayland_sink_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);
static void gst_wayland_sink_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_wayland_sink_finalize (GObject * object);

static GstStateChangeReturn gst_wayland_sink_change_state (GstElement * element,
    GstStateChange transition);
static void gst_wayland_sink_set_context (GstElement * element,
    GstContext * context);

static GstCaps *gst_wayland_sink_get_caps (GstBaseSink * bsink,
    GstCaps * filter);
static gboolean gst_wayland_sink_set_caps (GstBaseSink * bsink, GstCaps * caps);
static gboolean gst_wayland_sink_preroll (GstBaseSink * bsink,
    GstBuffer * buffer);
static gboolean
gst_wayland_sink_propose_allocation (GstBaseSink * bsink, GstQuery * query);
static gboolean gst_wayland_sink_render (GstBaseSink * bsink,
    GstBuffer * buffer);

/* VideoOverlay interface */
static void gst_wayland_sink_videooverlay_init (GstVideoOverlayInterface *
    iface);
static void gst_wayland_sink_set_window_handle (GstVideoOverlay * overlay,
    guintptr handle);
static void gst_wayland_sink_set_render_rectangle (GstVideoOverlay * overlay,
    gint x, gint y, gint w, gint h);
static void gst_wayland_sink_expose (GstVideoOverlay * overlay);

/* WaylandVideo interface */
static void gst_wayland_sink_waylandvideo_init (GstWaylandVideoInterface *
    iface);
static void gst_wayland_sink_begin_geometry_change (GstWaylandVideo * video);
static void gst_wayland_sink_end_geometry_change (GstWaylandVideo * video);
#ifdef GST_WLSINK_ENHANCEMENT
static void gst_wayland_sink_update_window_geometry (GstTizenwlSink * sink);
static void render_last_buffer (GstTizenwlSink * sink);
static void gst_wayland_sink_render_last_buffer (GstTizenwlSink * sink);

#endif

#define gst_wayland_sink_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstTizenwlSink, gst_wayland_sink, GST_TYPE_VIDEO_SINK,
    G_IMPLEMENT_INTERFACE (GST_TYPE_VIDEO_OVERLAY,
        gst_wayland_sink_videooverlay_init)
    G_IMPLEMENT_INTERFACE (GST_TYPE_WAYLAND_VIDEO,
        gst_wayland_sink_waylandvideo_init));

static void
gst_wayland_sink_class_init (GstTizenwlSinkClass * klass)
{
  FUNCTION_ENTER ();
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSinkClass *gstbasesink_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbasesink_class = (GstBaseSinkClass *) klass;

  gobject_class->set_property = gst_wayland_sink_set_property;
  gobject_class->get_property = gst_wayland_sink_get_property;
  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_wayland_sink_finalize);

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_template));

  gst_element_class_set_static_metadata (gstelement_class,
      "wayland video sink", "Sink/Video",
      "Output to wayland surface",
      "Sreerenj Balachandran <sreerenj.balachandran@intel.com>, "
      "George Kiagiadakis <george.kiagiadakis@collabora.com>");

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_wayland_sink_change_state);
  gstelement_class->set_context =
      GST_DEBUG_FUNCPTR (gst_wayland_sink_set_context);

  gstbasesink_class->get_caps = GST_DEBUG_FUNCPTR (gst_wayland_sink_get_caps);
  gstbasesink_class->set_caps = GST_DEBUG_FUNCPTR (gst_wayland_sink_set_caps);
  gstbasesink_class->preroll = GST_DEBUG_FUNCPTR (gst_wayland_sink_preroll);
  gstbasesink_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_wayland_sink_propose_allocation);
  gstbasesink_class->render = GST_DEBUG_FUNCPTR (gst_wayland_sink_render);

  g_object_class_install_property (gobject_class, PROP_DISPLAY,
      g_param_spec_string ("display", "Wayland Display name", "Wayland "
          "display name to connect to, if not supplied via the GstContext",
          NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
#ifdef GST_WLSINK_ENHANCEMENT
  g_object_class_install_property (gobject_class, PROP_ROTATE_ANGLE,
      g_param_spec_enum ("rotate", "Rotate angle",
          "Rotate angle of display output",
          GST_TYPE_WAYLANDSINK_ROTATE_ANGLE, DEGREE_0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_DISPLAY_GEOMETRY_METHOD,
      g_param_spec_enum ("display-geometry-method", "Display geometry method",
          "Geometrical method for display",
          GST_TYPE_WAYLANDSINK_DISPLAY_GEOMETRY_METHOD,
          DEF_DISPLAY_GEOMETRY_METHOD,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_ORIENTATION,
      g_param_spec_enum ("orientation",
          "Orientation information used for ROI/ZOOM",
          "Orientation information for display",
          GST_TYPE_WAYLANDSINK_ROTATE_ANGLE, DEGREE_0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_FLIP,
      g_param_spec_enum ("flip", "Display flip",
          "Flip for display",
          GST_TYPE_WAYLANDSINK_FLIP, DEF_DISPLAY_FLIP,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

#endif
}

static void
gst_wayland_sink_init (GstTizenwlSink * sink)
{
  FUNCTION_ENTER ();

  sink->display_geometry_method = DEF_DISPLAY_GEOMETRY_METHOD;
  sink->flip = DEF_DISPLAY_FLIP;
  sink->rotate_angle = DEGREE_0;
  sink->orientation = DEGREE_0;

  g_mutex_init (&sink->display_lock);
  g_mutex_init (&sink->render_lock);
}

static void
gst_wayland_sink_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  FUNCTION_ENTER ();

  GstTizenwlSink *sink = GST_WAYLAND_SINK (object);
  switch (prop_id) {
    case PROP_DISPLAY:
      GST_OBJECT_LOCK (sink);
      g_value_set_string (value, sink->display_name);
      GST_OBJECT_UNLOCK (sink);
      break;
#ifdef GST_WLSINK_ENHANCEMENT
    case PROP_ROTATE_ANGLE:
      g_value_set_enum (value, sink->rotate_angle);
      break;
    case PROP_DISPLAY_GEOMETRY_METHOD:
      g_value_set_enum (value, sink->display_geometry_method);
      break;
    case PROP_ORIENTATION:
      g_value_set_enum (value, sink->orientation);
      break;
    case PROP_FLIP:
      g_value_set_enum (value, sink->flip);
      break;
#endif
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_wayland_sink_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  FUNCTION_ENTER ();

  GstTizenwlSink *sink = GST_WAYLAND_SINK (object);
  switch (prop_id) {
    case PROP_DISPLAY:
      GST_OBJECT_LOCK (sink);
      sink->display_name = g_value_dup_string (value);
      GST_OBJECT_UNLOCK (sink);
      break;
#ifdef GST_WLSINK_ENHANCEMENT
    case PROP_ROTATE_ANGLE:
      sink->rotate_angle = g_value_get_enum (value);
      GST_INFO_OBJECT (sink, "Rotate angle is set (%d)", sink->rotate_angle);
      if (sink->window) {
        gst_wl_window_set_rotate_angle (sink->window, sink->rotate_angle);
      }
      sink->video_info_changed = TRUE;
      if (GST_STATE (sink) == GST_STATE_PAUSED) {
        /*need to render last buffer */
        gst_wayland_sink_render_last_buffer (sink);
      }
      break;
    case PROP_DISPLAY_GEOMETRY_METHOD:
      sink->display_geometry_method = g_value_get_enum (value);
      GST_INFO_OBJECT (sink, "Display geometry method is set (%d)",
          sink->display_geometry_method);
      if (sink->window) {
        gst_wl_window_set_disp_geo_method (sink->window,
            sink->display_geometry_method);
      }
      sink->video_info_changed = TRUE;
      if (GST_STATE (sink) == GST_STATE_PAUSED) {
        /*need to render last buffer */
        gst_wayland_sink_render_last_buffer (sink);
      }
      break;
    case PROP_ORIENTATION:
      sink->orientation = g_value_get_enum (value);
      GST_INFO_OBJECT (sink, "Orientation is set (%d)", sink->orientation);
      if (sink->window) {
        gst_wl_window_set_orientation (sink->window, sink->orientation);
      }
      sink->video_info_changed = TRUE;
      if (GST_STATE (sink) == GST_STATE_PAUSED) {
        /*need to render last buffer */
        gst_wayland_sink_render_last_buffer (sink);
      }
      break;
    case PROP_FLIP:
      sink->flip = g_value_get_enum (value);
      GST_INFO_OBJECT (sink, "flip is set (%d)", sink->flip);
      if (sink->flip) {
        gst_wl_window_set_flip (sink->window, sink->flip);
      }
      sink->video_info_changed = TRUE;
      if (GST_STATE (sink) == GST_STATE_PAUSED) {
        /*need to render last buffer */
        gst_wayland_sink_render_last_buffer (sink);
      }
      break;
#endif
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_wayland_sink_finalize (GObject * object)
{
  FUNCTION_ENTER ();

  GstTizenwlSink *sink = GST_WAYLAND_SINK (object);
  GST_DEBUG_OBJECT (sink, "Finalizing the sink..");

  if (sink->last_buffer)
    gst_buffer_unref (sink->last_buffer);
  if (sink->display) {
    /* see comment about this call in gst_wayland_sink_change_state() */
#ifdef GST_WLSINK_ENHANCEMENT
    if (sink->pool && !sink->display->is_native_format)
#else
    if (sink->pool)
#endif
      gst_wayland_compositor_release_all_buffers (GST_WAYLAND_BUFFER_POOL
          (sink->pool));

    g_object_unref (sink->display);
    sink->display = NULL;
  }
  if (sink->window) {
    g_object_unref (sink->window);
    sink->window = NULL;
  }
  if (sink->pool) {
    gst_object_unref (sink->pool);
    sink->pool = NULL;
  }

  if (sink->display_name) {
    g_free (sink->display_name);
    sink->display_name = NULL;
  }

  g_mutex_clear (&sink->display_lock);
  g_mutex_clear (&sink->render_lock);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/* must be called with the display_lock */
static void
gst_wayland_sink_set_display_from_context (GstTizenwlSink * sink,
    GstContext * context)
{
  FUNCTION_ENTER ();

  struct wl_display *display;
  GError *error = NULL;

  display = gst_wayland_display_handle_context_get_handle (context);
  sink->display = gst_wl_display_new_existing (display, FALSE, &error);

  if (error) {
    GST_ELEMENT_WARNING (sink, RESOURCE, OPEN_READ_WRITE,
        ("Could not set display handle"),
        ("Failed to use the external wayland display: '%s'", error->message));
    g_error_free (error);
  }
}

static gboolean
gst_wayland_sink_find_display (GstTizenwlSink * sink)
{
  FUNCTION_ENTER ();

  GstQuery *query;
  GstMessage *msg;
  GstContext *context = NULL;
  GError *error = NULL;
  gboolean ret = TRUE;

  g_mutex_lock (&sink->display_lock);

  if (!sink->display) {
    /* first query upstream for the needed display handle */
    query = gst_query_new_context (GST_WAYLAND_DISPLAY_HANDLE_CONTEXT_TYPE);
    if (gst_pad_peer_query (GST_VIDEO_SINK_PAD (sink), query)) {
      gst_query_parse_context (query, &context);
      gst_wayland_sink_set_display_from_context (sink, context);
    }
    gst_query_unref (query);

    if (G_LIKELY (!sink->display)) {
      /* now ask the application to set the display handle */
      msg = gst_message_new_need_context (GST_OBJECT_CAST (sink),
          GST_WAYLAND_DISPLAY_HANDLE_CONTEXT_TYPE);

      g_mutex_unlock (&sink->display_lock);
      gst_element_post_message (GST_ELEMENT_CAST (sink), msg);
      /* at this point we expect gst_wayland_sink_set_context
       * to get called and fill sink->display */
      g_mutex_lock (&sink->display_lock);

      if (!sink->display) {
        /* if the application didn't set a display, let's create it ourselves */
        GST_OBJECT_LOCK (sink);
        sink->display = gst_wl_display_new (sink->display_name, &error);
        GST_OBJECT_UNLOCK (sink);

        if (error) {
          GST_ELEMENT_WARNING (sink, RESOURCE, OPEN_READ_WRITE,
              ("Could not initialise Wayland output"),
              ("Failed to create GstWlDisplay: '%s'", error->message));
          g_error_free (error);
          ret = FALSE;
        } else {
          /* inform the world about the new display */
          context =
              gst_wayland_display_handle_context_new (sink->display->display);
          msg = gst_message_new_have_context (GST_OBJECT_CAST (sink), context);
          gst_element_post_message (GST_ELEMENT_CAST (sink), msg);
        }
      }
    }
  }

  g_mutex_unlock (&sink->display_lock);

  return ret;
}

static GstStateChangeReturn
gst_wayland_sink_change_state (GstElement * element, GstStateChange transition)
{
  FUNCTION_ENTER ();

  GstTizenwlSink *sink = GST_WAYLAND_SINK (element);
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (!gst_wayland_sink_find_display (sink))
        return GST_STATE_CHANGE_FAILURE;
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_buffer_replace (&sink->last_buffer, NULL);
      if (sink->window) {
        if (gst_wl_window_is_toplevel (sink->window)) {
          g_clear_object (&sink->window);
        } else {
          /* remove buffer from surface, show nothing */
          wl_surface_attach (sink->window->surface, NULL, 0, 0);
          wl_surface_damage (sink->window->surface, 0, 0,
              sink->window->surface_width, sink->window->surface_height);
          wl_surface_commit (sink->window->surface);
          wl_display_flush (sink->display->display);
        }
      }
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      g_mutex_lock (&sink->display_lock);
      /* If we had a toplevel window, we most likely have our own connection
       * to the display too, and it is a good idea to disconnect and allow
       * potentially the application to embed us with GstVideoOverlay
       * (which requires to re-use the same display connection as the parent
       * surface). If we didn't have a toplevel window, then the display
       * connection that we have is definitely shared with the application
       * and it's better to keep it around (together with the window handle)
       * to avoid requesting them again from the application if/when we are
       * restarted (GstVideoOverlay behaves like that in other sinks)
       */
      if (sink->display && !sink->window) {     /* -> the window was toplevel */
        /* Force all buffers to return to the pool, regardless of
         * whether the compositor has released them or not. We are
         * going to kill the display, so we need to return all buffers
         * to be destroyed before this happens.
         * Note that this is done here instead of the pool destructor
         * because the buffers hold a reference to the pool. Also,
         * the buffers can only be unref'ed from the display's event loop
         * and the pool holds a reference to the display. If we drop
         * our references here, when the compositor releases the buffers,
         * they will be unref'ed from the event loop thread, which will
         * unref the pool and therefore the display, which will try to
         * stop the thread from within itself and cause a deadlock.
         */
        if (sink->pool) {
          gst_wayland_compositor_release_all_buffers (GST_WAYLAND_BUFFER_POOL
              (sink->pool));
        }
        g_clear_object (&sink->display);
        g_clear_object (&sink->pool);
      }
      g_mutex_unlock (&sink->display_lock);
      break;
    default:
      break;
  }

  return ret;
}

static void
gst_wayland_sink_set_context (GstElement * element, GstContext * context)
{
  FUNCTION_ENTER ();

  GstTizenwlSink *sink = GST_WAYLAND_SINK (element);
  if (gst_context_has_context_type (context,
          GST_WAYLAND_DISPLAY_HANDLE_CONTEXT_TYPE)) {
    g_mutex_lock (&sink->display_lock);
    if (G_LIKELY (!sink->display)) {
      gst_wayland_sink_set_display_from_context (sink, context);
    } else {
      GST_WARNING_OBJECT (element, "changing display handle is not supported");
      g_mutex_unlock (&sink->display_lock);
      return;
    }
    g_mutex_unlock (&sink->display_lock);
  }

  GST_INFO ("element %p context %p", element, context);
  if (GST_ELEMENT_CLASS (parent_class)->set_context)
    GST_ELEMENT_CLASS (parent_class)->set_context (element, context);
}

static GstCaps *
gst_wayland_sink_get_caps (GstBaseSink * bsink, GstCaps * filter)
{
  FUNCTION_ENTER ();

  GstTizenwlSink *sink;
  GstCaps *caps;
  sink = GST_WAYLAND_SINK (bsink);

  caps = gst_pad_get_pad_template_caps (GST_VIDEO_SINK_PAD (sink));

  g_mutex_lock (&sink->display_lock);

  if (sink->display) {
    GValue list = G_VALUE_INIT;
    GValue value = G_VALUE_INIT;
    GArray *formats;
    gint i;
#ifdef GST_WLSINK_ENHANCEMENT
    uint32_t fmt;
#else
    enum wl_shm_format fmt;
#endif

    g_value_init (&list, GST_TYPE_LIST);
    g_value_init (&value, G_TYPE_STRING);

    formats = sink->display->formats;
    for (i = 0; i < formats->len; i++) {
      fmt = g_array_index (formats, uint32_t, i);
      g_value_set_string (&value, gst_wayland_format_to_string (fmt));
      gst_value_list_append_value (&list, &value);
#ifdef GST_WLSINK_ENHANCEMENT
      /* TBM doesn't support SN12. So we add SN12 manually as supported format.
       * SN12 is exactly same with NV12.
       */
      if (fmt == TBM_FORMAT_NV12) {
        g_value_set_string (&value,
            gst_video_format_to_string (GST_VIDEO_FORMAT_SN12));
        gst_value_list_append_value (&list, &value);
      }
#endif
    }

    caps = gst_caps_make_writable (caps);
    gst_structure_set_value (gst_caps_get_structure (caps, 0), "format", &list);

    GST_DEBUG_OBJECT (sink, "display caps: %" GST_PTR_FORMAT, caps);
  }

  g_mutex_unlock (&sink->display_lock);

  if (filter) {
    GstCaps *intersection;

    intersection =
        gst_caps_intersect_full (filter, caps, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (caps);
    caps = intersection;
  }

  return caps;
}

static gboolean
gst_wayland_sink_set_caps (GstBaseSink * bsink, GstCaps * caps)
{
  FUNCTION_ENTER ();

  GstTizenwlSink *sink;
  GstBufferPool *newpool;
  GstVideoInfo info;
#ifdef GST_WLSINK_ENHANCEMENT
  uint32_t format;
#else
  enum wl_shm_format format;
#endif
  GArray *formats;
  gint i;
  GstStructure *structure;
  static GstAllocationParams params = { 0, 0, 0, 15, };
  sink = GST_WAYLAND_SINK (bsink);

  GST_DEBUG_OBJECT (sink, "set caps %" GST_PTR_FORMAT, caps);

  /* extract info from caps */
  if (!gst_video_info_from_caps (&info, caps))
    goto invalid_format;
#ifdef GST_WLSINK_ENHANCEMENT
  sink->caps = gst_caps_copy (caps);
#endif

  format = gst_video_format_to_wayland_format (GST_VIDEO_INFO_FORMAT (&info));
  if ((gint) format == -1)
    goto invalid_format;

  /* verify we support the requested format */
  formats = sink->display->formats;
  for (i = 0; i < formats->len; i++) {
    if (g_array_index (formats, uint32_t, i) == format)
      break;
  }

  if (i >= formats->len)
    goto unsupported_format;

#ifdef GST_WLSINK_ENHANCEMENT
  if (GST_VIDEO_INFO_FORMAT (&info) == GST_VIDEO_FORMAT_SN12 ||
      GST_VIDEO_INFO_FORMAT (&info) == GST_VIDEO_FORMAT_ST12) {
    sink->display->is_native_format = TRUE;
  } else {
    sink->display->is_native_format = FALSE;

    /* create a new pool for the new configuration */
    newpool = gst_wayland_buffer_pool_new (sink->display);
    if (!newpool)
      goto pool_failed;

    structure = gst_buffer_pool_get_config (newpool);
    gst_buffer_pool_config_set_params (structure, caps, info.size, 2, 0);
    gst_buffer_pool_config_set_allocator (structure, NULL, &params);
    if (!gst_buffer_pool_set_config (newpool, structure))
      goto config_failed;

    gst_object_replace ((GstObject **) & sink->pool, (GstObject *) newpool);
    gst_object_unref (newpool);

  }
  /* store the video info */
  sink->video_info = info;
  sink->video_info_changed = TRUE;
#else
  /* create a new pool for the new configuration */
  newpool = gst_wayland_buffer_pool_new (sink->display);
  if (!newpool)
    goto pool_failed;

  structure = gst_buffer_pool_get_config (newpool);
  gst_buffer_pool_config_set_params (structure, caps, info.size, 2, 0);
  gst_buffer_pool_config_set_allocator (structure, NULL, &params);
  if (!gst_buffer_pool_set_config (newpool, structure))
    goto config_failed;

  /* store the video info */
  sink->video_info = info;
  sink->video_info_changed = TRUE;

  gst_object_replace ((GstObject **) & sink->pool, (GstObject *) newpool);
  gst_object_unref (newpool);
#endif
  return TRUE;

invalid_format:
  {
    GST_DEBUG_OBJECT (sink,
        "Could not locate image format from caps %" GST_PTR_FORMAT, caps);
    return FALSE;
  }
unsupported_format:
  {
    GST_DEBUG_OBJECT (sink, "Format %s is not available on the display",
        gst_wayland_format_to_string (format));
    return FALSE;
  }
pool_failed:
  {
    GST_DEBUG_OBJECT (sink, "Failed to create new pool");
    return FALSE;
  }
config_failed:
  {
    GST_DEBUG_OBJECT (bsink, "failed setting config");
    gst_object_unref (newpool);
    return FALSE;
  }
}

static gboolean
gst_wayland_sink_propose_allocation (GstBaseSink * bsink, GstQuery * query)
{
  FUNCTION_ENTER ();

  GstTizenwlSink *sink = GST_WAYLAND_SINK (bsink);
  GstBufferPool *pool = NULL;
  GstStructure *config;
  GstCaps *caps;
  guint size;
  gboolean need_pool;

  if (sink->display->is_native_format == TRUE)
    return TRUE;

  gst_query_parse_allocation (query, &caps, &need_pool);

  if (caps == NULL)
    goto no_caps;

  if (sink->pool)
    pool = gst_object_ref (sink->pool);

  if (pool != NULL) {
    GstCaps *pcaps;

    /* we had a pool, check caps */
    config = gst_buffer_pool_get_config (pool);
    gst_buffer_pool_config_get_params (config, &pcaps, &size, NULL, NULL);

    if (!gst_caps_is_equal (caps, pcaps)) {
      /* different caps, we can't use this pool */
      gst_object_unref (pool);
      pool = NULL;
    }
    gst_structure_free (config);
  }

  if (pool == NULL && need_pool) {
    GstVideoInfo info;

    if (!gst_video_info_from_caps (&info, caps))
      goto invalid_caps;

    GST_DEBUG_OBJECT (sink, "create new pool");
    pool = gst_wayland_buffer_pool_new (sink->display);

    /* the normal size of a frame */
    size = info.size;

    config = gst_buffer_pool_get_config (pool);
    gst_buffer_pool_config_set_params (config, caps, size, 2, 0);
    if (!gst_buffer_pool_set_config (pool, config))
      goto config_failed;
  }
  if (pool) {
    gst_query_add_allocation_pool (query, pool, size, 2, 0);
    gst_object_unref (pool);
  }

  return TRUE;

  /* ERRORS */
no_caps:
  {
    GST_DEBUG_OBJECT (bsink, "no caps specified");
    return FALSE;
  }
invalid_caps:
  {
    GST_DEBUG_OBJECT (bsink, "invalid caps specified");
    return FALSE;
  }
config_failed:
  {
    GST_DEBUG_OBJECT (bsink, "failed setting config");
    gst_object_unref (pool);
    return FALSE;
  }
}

static GstFlowReturn
gst_wayland_sink_preroll (GstBaseSink * bsink, GstBuffer * buffer)
{
  FUNCTION_ENTER ();

  GST_DEBUG_OBJECT (bsink, "preroll buffer %p", buffer);
  return gst_wayland_sink_render (bsink, buffer);
}

static void
frame_redraw_callback (void *data, struct wl_callback *callback, uint32_t time)
{
  FUNCTION_ENTER ();

  GstTizenwlSink *sink = data;

  GST_LOG ("frame_redraw_cb");

  g_atomic_int_set (&sink->redraw_pending, FALSE);
  wl_callback_destroy (callback);
}

static const struct wl_callback_listener frame_callback_listener = {
  frame_redraw_callback
};

/* must be called with the render lock */
static void
render_last_buffer (GstTizenwlSink * sink)
{
  FUNCTION_ENTER ();

  GstWlMeta *meta;
  struct wl_surface *surface;
  struct wl_callback *callback;

  meta = gst_buffer_get_wl_meta (sink->last_buffer);
  surface = gst_wl_window_get_wl_surface (sink->window);

  g_atomic_int_set (&sink->redraw_pending, TRUE);
  callback = wl_surface_frame (surface);
  wl_callback_add_listener (callback, &frame_callback_listener, sink);

  /* Here we essentially add a reference to the buffer. This represents
   * the fact that the compositor is using the buffer and it should
   * not return back to the pool and be reused until the compositor
   * releases it. The release is handled internally in the pool */
  gst_wayland_compositor_acquire_buffer (meta->pool, sink->last_buffer);

  GST_DEBUG ("wl_surface_attach wl_buffer %p", meta->wbuffer);

  wl_surface_attach (surface, meta->wbuffer, 0, 0);
  wl_surface_damage (surface, 0, 0, sink->window->surface_width,
      sink->window->surface_height);

  wl_surface_commit (surface);
  wl_display_flush (sink->display->display);
}

static GstFlowReturn
gst_wayland_sink_render (GstBaseSink * bsink, GstBuffer * buffer)
{
  FUNCTION_ENTER ();

  GstTizenwlSink *sink = GST_WAYLAND_SINK (bsink);
  GstBuffer *to_render;
  GstWlMeta *meta;
  GstFlowReturn ret = GST_FLOW_OK;

#ifdef GST_WLSINK_ENHANCEMENT
  GstBufferPool *newpool;
  GstStructure *structure;
  static GstAllocationParams params = { 0, 0, 0, 15, };
#endif

  g_mutex_lock (&sink->render_lock);

  GST_LOG_OBJECT (sink, "render buffer %p", buffer);

  if (G_UNLIKELY (!sink->window)) {
    /* ask for window handle. Unlock render_lock while doing that because
     * set_window_handle & friends will lock it in this context */
    g_mutex_unlock (&sink->render_lock);
    gst_video_overlay_prepare_window_handle (GST_VIDEO_OVERLAY (sink));
    g_mutex_lock (&sink->render_lock);

    if (sink->window) {
      /* inform the window about our caps */
      gst_wl_window_set_video_info (sink->window, &sink->video_info);
    } else {
      /* if we were not provided a window, create one ourselves */
      sink->window =
          gst_wl_window_new_toplevel (sink->display, &sink->video_info);
    }
#ifdef GST_WLSINK_ENHANCEMENT
    gst_wayland_sink_update_window_geometry (sink);
    sink->video_info_changed = TRUE;
#else
    sink->video_info_changed = FALSE;
#endif
  }

  /* drop buffers until we get a frame callback */
  if (g_atomic_int_get (&sink->redraw_pending) == TRUE)
    goto done;

  if (G_UNLIKELY (sink->video_info_changed)) {
    gst_wl_window_set_video_info (sink->window, &sink->video_info);
    sink->video_info_changed = FALSE;
  }
  GST_INFO ("window->render_rectangle(%d,%d %d x %d)",
      sink->window->render_rectangle.x,
      sink->window->render_rectangle.y,
      sink->window->render_rectangle.w, sink->window->render_rectangle.h);
  GST_INFO ("window->surface_width(%d),window->surface_height(%d)",
      sink->window->surface_width, sink->window->surface_height);

  /* now that we have for sure set the video info on the window, it must have
   * a valid size, otherwise this means that the application has called
   * set_window_handle() without calling set_render_rectangle(), which is
   * absolutely necessary for us.
   */
  if (G_UNLIKELY (sink->window->surface_width == 0 ||
          sink->window->surface_height == 0))
    goto no_window_size;

  meta = gst_buffer_get_wl_meta (buffer);

  if (meta && meta->pool->display == sink->display) {
    GST_LOG_OBJECT (sink, "buffer %p from our pool, writing directly", buffer);
    to_render = buffer;
  } else {
    GstMapInfo src;
    GST_LOG_OBJECT (sink, "buffer %p not from our pool, copying", buffer);

#ifdef GST_WLSINK_ENHANCEMENT
    if (sink->display->is_native_format == TRUE) {
      /*in case of SN12 or ST12 video  format */
      GstMemory *mem;
      GstMapInfo mem_info = GST_MAP_INFO_INIT;
      MMVideoBuffer *mm_video_buf = NULL;
	  int i = 0;

      mem = gst_buffer_peek_memory (buffer, 1);
      gst_memory_map (mem, &mem_info, GST_MAP_READ);
      mm_video_buf = (MMVideoBuffer *) mem_info.data;
      gst_memory_unmap (mem, &mem_info);

      if (mm_video_buf == NULL) {
        GST_WARNING_OBJECT (sink, "mm_video_buf is NULL. Skip rendering");
        return ret;
      }
      /* assign mm_video_buf info */
      if (mm_video_buf->type == MM_VIDEO_BUFFER_TYPE_TBM_BO) {
        GST_DEBUG_OBJECT (sink, "TBM bo %p %p %p", mm_video_buf->handle.bo[0],
            mm_video_buf->handle.bo[1], mm_video_buf->handle.bo[2]);

        sink->display->native_video_size = 0;

        for (i = 0; i < NV_BUF_PLANE_NUM; i++) {
          if (mm_video_buf->handle.bo[i] != NULL) {
            sink->display->bo[i] = mm_video_buf->handle.bo[i];
          } else {
            sink->display->bo[i] = 0;
          }
          sink->display->plane_size[i] = mm_video_buf->size[i];
          sink->display->stride_width[i] = mm_video_buf->stride_width[i];
          sink->display->stride_height[i] = mm_video_buf->stride_height[i];
          sink->display->native_video_size += sink->display->plane_size[i];
        }
      } else {
        GST_ERROR_OBJECT (sink, "Buffer type is not TBM");
        return ret;
      }

      if (!sink->pool) {

        /* create a new pool for the new configuration */
        newpool = gst_wayland_buffer_pool_new (sink->display);
        if (!newpool) {
          GST_DEBUG_OBJECT (sink, "Failed to create new pool");
          return FALSE;
        }
        structure = gst_buffer_pool_get_config (newpool);
        /*When the buffer is released, Core compare size with buffer size,
           wl_buffer is not created if the size is same. It is a very critical problem
           So we set 0 to size */
        gst_buffer_pool_config_set_params (structure, sink->caps, 0, 2, 0);
        gst_buffer_pool_config_set_allocator (structure, NULL, &params);
        if (!gst_buffer_pool_set_config (newpool, structure)) {
          GST_DEBUG_OBJECT (bsink, "failed setting config");
          gst_object_unref (newpool);
          return FALSE;
        }

        gst_object_replace ((GstObject **) & sink->pool, (GstObject *) newpool);
        gst_object_unref (newpool);

      }

      if (!gst_buffer_pool_set_active (sink->pool, TRUE))
        goto activate_failed;

      ret = gst_buffer_pool_acquire_buffer (sink->pool, &to_render, NULL);
      if (ret != GST_FLOW_OK)
        goto no_buffer;


      /*add displaying buffer */
      GstWlMeta *meta;
      meta = gst_buffer_get_wl_meta (to_render);
      gst_wayland_buffer_pool_add_displaying_buffer (sink->pool, meta, buffer);

    } else {
      /*in case of normal video format and pool is not our pool */

      if (!sink->pool)
        goto no_pool;

      if (!gst_buffer_pool_set_active (sink->pool, TRUE))
        goto activate_failed;

      ret = gst_buffer_pool_acquire_buffer (sink->pool, &to_render, NULL);
      if (ret != GST_FLOW_OK)
        goto no_buffer;

      gst_buffer_map (buffer, &src, GST_MAP_READ);
      gst_buffer_fill (to_render, 0, src.data, src.size);
      gst_buffer_unmap (buffer, &src);
    }
#else
    if (!sink->pool)
      goto no_pool;

    if (!gst_buffer_pool_set_active (sink->pool, TRUE))
      goto activate_failed;

    ret = gst_buffer_pool_acquire_buffer (sink->pool, &to_render, NULL);
    if (ret != GST_FLOW_OK)
      goto no_buffer;

    gst_buffer_map (buffer, &src, GST_MAP_READ);
    gst_buffer_fill (to_render, 0, src.data, src.size);
    gst_buffer_unmap (buffer, &src);
#endif
  }

  gst_buffer_replace (&sink->last_buffer, to_render);
  render_last_buffer (sink);

  if (buffer != to_render) {
    GST_LOG_OBJECT (sink, "Decrease ref count of buffer");
    gst_buffer_unref (to_render);
  }
  goto done;

no_window_size:
  {
    GST_ELEMENT_ERROR (sink, RESOURCE, WRITE,
        ("Window has no size set"),
        ("Make sure you set the size after calling set_window_handle"));
    ret = GST_FLOW_ERROR;
    goto done;
  }
no_buffer:
  {
    GST_WARNING_OBJECT (sink, "could not create image");
    goto done;
  }
no_pool:
  {
    GST_ELEMENT_ERROR (sink, RESOURCE, WRITE,
        ("Internal error: can't allocate images"),
        ("We don't have a bufferpool negotiated"));
    ret = GST_FLOW_ERROR;
    goto done;
  }
activate_failed:
  {
    GST_ERROR_OBJECT (sink, "failed to activate bufferpool.");
    ret = GST_FLOW_ERROR;
    goto done;
  }
done:
  {
    g_mutex_unlock (&sink->render_lock);
    return ret;
  }
}

static void
gst_wayland_sink_videooverlay_init (GstVideoOverlayInterface * iface)
{
  FUNCTION_ENTER ();

  iface->set_window_handle = gst_wayland_sink_set_window_handle;
  iface->set_render_rectangle = gst_wayland_sink_set_render_rectangle;
  iface->expose = gst_wayland_sink_expose;
}

static void
gst_wayland_sink_set_window_handle (GstVideoOverlay * overlay, guintptr handle)
{
  FUNCTION_ENTER ();

  GstTizenwlSink *sink = GST_WAYLAND_SINK (overlay);
  struct wl_surface *surface = (struct wl_surface *) handle;

  g_return_if_fail (sink != NULL);

  if (sink->window != NULL) {
    GST_WARNING_OBJECT (sink, "changing window handle is not supported");
    return;
  }

  g_mutex_lock (&sink->render_lock);

  GST_DEBUG_OBJECT (sink, "Setting window handle %" GST_PTR_FORMAT,
      (void *) handle);

  g_clear_object (&sink->window);

  if (handle) {
    if (G_LIKELY (gst_wayland_sink_find_display (sink))) {
      /* we cannot use our own display with an external window handle */
      if (G_UNLIKELY (sink->display->own_display)) {
        GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_READ_WRITE,
            ("Application did not provide a wayland display handle"),
            ("waylandsink cannot use an externally-supplied surface without "
                "an externally-supplied display handle. Consider providing a "
                "display handle from your application with GstContext"));
      } else {
        sink->window = gst_wl_window_new_in_surface (sink->display, surface);
        GST_DEBUG ("sink->window %p", sink->window);
      }
    } else {
      GST_ERROR_OBJECT (sink, "Failed to find display handle, "
          "ignoring window handle");
    }
  }
#ifdef GST_WLSINK_ENHANCEMENT
  gst_wayland_sink_update_window_geometry (sink);
#endif

  g_mutex_unlock (&sink->render_lock);
}

#ifdef GST_WLSINK_ENHANCEMENT
static void
gst_wayland_sink_update_window_geometry (GstTizenwlSink * sink)
{
  FUNCTION_ENTER ();
  g_return_if_fail (sink != NULL);
  g_return_if_fail (sink->window != NULL);

  gst_wl_window_set_rotate_angle (sink->window, sink->rotate_angle);
  gst_wl_window_set_disp_geo_method (sink->window,
      sink->display_geometry_method);
  gst_wl_window_set_orientation (sink->window, sink->orientation);
  gst_wl_window_set_flip (sink->window, sink->flip);
}

static void
gst_wayland_sink_render_last_buffer (GstTizenwlSink * sink)
{
  FUNCTION_ENTER ();
  g_return_if_fail (sink != NULL);

  g_mutex_lock (&sink->render_lock);
  gst_wl_window_set_video_info (sink->window, &sink->video_info);
  sink->video_info_changed = FALSE;
  if (sink->last_buffer)
    render_last_buffer (sink);
  g_mutex_unlock (&sink->render_lock);
}
#endif
static void
gst_wayland_sink_set_render_rectangle (GstVideoOverlay * overlay,
    gint x, gint y, gint w, gint h)
{
  FUNCTION_ENTER ();

  GstTizenwlSink *sink = GST_WAYLAND_SINK (overlay);

  g_return_if_fail (sink != NULL);

  g_mutex_lock (&sink->render_lock);
  if (!sink->window) {
    g_mutex_unlock (&sink->render_lock);
    GST_WARNING_OBJECT (sink,
        "set_render_rectangle called without window, ignoring");
    return;
  }

  GST_DEBUG_OBJECT (sink, "window geometry changed to (%d, %d) %d x %d",
      x, y, w, h);
  gst_wl_window_set_render_rectangle (sink->window, x, y, w, h);

  g_mutex_unlock (&sink->render_lock);
}

static void
gst_wayland_sink_expose (GstVideoOverlay * overlay)
{
  FUNCTION_ENTER ();

  GstTizenwlSink *sink = GST_WAYLAND_SINK (overlay);

  g_return_if_fail (sink != NULL);

  GST_DEBUG_OBJECT (sink, "expose");

  g_mutex_lock (&sink->render_lock);
  if (sink->last_buffer && g_atomic_int_get (&sink->redraw_pending) == FALSE) {
    GST_DEBUG_OBJECT (sink, "redrawing last buffer");
    render_last_buffer (sink);
  }
  g_mutex_unlock (&sink->render_lock);
}

static void
gst_wayland_sink_waylandvideo_init (GstWaylandVideoInterface * iface)
{
  FUNCTION_ENTER ();

  iface->begin_geometry_change = gst_wayland_sink_begin_geometry_change;
  iface->end_geometry_change = gst_wayland_sink_end_geometry_change;
}

static void
gst_wayland_sink_begin_geometry_change (GstWaylandVideo * video)
{
  FUNCTION_ENTER ();

  GstTizenwlSink *sink = GST_WAYLAND_SINK (video);
  g_return_if_fail (sink != NULL);

  g_mutex_lock (&sink->render_lock);
  if (!sink->window || !sink->window->subsurface) {
    g_mutex_unlock (&sink->render_lock);
    GST_INFO_OBJECT (sink,
        "begin_geometry_change called without window, ignoring");
    return;
  }

  wl_subsurface_set_sync (sink->window->subsurface);
  g_mutex_unlock (&sink->render_lock);
}

static void
gst_wayland_sink_end_geometry_change (GstWaylandVideo * video)
{
  FUNCTION_ENTER ();

  GstTizenwlSink *sink = GST_WAYLAND_SINK (video);
  g_return_if_fail (sink != NULL);

  g_mutex_lock (&sink->render_lock);
  if (!sink->window || !sink->window->subsurface) {
    g_mutex_unlock (&sink->render_lock);
    GST_INFO_OBJECT (sink,
        "end_geometry_change called without window, ignoring");
    return;
  }

  wl_subsurface_set_desync (sink->window->subsurface);
  g_mutex_unlock (&sink->render_lock);
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  FUNCTION_ENTER ();

  GST_DEBUG_CATEGORY_INIT (gsttizenwl_debug, "tizenwlsink", 0,
      " temporary wayland video sink");

  return gst_element_register (plugin, "tizenwlsink", GST_RANK_MARGINAL,
      GST_TYPE_WAYLAND_SINK);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    tizenwlsink,
    "Wayland Video Sink", plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME,
    GST_PACKAGE_ORIGIN)
