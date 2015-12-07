/* GStreamer Wayland video sink
 *
 * Copyright (C) 2011 Intel Corporation
 * Copyright (C) 2011 Sreerenj Balachandran <sreerenj.balachandran@intel.com>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "wlwindow.h"
#ifdef GST_WLSINK_ENHANCEMENT
#include "gstwaylandsink.h"
#define SWAP(a, b) { (a) ^= (b) ^= (a) ^= (b); }
#endif

GST_DEBUG_CATEGORY_EXTERN (gsttizenwl_debug);
#define GST_CAT_DEFAULT gsttizenwl_debug

G_DEFINE_TYPE (GstWlWindow, gst_wl_window, G_TYPE_OBJECT);

static void gst_wl_window_finalize (GObject * gobject);

static void
handle_ping (void *data, struct wl_shell_surface *shell_surface,
    uint32_t serial)
{
  FUNCTION_ENTER ();

  wl_shell_surface_pong (shell_surface, serial);
}

static void
handle_configure (void *data, struct wl_shell_surface *shell_surface,
    uint32_t edges, int32_t width, int32_t height)
{
  FUNCTION_ENTER ();

}

static void
handle_popup_done (void *data, struct wl_shell_surface *shell_surface)
{
  FUNCTION_ENTER ();

}

static const struct wl_shell_surface_listener shell_surface_listener = {
  handle_ping,
  handle_configure,
  handle_popup_done
};

static void
gst_wl_window_class_init (GstWlWindowClass * klass)
{
  FUNCTION_ENTER ();

  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = gst_wl_window_finalize;
}

static void
gst_wl_window_init (GstWlWindow * self)
{
  FUNCTION_ENTER ();

}

static void
gst_wl_window_finalize (GObject * gobject)
{
  FUNCTION_ENTER ();

  GstWlWindow *self = GST_WL_WINDOW (gobject);

#ifdef GST_WLSINK_ENHANCEMENT
  tizen_video_object_destroy (self->video_object);
#endif

  if (self->shell_surface) {
    wl_shell_surface_destroy (self->shell_surface);
  }

  if (self->subsurface) {
    wl_subsurface_destroy (self->subsurface);
  }

  wl_viewport_destroy (self->viewport);
  wl_surface_destroy (self->surface);

  g_clear_object (&self->display);

  G_OBJECT_CLASS (gst_wl_window_parent_class)->finalize (gobject);
}

static GstWlWindow *
gst_wl_window_new_internal (GstWlDisplay * display, struct wl_surface *surface)
{
  FUNCTION_ENTER ();

  GstWlWindow *window;
  struct wl_region *region;

  g_return_val_if_fail (surface != NULL, NULL);

  window = g_object_new (GST_TYPE_WL_WINDOW, NULL);
  window->display = g_object_ref (display);
  window->surface = surface;

  /* make sure the surface runs on our local queue */
  wl_proxy_set_queue ((struct wl_proxy *) surface, display->queue);

#ifdef GST_WLSINK_ENHANCEMENT
  /* create shell_surface here for enlightenment */
  if (display->need_shell_surface)
    window->shell_surface = wl_shell_get_shell_surface (display->shell,
        window->area_surface);
#endif
 
  window->viewport = wl_scaler_get_viewport (display->scaler, window->surface);

  /* do not accept input */
  region = wl_compositor_create_region (display->compositor);
  wl_surface_set_input_region (surface, region);
  wl_region_destroy (region);

#ifdef GST_WLSINK_ENHANCEMENT
  window->video_object = tizen_video_get_object(display->tizen_video, window->surface);
#endif

  return window;
}

GstWlWindow *
gst_wl_window_new_toplevel (GstWlDisplay * display, GstVideoInfo * video_info)
{
  FUNCTION_ENTER ();

  GstWlWindow *window;

  window = gst_wl_window_new_internal (display,
      wl_compositor_create_surface (display->compositor));

  gst_wl_window_set_video_info (window, video_info);
  gst_wl_window_set_render_rectangle (window, 0, 0, window->video_width,
      window->video_height);

#ifdef GST_WLSINK_ENHANCEMENT
  /* not create shell_surface here for enlightenment */
  display->need_shell_surface = TRUE;
#else
  window->shell_surface = wl_shell_get_shell_surface (display->shell,
      window->surface);
#endif

  if (window->shell_surface) {
    wl_shell_surface_add_listener (window->shell_surface,
        &shell_surface_listener, window);
    wl_shell_surface_set_toplevel (window->shell_surface);
  } else {
    GST_ERROR ("Unable to get wl_shell_surface");

    g_object_unref (window);
    return NULL;
  }

  return window;
}

