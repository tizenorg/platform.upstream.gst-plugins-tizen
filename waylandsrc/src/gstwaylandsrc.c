/*
 * GStreamer Wayland video source
 *
 * Copyright (C) 2013 Sebastian Wick <sebastian@sebastianwick.net>
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
 * SECTION:element-waylandsrc
 *
 *  The waylandsrc captures video frames from a wayland compositor
 *  Setup the Wayland environment as described in
 *  <ulink url="http://wayland.freedesktop.org/building.html">Wayland</ulink> home page.
 *  The current implementaion is based on weston compositor.
 *  This plugin depends on the screnshooter protocol in weston.
 *
 * <refsect2>
 * <title>Example pipelines</title>
 * |[
 * gst-launch -v waylandsrc ! waylandsink
 * ]| test the video capturing in wayland
 * </refsect2>
 */

#include <stdio.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <gst/video/video-format.h>
#include "tizen-screenshooter-client-protocol.h"
#include "gstwaylandsrc.h"

GST_DEBUG_CATEGORY_STATIC (waylandsrc_debug);
#define GST_CAT_DEFAULT waylandsrc_debug 

#if G_BYTE_ORDER == G_BIG_ENDIAN
#define GST_WAYLAND_SRC_VIDEO_FORMAT "ARGB"
#else
//#define GST_WAYLAND_SRC_VIDEO_FORMAT "BGRA"
#define GST_WAYLAND_SRC_VIDEO_FORMAT "ARGB"
#endif

#define NUM_BUFFERS 5

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (GST_WAYLAND_SRC_VIDEO_FORMAT))
    );

enum
{
  PROP_0,
  PROP_DISPLAY_NAME,
  PROP_OUTPUT_NUM
};

/*Fixme: Add more interfaces */
#define gst_wayland_src_parent_class parent_class
G_DEFINE_TYPE (GstWaylandSrc, gst_wayland_src, GST_TYPE_PUSH_SRC);

static void gst_wayland_src_finalize (GObject * object);

static gboolean gst_wayland_src_connect (GstWaylandSrc * src);

static void gst_wayland_src_disconnect (GstWaylandSrc * src);

static struct output *gst_wayland_src_active_output (GstWaylandSrc * src);

static void gst_wayland_src_dispose (GObject * object);

static void
gst_wayland_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);

static void
gst_wayland_src_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec);

static GstCaps *gst_wayland_src_fixate (GstBaseSrc * bsrc, GstCaps * caps);

static GstCaps *gst_wayland_src_get_caps (GstBaseSrc * psrc, GstCaps * filter);

static gboolean gst_wayland_src_set_caps (GstBaseSrc * psrc, GstCaps * caps);

static gboolean gst_wayland_src_start (GstBaseSrc * basesrc);

static gboolean gst_wayland_src_stop (GstBaseSrc * basesrc);

static gboolean gst_wayland_src_unlock (GstBaseSrc * basesrc);

static GstFlowReturn gst_wayland_src_create (GstPushSrc * psrc, GstBuffer ** buf);

static void *gst_wayland_src_capture_thread (GstWaylandSrc * src);
static gboolean gst_wayland_src_thread_start (GstWaylandSrc * src);
static void gst_wayland_src_gst_buffer_unref (gpointer data);

static void
handle_global (void *data, struct wl_registry *registry,
    uint32_t name, const char *interface, uint32_t version);

static void
handle_global_remove (void *data, struct wl_registry *registry, 
uint32_t name);

static void
display_handle_geometry (void *data,
    struct wl_output *wl_output,
    int x,
    int y,
    int physical_width,
    int physical_height,
    int subpixel, const char *make, const char *model, int transform);

static void
display_handle_mode (void *data,
    struct wl_output *wl_output,
    uint32_t flags, int width, int height, int refresh);

static const struct wl_registry_listener registry_listener = {
  handle_global,
  handle_global_remove
};

static const struct wl_output_listener output_listener = {
  display_handle_geometry,
  display_handle_mode
};

static void
mirror_handle_dequeued(void *data,
                       struct tizen_screenmirror *tizen_screenmirror,
                       struct wl_buffer *buffer);

static void
mirror_handle_content(void *data,
                      struct tizen_screenmirror *tizen_screenmirror,
                      uint32_t content);

static void
mirror_handle_stop(void *data, struct tizen_screenmirror *tizen_screenmirror);

