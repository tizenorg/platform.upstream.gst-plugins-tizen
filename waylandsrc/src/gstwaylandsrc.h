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

#ifndef __GST_WAYLAND_VIDEO_SOURCE_H__
#define __GST_WAYLAND_VIDEO_SOURCE_H__

G_BEGIN_DECLS

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>

#include <wayland-client.h>

#define GST_TYPE_WAYLAND_SRC \
	    (gst_wayland_src_get_type())
#define GST_WAYLAND_SRC(obj) \
(G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_WAYLAND_SRC,GstWaylandSrc))

#define GST_WAYLAND_SRC_CLASS(klass) \
(G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_WAYLAND_SRC,GstWaylandSrcClass))

#define GST_IS_WAYLAND_SRC(obj) \
	    (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_WAYLAND_SRC))
#define GST_IS_WAYLAND_SRC_CLASS(klass) \
	    (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_WAYLAND_SRC))
#define GST_WAYLAND_SRC_GET_CLASS(inst) \
        (G_TYPE_INSTANCE_GET_CLASS ((inst), GST_TYPE_WAYLAND_SRC, GstWaylandSrcClass))


typedef struct _GstWaylandSrc GstWaylandSrc;
typedef struct _GstWaylandSrcClass GstWaylandSrcClass;

struct output {
  struct wl_output *output;
  GstWaylandSrc *src;
  guint width;
  guint height;
  guint offset_x;
  guint offset_y;
  struct wl_list link;
};


struct output_buffer {
  struct output *output;
  void *data;
  struct wl_buffer *wl_buffer;
  GstBuffer *gst_buffer;
  int stride, size, offset;
  struct wl_list link;
};

/*
struct shm_pool {
  struct wl_shm_pool *pool;
  size_t block_size;
  size_t blocks;
  size_t free_block_index;
  size_t *free_blocks;
  void *data;
};
*/

struct _GstWaylandSrc
{
  GstPushSrc parent;
  gchar *display_name;
  struct wl_display *display;
  struct wl_event_queue *queue;
  struct wl_registry *registry;
  struct wl_shm *shm;
  struct tizen_screenshooter *screenshooter;
  struct tizen_screenmirror *screenmirror;
  struct shm_pool *shm_pool;
  guint output_num;
  guint fps_n;
  guint fps_d;
  gint64 last_frame_no;
  int buffer_copy_done;
  struct wl_list output_list;
  struct wl_list buffer_list;

  GQueue *buf_queue;
  GMutex queue_lock;
  GMutex cond_lock;
  GCond queue_cond;
  GThread * updates_thread;
  gboolean thread_return;
};

struct _GstWaylandSrcClass
{
  GstPushSrcClass parent;
};

GType gst_wayland_src_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __GST_WAYLAND_VIDEO_SOURCE_H__ */