GstWlWindow *
gst_wl_window_new_in_surface (GstWlDisplay * display,
    struct wl_surface * parent)
{
  FUNCTION_ENTER ();

  GstWlWindow *window;

  window = gst_wl_window_new_internal (display,
      wl_compositor_create_surface (display->compositor));

  window->subsurface = wl_subcompositor_get_subsurface (display->subcompositor,
      window->surface, parent);
  wl_subsurface_set_desync (window->subsurface);
#ifdef GST_WLSINK_ENHANCEMENT
  if (display->tizen_policy)
    tizen_policy_place_subsurface_below_parent (display->tizen_policy,
        window->subsurface);

  wl_surface_commit (parent);
#endif
  return window;
}

GstWlDisplay *
gst_wl_window_get_display (GstWlWindow * window)
{
  FUNCTION_ENTER ();

  g_return_val_if_fail (window != NULL, NULL);

  return g_object_ref (window->display);
}

struct wl_surface *
gst_wl_window_get_wl_surface (GstWlWindow * window)
{
  FUNCTION_ENTER ();

  g_return_val_if_fail (window != NULL, NULL);

  return window->surface;
}

gboolean
gst_wl_window_is_toplevel (GstWlWindow * window)
{
  FUNCTION_ENTER ();

  g_return_val_if_fail (window != NULL, FALSE);

  return (window->shell_surface != NULL);
}