static const struct tizen_screenmirror_listener mirror_listener = { 
  mirror_handle_dequeued,
  mirror_handle_content,
  mirror_handle_stop
};

static void
mirror_handle_dequeued(void *data,
                       struct tizen_screenmirror *tizen_screenmirror,
                       struct wl_buffer *buffer)
{
  struct output_buffer *out_buffer;
  GstWaylandSrc *src = data;
  GstBuffer *gst_buffer;
  GstClock *clock;
  GstClockTime base_time, next_capture_ts;
  //gint64 next_frame_no;

  clock = gst_element_get_clock (GST_ELEMENT (src));
  if (!clock) {
    GST_ERROR_OBJECT (src, "Failed to get clock");
    return;
  }

  base_time = gst_element_get_base_time (GST_ELEMENT (src));  
  next_capture_ts = gst_clock_get_time (clock) - base_time;  

  wl_list_for_each(out_buffer, &src->buffer_list, link) {
    GST_WARNING ("fetching start");

    if (out_buffer->wl_buffer != buffer)
      continue;

    GST_DEBUG ("Buffer [%d] dequeued", wl_proxy_get_id((struct wl_proxy *)buffer));
    out_buffer->gst_buffer = gst_buffer_new ();  
    gst_buffer_append_memory (out_buffer->gst_buffer,
        gst_memory_new_wrapped (GST_MEMORY_FLAG_NO_SHARE, out_buffer->data,
            out_buffer->size, 0, out_buffer->size, (gpointer) out_buffer,
            gst_wayland_src_gst_buffer_unref));

    gst_buffer = out_buffer->gst_buffer;
    GST_BUFFER_DTS (gst_buffer) = GST_CLOCK_TIME_NONE;
    GST_BUFFER_PTS (gst_buffer) = next_capture_ts;  
    //GST_BUFFER_DURATION (gst_buffer) = dur;


    g_mutex_lock (&src->queue_lock);
    g_queue_push_tail (src->buf_queue, out_buffer);
    g_mutex_unlock (&src->queue_lock);

    /* Notify src_create to get new item */
    GST_WARNING ("signal");
    g_cond_signal (&src->queue_cond);
    break;
  }
}

static void
mirror_handle_content(void *data,
                      struct tizen_screenmirror *tizen_screenmirror,
                      uint32_t content)
{
    switch(content)
    {
    case TIZEN_SCREENMIRROR_CONTENT_NORMAL:
        GST_INFO ("TIZEN_SCREENMIRROR_CONTENT_NORMAL event");
        break;
    case TIZEN_SCREENMIRROR_CONTENT_VIDEO:
        GST_INFO ("TIZEN_SCREENMIRROR_CONTENT_VIDEO event");
        break;
    }
}

static void
mirror_handle_stop(void *data, struct tizen_screenmirror *tizen_screenmirror)
{
  GST_WARNING ("mirror_handle_stop");
}

static struct wl_shm_pool *
make_shm_pool (struct wl_shm *shm, int size, void **data) {
  struct wl_shm_pool *pool;
  int fd;
  char filename[1024];
  static int init = 0;

  snprintf (filename, 256, "%s-%d-%s", "/tmp/wayland-shm", init++, "XXXXXX");

  fd = mkstemp (filename);
  if (fd < 0) {
    GST_ERROR ("open %s failed:", filename);
    return NULL;
  }
  if (ftruncate (fd, size) < 0) {
    GST_ERROR ("ftruncate failed:..!");
    close (fd);
    return NULL;
  }

  *data = mmap (NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);  
  if (*data == MAP_FAILED) {
    GST_ERROR ("mmap failed: ");
    close (fd);
    return NULL;
  }

  pool = wl_shm_create_pool (shm, fd, size);

  close (fd);

  return pool;
}

static struct output_buffer *
shm_buffer_create (struct wl_shm *shm, size_t width, size_t height) 
{
  struct output_buffer *out_buffer = NULL;
  struct wl_shm_pool * pool = NULL;
  void *data = NULL;
  size_t block_size = 0, stride = 0;

  out_buffer = malloc (sizeof *out_buffer);
  if (!out_buffer)
    return NULL;

  stride = width * 4;
  block_size = stride * height;

  pool = make_shm_pool (shm, block_size, &data);  
  if (!pool) {
    return NULL;
  }

  out_buffer->wl_buffer =
      wl_shm_pool_create_buffer(pool, 0, width, height, stride, WL_SHM_FORMAT_ARGB8888);
  out_buffer->size = block_size;
  out_buffer->stride = stride;
  out_buffer->data = data;

  wl_shm_pool_destroy(pool);

  return out_buffer;
}