static void
gst_wl_window_resize_internal (GstWlWindow * window, gboolean commit)
{
  FUNCTION_ENTER ();

  GstVideoRectangle src = { 0, };
  GstVideoRectangle res;        //dst

  src.w = window->video_width;
  src.h = window->video_height;
#ifdef GST_WLSINK_ENHANCEMENT   // need to change ifndef to ifdef

  GstVideoRectangle src_origin = { 0, 0, 0, 0 };
  GstVideoRectangle src_input = { 0, 0, 0, 0 };
  GstVideoRectangle dst = { 0, 0, 0, 0 };

  gint rotate = 0;
  gint transform = WL_OUTPUT_TRANSFORM_NORMAL;

  src.x = src.y = 0;
  src_input.w = src_origin.w = window->video_width;
  src_input.h = src_origin.h = window->video_height;
  GST_INFO ("video (%d x %d)", window->video_width, window->video_height);
  GST_INFO ("src_input(%d, %d, %d x %d)", src_input.x, src_input.y, src_input.w,
      src_input.h);
  GST_INFO ("src_origin(%d, %d, %d x %d)", src_origin.x, src_origin.y,
      src_origin.w, src_origin.h);

  if (window->rotate_angle == DEGREE_0 || window->rotate_angle == DEGREE_180) {
    src.w = window->video_width;        //video_width
    src.h = window->video_height;       //video_height
  } else {
    src.w = window->video_height;
    src.h = window->video_width;
  }
  GST_INFO ("src(%d, %d, %d x %d)", src.x, src.y, src.w, src.h);

  /*default res.w and res.h */
  dst.w = window->render_rectangle.w;
  dst.h = window->render_rectangle.h;
  GST_INFO ("dst(%d,%d,%d x %d)", dst.x, dst.y, dst.w, dst.h);
  GST_INFO ("window->render_rectangle(%d,%d,%d x %d)",
      window->render_rectangle.x, window->render_rectangle.y,
      window->render_rectangle.w, window->render_rectangle.h);
  switch (window->disp_geo_method) {
    case DISP_GEO_METHOD_LETTER_BOX:
      GST_INFO ("DISP_GEO_METHOD_LETTER_BOX");
      gst_video_sink_center_rect (src, dst, &res, TRUE);
      gst_video_sink_center_rect (dst, src, &src_input, FALSE);
      res.x += window->render_rectangle.x;
      res.y += window->render_rectangle.y;
      break;
    case DISP_GEO_METHOD_ORIGIN_SIZE_OR_LETTER_BOX:
      if (src.w > dst.w || src.h > dst.h) {
        /*LETTER BOX */
        GST_INFO
            ("DISP_GEO_METHOD_ORIGIN_SIZE_OR_LETTER_BOX -> set LETTER BOX");
        gst_video_sink_center_rect (src, dst, &res, TRUE);
        gst_video_sink_center_rect (dst, src, &src_input, FALSE);
        res.x += window->render_rectangle.x;
        res.y += window->render_rectangle.y;
      } else {
        /*ORIGIN SIZE */
        GST_INFO ("DISP_GEO_METHOD_ORIGIN_SIZE");
        gst_video_sink_center_rect (src, dst, &res, FALSE);
        gst_video_sink_center_rect (dst, src, &src_input, FALSE);
      }
      break;
    case DISP_GEO_METHOD_ORIGIN_SIZE:  //is working
      GST_INFO ("DISP_GEO_METHOD_ORIGIN_SIZE");
      gst_video_sink_center_rect (src, dst, &res, FALSE);
      gst_video_sink_center_rect (dst, src, &src_input, FALSE);
      break;
    case DISP_GEO_METHOD_FULL_SCREEN:  //is working
      GST_INFO ("DISP_GEO_METHOD_FULL_SCREEN");
      res.x = res.y = 0;
      res.w = window->render_rectangle.w;
      res.h = window->render_rectangle.h;
      break;
    case DISP_GEO_METHOD_CROPPED_FULL_SCREEN:
      GST_INFO ("DISP_GEO_METHOD_CROPPED_FULL_SCREEN");
      gst_video_sink_center_rect (src, dst, &res, FALSE);
      gst_video_sink_center_rect (dst, src, &src_input, FALSE);
      res.x = res.y = 0;
      res.w = dst.w;
      res.h = dst.h;
      break;
    default:
      break;
  }

  switch (window->rotate_angle) {
    case DEGREE_0:
      transform = WL_OUTPUT_TRANSFORM_NORMAL;
      break;
    case DEGREE_90:
      transform = WL_OUTPUT_TRANSFORM_90;
      break;
    case DEGREE_180:
      transform = WL_OUTPUT_TRANSFORM_180;
      break;
    case DEGREE_270:
      transform = WL_OUTPUT_TRANSFORM_270;
      break;

    default:
      GST_ERROR ("Unsupported rotation [%d]... set DEGREE 0.",
          window->rotate_angle);
      break;
  }

  switch (window->flip) {
    case FLIP_NONE:
      break;
    case FLIP_VERTICAL:
      transform = WL_OUTPUT_TRANSFORM_FLIPPED;
      break;
    case FLIP_HORIZONTAL:
      transform = WL_OUTPUT_TRANSFORM_FLIPPED_180;
      break;
    case FLIP_BOTH:
      transform = WL_OUTPUT_TRANSFORM_180;
      break;
    default:
      GST_ERROR ("Unsupported flip [%d]... set FLIP_NONE.", window->flip);
  }

  GST_INFO
      ("window[%d x %d] src[%d,%d,%d x %d],dst[%d,%d,%d x %d],input[%d,%d,%d x %d],result[%d,%d,%d x %d]",
      window->render_rectangle.w, window->render_rectangle.h,
      src.x, src.y, src.w, src.h,
      dst.x, dst.y, dst.w, dst.h,
      src_input.x, src_input.y, src_input.w, src_input.h,
      res.x, res.y, res.w, res.h);

  GST_INFO ("video (%d x %d)", window->video_width, window->video_height);
  GST_INFO ("src_input(%d, %d, %d x %d)", src_input.x, src_input.y, src_input.w,
      src_input.h);
  GST_INFO ("src_origin(%d, %d, %d x %d)", src_origin.x, src_origin.y,
      src_origin.w, src_origin.h);
  GST_INFO ("src(%d, %d, %d x %d)", src.x, src.y, src.w, src.h);
  GST_INFO ("dst(%d,%d,%d x %d)", dst.x, dst.y, dst.w, dst.h);
  GST_INFO ("window->render_rectangle(%d,%d,%d x %d)",
      window->render_rectangle.x, window->render_rectangle.y,
      window->render_rectangle.w, window->render_rectangle.h);
  GST_INFO ("res(%d, %d, %d x %d)", res.x, res.y, res.w, res.h);

  if (window->subsurface) {
    GST_INFO ("have window->subsurface");
    wl_subsurface_set_position (window->subsurface,
        window->render_rectangle.x + res.x, window->render_rectangle.y + res.y);
    GST_INFO ("wl_subsurface_set_position(%d,%d)",
        window->render_rectangle.x + res.x, window->render_rectangle.y + res.y);
  }
  wl_viewport_set_destination (window->viewport, res.w, res.h);
  GST_INFO ("wl_viewport_set_destination(%d,%d)", res.w, res.h);

  wl_viewport_set_source (window->viewport, wl_fixed_from_int (src_input.x),
      wl_fixed_from_int (src_input.y), wl_fixed_from_int (src_input.w),
      wl_fixed_from_int (src_input.h));
  GST_INFO ("wl_viewport_set_source(%d,%d, %d x %d)", src_input.x, src_input.y,
      src_input.w, src_input.h);

  wl_surface_set_buffer_transform (window->surface, transform);
  GST_INFO ("wl_surface_set_buffer_transform (%d)", transform);

  if (commit) {
    wl_surface_damage (window->surface, 0, 0, res.w, res.h);
    wl_surface_commit (window->surface);
  }

  /* this is saved for use in wl_surface_damage */
  window->surface_width = res.w;
  window->surface_height = res.h;

#else
  gst_video_sink_center_rect (src, window->render_rectangle, &res, TRUE);
  if (window->subsurface)
    wl_subsurface_set_position (window->subsurface,
        window->render_rectangle.x + res.x, window->render_rectangle.y + res.y);

  wl_viewport_set_destination (window->viewport, res.w, res.h);

  if (commit) {
    wl_surface_damage (window->surface, 0, 0, res.w, res.h);
    wl_surface_commit (window->surface);
  }

  /* this is saved for use in wl_surface_damage */
  window->surface_width = res.w;
  window->surface_height = res.h;
#endif
}