static gboolean
gst_wayland_src_thread_start (GstWaylandSrc * src)
{
  if (!src->updates_thread) {
    src->updates_thread =
        g_thread_new ("wayland_capture", (GThreadFunc) gst_wayland_src_capture_thread, src);

    GST_WARNING_OBJECT (src, "image capture thread start");
  } else {
    GST_LOG_OBJECT (src, "The thread function already running");
  }

  return TRUE;
}

static void *
gst_wayland_src_capture_thread (GstWaylandSrc *src)
{
  struct pollfd pfd;
  struct output *output = NULL;
  struct output_buffer *out_buffer = NULL;
  int i;

  output = gst_wayland_src_active_output (src);
  if (!output)
    return FALSE;

  for (i = 0; i < NUM_BUFFERS; i++) {
    out_buffer = shm_buffer_create (src->shm, output->width, output->height);
    if (!out_buffer) {
      GST_ERROR ("failed to create shm buffer\n");
      return FALSE;
    }

    out_buffer->output = output;

    wl_list_insert(&src->buffer_list, &out_buffer->link);
    GST_INFO ("Buffer [%d] is inserted", i);
    tizen_screenmirror_queue (src->screenmirror, out_buffer->wl_buffer);
  }

  tizen_screenmirror_start(src->screenmirror);
  wl_display_roundtrip(src->display);

  pfd.fd = wl_display_get_fd(src->display);
  pfd.events = POLLIN;

  /* thread loop */
  while (!src->thread_return) {
    while (wl_display_prepare_read_queue (src->display, src->queue) != 0)
      wl_display_dispatch_queue_pending (src->display, src->queue);

    wl_display_flush (src->display);

    GST_WARNING ("Poll start");
    if (poll(&pfd, 1, 100)  < 0) {
      wl_display_cancel_read (src->display);
      break;
    } else {
      wl_display_read_events (src->display);
      wl_display_dispatch_queue_pending (src->display, src->queue);
      GST_WARNING ("Poll end22");
    }
  }

  GST_LOG_OBJECT (src, "The thread function returns");

  return NULL;
}