void
gst_wl_window_set_video_info (GstWlWindow * window, GstVideoInfo * info)
{
  FUNCTION_ENTER ();

  g_return_if_fail (window != NULL);

  window->video_width =
      gst_util_uint64_scale_int_round (info->width, info->par_n, info->par_d);
  window->video_height = info->height;

  if (window->render_rectangle.w != 0)
    gst_wl_window_resize_internal (window, FALSE);
}

void
gst_wl_window_set_render_rectangle (GstWlWindow * window, gint x, gint y,
    gint w, gint h)
{
  FUNCTION_ENTER ();

  g_return_if_fail (window != NULL);

  window->render_rectangle.x = x;
  window->render_rectangle.y = y;
  window->render_rectangle.w = w;
  window->render_rectangle.h = h;

  if (window->video_width != 0)
    gst_wl_window_resize_internal (window, TRUE);
}

#ifdef GST_WLSINK_ENHANCEMENT
void
gst_wl_window_set_rotate_angle (GstWlWindow * window, guint rotate_angle)
{
  FUNCTION_ENTER ();
  g_return_if_fail (window != NULL);
  window->rotate_angle = rotate_angle;
  GST_INFO ("rotate_angle value is (%d)", window->rotate_angle);

}

void
gst_wl_window_set_disp_geo_method (GstWlWindow * window, guint disp_geo_method)
{
  FUNCTION_ENTER ();
  g_return_if_fail (window != NULL);
  window->disp_geo_method = disp_geo_method;
  GST_INFO ("disp_geo_method value is (%d)", window->disp_geo_method);
}

void
gst_wl_window_set_orientation (GstWlWindow * window, guint orientation)
{
  FUNCTION_ENTER ();
  g_return_if_fail (window != NULL);
  window->orientation = orientation;
  GST_INFO ("orientation value is (%d)", window->orientation);
}

void
gst_wl_window_set_flip (GstWlWindow * window, guint flip)
{
  FUNCTION_ENTER ();
  g_return_if_fail (window != NULL);
  window->flip = flip;
  GST_INFO ("flip value is (%d)", window->flip);
}
#endif