static void
gst_wayland_src_class_init (GstWaylandSrcClass * klass)
{
  GObjectClass *gc = G_OBJECT_CLASS (klass);
  GstElementClass *ec = GST_ELEMENT_CLASS (klass);
  GstBaseSrcClass *bc = GST_BASE_SRC_CLASS (klass);
  GstPushSrcClass *pc = GST_PUSH_SRC_CLASS (klass);

  gc->finalize = gst_wayland_src_finalize;
  gc->dispose = gst_wayland_src_dispose;
  gc->set_property = gst_wayland_src_set_property;
  gc->get_property = gst_wayland_src_get_property;

  bc->fixate = gst_wayland_src_fixate;
  bc->get_caps = gst_wayland_src_get_caps;
  bc->set_caps = gst_wayland_src_set_caps;
  bc->start = gst_wayland_src_start;  
  bc->stop = gst_wayland_src_stop;
  bc->unlock = gst_wayland_src_unlock;

  pc->create = gst_wayland_src_create;

  gst_element_class_add_pad_template (ec,
      gst_static_pad_template_get (&src_template));

  g_object_class_install_property (gc, PROP_DISPLAY_NAME,
      g_param_spec_string ("display-name", "Display", "Wayland Display Name",
          NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gc, PROP_OUTPUT_NUM,
      g_param_spec_uint ("output-num", "Output number", "Wayland Output Number",
          0, G_MAXINT, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_static_metadata (ec,
      "wayland video source", "Source/Video",
      "captures a wayland output",
      "Sebastian Wick <sebastian@sebastianwick.net>");
}

static void
gst_wayland_src_init (GstWaylandSrc * src)
{
  gst_base_src_set_format (GST_BASE_SRC (src), GST_FORMAT_TIME);
  gst_base_src_set_live (GST_BASE_SRC (src), TRUE);

  src->display_name = NULL;
  src->display = NULL;
  src->registry = NULL;
  src->shm = NULL;
  src->screenshooter = NULL;
  src->output_num = 0;
  src->fps_n = src->fps_d = 0;
  src->buffer_copy_done = 0;
  wl_list_init (&src->output_list);
  wl_list_init (&src->buffer_list);

  src->buf_queue = g_queue_new ();

  g_mutex_init (&src->queue_lock);
  g_mutex_init (&src->cond_lock);
  g_cond_init (&src->queue_cond);
}

static void
gst_wayland_src_finalize (GObject * object)
{
  GstWaylandSrc *src = GST_WAYLAND_SRC (object);
  struct output *output = NULL;
  struct output *output_tmp = NULL;
  struct output_buffer *out_buffer = NULL;
  struct output_buffer *out_buffer_tmp = NULL;

  g_mutex_clear (&src->queue_lock);
  g_mutex_clear (&src->cond_lock);
  g_cond_clear (&src->queue_cond);
  g_queue_free (src->buf_queue);

  if (src->display_name) {
    g_free (src->display_name);
    src->display_name = NULL;
  }

  if (src->registry)
    wl_registry_destroy(src->registry);

  wl_list_for_each_safe(output, output_tmp, &src->output_list, link) {
    if (output->output)
      wl_output_destroy (output->output);
    wl_list_remove (&output->link);
    g_free(output);
  }

  wl_list_for_each_safe(out_buffer, out_buffer_tmp, &src->buffer_list, link) {
    if (out_buffer->wl_buffer)
      wl_buffer_destroy (out_buffer->wl_buffer);
    wl_list_remove (&out_buffer->link);
    g_free(out_buffer);
  }

  if (src->shm)
    wl_shm_destroy(src->shm);

  if (src->screenmirror)
    tizen_screenmirror_destroy(src->screenmirror);
  if (src->screenshooter)
    tizen_screenshooter_destroy(src->screenshooter);

  if (src->queue)
    wl_event_queue_destroy(src->queue);

  if (src->display) {
    gst_wayland_src_disconnect (src);
  }
}

static gboolean
gst_wayland_src_connect (GstWaylandSrc * src)
{
  src->display = wl_display_connect (src->display_name);
  if (src->display == NULL) {
    GST_ERROR_OBJECT (src, "failed to connect to wayland display");
    return FALSE;
  }

  src->registry = wl_display_get_registry (src->display);  
  src->queue = wl_display_create_queue (src->display); 
  wl_proxy_set_queue ((struct wl_proxy *) src->display, src->queue);
  wl_proxy_set_queue ((struct wl_proxy *) src->registry, src->queue);

  wl_registry_add_listener (src->registry, &registry_listener, src);  

  wl_display_dispatch_queue(src->display, src->queue);
  wl_display_roundtrip_queue(src->display, src->queue);

  if (!src->screenshooter) {
    GST_ERROR_OBJECT (src, "display doesn't support tizen screenshooter\n");
    return FALSE;
  }

  return TRUE;
}

static void
gst_wayland_src_disconnect (GstWaylandSrc * src)
{
  g_assert (src->display != NULL);

  wl_display_flush (src->display);
  wl_display_disconnect (src->display);
  src->display = NULL;
}

static struct output *
gst_wayland_src_active_output (GstWaylandSrc * src)
{
  struct output *it = NULL;
  wl_list_for_each(it, &src->output_list, link) {
    /* return first output */
    return it;
  }

  return NULL;
}

static void
gst_wayland_src_dispose (GObject * object) { }

static void
gst_wayland_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstWaylandSrc *src = GST_WAYLAND_SRC (object);

  switch (prop_id) {
    case PROP_DISPLAY_NAME:
      if (src->display) {
        GST_WARNING_OBJECT (src, "display name must be set before opening display");
        break;
      }
      if (src->display_name)
        g_free (src->display_name);
      src->display_name = g_strdup (g_value_get_string (value));
      break;
    case PROP_OUTPUT_NUM:
      src->output_num = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_wayland_src_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstWaylandSrc *src = GST_WAYLAND_SRC (object);

  switch (prop_id) {
    case PROP_DISPLAY_NAME:
      g_value_set_string (value, src->display_name);
      break;
    case PROP_OUTPUT_NUM:
      g_value_set_uint (value, src->output_num);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstCaps *
gst_wayland_src_fixate (GstBaseSrc * bsrc, GstCaps * caps)
{
  gint i;
  GstStructure *structure;

  caps = gst_caps_make_writable (caps);

  for (i = 0; i < gst_caps_get_size (caps); ++i) {
    structure = gst_caps_get_structure (caps, i);

    gst_structure_fixate_field_nearest_fraction (structure, "framerate", 25, 1);
  }
  caps = GST_BASE_SRC_CLASS(parent_class)->fixate (bsrc, caps);

  return caps;
}

static GstCaps *
gst_wayland_src_get_caps (GstBaseSrc * psrc, GstCaps * filter)
{
  GstWaylandSrc *src = GST_WAYLAND_SRC (psrc);
  GstCaps *caps = NULL;
  struct output *output = NULL;

  if (!src->display && !gst_wayland_src_connect (src))
    return NULL;

  output = gst_wayland_src_active_output (src);
  if (!output)
    return NULL;

  caps = gst_caps_new_simple ("video/x-raw",
      "format", G_TYPE_STRING, GST_WAYLAND_SRC_VIDEO_FORMAT,
      "width", G_TYPE_INT, output->width,
      "height", G_TYPE_INT, output->height,
      "framerate", GST_TYPE_FRACTION_RANGE, 1, G_MAXINT, G_MAXINT, 1, NULL);

  return caps;
}

static gboolean
gst_wayland_src_set_caps (GstBaseSrc * psrc, GstCaps * caps)
{
  GstWaylandSrc *src = GST_WAYLAND_SRC (psrc);
  GstStructure *structure;
  const GValue *fps;

  if (!src->display)
    return FALSE;

  /* The only thing that can change is the framerate downstream wants */
  structure = gst_caps_get_structure (caps, 0);
  fps = gst_structure_get_value (structure, "framerate");
  if (!fps)
    return FALSE;

  src->fps_n = gst_value_get_fraction_numerator (fps);
  src->fps_d = gst_value_get_fraction_denominator (fps);

  return TRUE;
}

static gboolean
gst_wayland_src_start (GstBaseSrc * basesrc)
{
  GstWaylandSrc *src = GST_WAYLAND_SRC (basesrc);

  if (!src->display) {
    GST_ERROR_OBJECT (src, "Not setup");
    return FALSE;
  }

  src->last_frame_no = -1;

  gst_wayland_src_thread_start (src);

  return TRUE;
}

static gboolean
gst_wayland_src_stop (GstBaseSrc * basesrc)
{
  GstWaylandSrc *src = GST_WAYLAND_SRC (basesrc);

  GST_DEBUG_OBJECT (src, "stop start ");

  if (src->updates_thread) {
    src->thread_return = TRUE;
    g_thread_join (src->updates_thread);
    src->updates_thread = NULL;
  }

  GST_DEBUG_OBJECT (src, "stop end ");
  return TRUE;
}

static gboolean
gst_wayland_src_unlock (GstBaseSrc * basesrc)
{
  return TRUE;
}

static GstFlowReturn
gst_wayland_src_create (GstPushSrc * psrc, GstBuffer ** ret_buf)
{
  GstWaylandSrc *src = GST_WAYLAND_SRC (psrc);
  struct output_buffer *out_buffer = NULL;

  GST_WARNING ("Create START--");
  g_mutex_lock (&src->queue_lock);

  if (g_queue_is_empty (src->buf_queue)) {
    g_mutex_unlock (&src->queue_lock);

    g_mutex_lock (&src->cond_lock);
    g_cond_wait (&src->queue_cond, &src->cond_lock);
    g_mutex_unlock (&src->cond_lock);

    g_mutex_lock (&src->queue_lock);
    out_buffer = (struct output_buffer *) g_queue_pop_head (src->buf_queue);
    g_mutex_unlock (&src->queue_lock);
    GST_DEBUG_OBJECT (src, "Got a buffer");
  } else {
#if 0
    struct output_buffer *tempbuf = NULL;
    while ((tempbuf =
            (struct output_buffer *) g_queue_pop_head (src->buf_queue)) != NULL) {
      /* To reduce latency, skipping the old frames and submitting only latest frames */
      out_buffer = tempbuf;
      if (out_buffer) {
        if (g_queue_get_length (src->buf_queue) > 0) {
          GST_DEBUG_OBJECT (src, "Buffer[%d] skipped", wl_proxy_get_id((struct wl_proxy *)out_buffer->wl_buffer));
          gst_buffer_unref (out_buffer->gst_buffer);
        }
      }
    }
    g_mutex_unlock (&src->queue_lock);

#else
    out_buffer = (struct output_buffer *) g_queue_pop_head (src->buf_queue);
    GST_DEBUG_OBJECT (src, "Got a buffer.. %d buffers remaining..", g_queue_get_length(src->buf_queue));
    g_mutex_unlock (&src->queue_lock);
#endif
  }

  if (out_buffer == NULL) {
    GST_ERROR_OBJECT (src, "Failed to get a buffer");
    return GST_FLOW_ERROR;
  }

  *ret_buf = out_buffer->gst_buffer;

  GST_INFO_OBJECT (src, "Create gst buffer for wl_buffer[%d] (Size:%d PTS: %" G_GINT64_FORMAT ")",
      wl_proxy_get_id((struct wl_proxy *)out_buffer->wl_buffer), out_buffer->size, GST_BUFFER_PTS (*ret_buf)); 

  GST_WARNING ("Create END--");
  return GST_FLOW_OK;
}

static void
gst_wayland_src_gst_buffer_unref (gpointer data)
{
  struct output_buffer *out_buffer = data;
  struct output *output = out_buffer->output;
  GstWaylandSrc *src = output->src;

  tizen_screenmirror_queue (src->screenmirror, out_buffer->wl_buffer);
  GST_DEBUG ("Buffer [%d] queued", wl_proxy_get_id((struct wl_proxy *)out_buffer->wl_buffer));

  wl_display_sync(src->display);
}

static void
handle_global (void *data, struct wl_registry *registry,
    uint32_t name, const char *interface, uint32_t version)
{
  GstWaylandSrc *src = data;
  struct output *output;

  if (strcmp (interface, "wl_output") == 0) {
    output = malloc (sizeof *output);
    output->output = wl_registry_bind (registry, name, &wl_output_interface, 1);
    output->src = src;
    output->width = 0;
    output->height = 0;
    wl_list_insert (&src->output_list, &output->link);
    wl_output_add_listener (output->output, &output_listener, output);

    GST_INFO ("output is inserted");
  } else if (strcmp (interface, "wl_shm") == 0) {
    src->shm = wl_registry_bind (registry, name, &wl_shm_interface, 1);
    if (src->shm == NULL) {
      GST_ERROR_OBJECT (src, "shm is NULL");
      return;
    }

    GST_INFO ("shm is binded");
  } else if (strcmp (interface, "tizen_screenshooter") == 0) {
    src->screenshooter =
        wl_registry_bind (registry, name, &tizen_screenshooter_interface, 1); 
    if (src->screenshooter == NULL) {
      GST_ERROR_OBJECT (src, "tizen screenshooter is NULL");
      return;
    }

    output = gst_wayland_src_active_output (src);
    if (output == NULL) {
      GST_ERROR_OBJECT (src, "output is NULL");
      return;
    }

    src->screenmirror =
        tizen_screenshooter_get_screenmirror (src->screenshooter, output->output);
    if (src->screenmirror == NULL) {
      GST_ERROR_OBJECT (src, "tizen screenmirror is NULL");
      return;
    }

    tizen_screenmirror_add_listener (src->screenmirror, &mirror_listener, src);
    GST_INFO ("Tizen screenshooter is created");
  }
}

static void
handle_global_remove (void *data, struct wl_registry *registry, uint32_t name)
{
  /* TODO: remove stuff */
}

static void
display_handle_geometry (void *data,
    struct wl_output *wl_output,
    int x,
    int y,
    int physical_width,
    int physical_height,
    int subpixel, const char *make, const char *model, int transform)
{
  struct output *output = wl_output_get_user_data(wl_output);

  if (wl_output == output->output) {
    output->offset_x = x;
    output->offset_y = y;
    GST_LOG ("Output x:%d, y:%d", x, y);
  }
}

static void
display_handle_mode (void *data,
    struct wl_output *wl_output,
    uint32_t flags, int width, int height, int refresh)
{
  struct output *output = wl_output_get_user_data (wl_output);

  if (wl_output == output->output && (flags & WL_OUTPUT_MODE_CURRENT)) {
    output->width = width;
    output->height = height;
    GST_LOG ("Output width:%d, height:%d", width, height);
  }
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (waylandsrc_debug, "waylandsrc", 0,
      "Wayland display video capture Source");
  return gst_element_register (plugin, "waylandsrc", GST_RANK_PRIMARY,
      GST_TYPE_WAYLAND_SRC);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    waylandsrc,
    "Wayland display video src",
    plugin_init, VERSION, "LGPL", "Samsung Electronics Co",
    "http://www.samsung.com")

