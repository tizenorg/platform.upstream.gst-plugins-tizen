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
#include <tbm_bufmgr.h>
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

struct output
{
  struct wl_output *output;
  GstWaylandSrc *src;
  guint width;
  guint height;
  guint offset_x;
  guint offset_y;
  struct wl_list link;
};


struct output_buffer
{
  struct output *output;

  /* for shm buffer */
  void *data;

  /* for tbm buffer */
  tbm_bo bo[2];
  tbm_surface_h surface;

  struct wl_buffer *wl_buffer;
  GstBuffer *gst_buffer;
  int stride, size, offset;
  struct wl_list link;
};

struct tbm_buffer_format
{
  guint32 format;
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
  gboolean use_tbm;
  guint fps_n;
  guint fps_d;
  gint64 last_frame_no;
  int buffer_copy_done;
  struct wl_list output_list;
  struct wl_list buffer_list;
  struct wl_list support_format_list;

  GQueue *buf_queue;
  GMutex queue_lock;
  GMutex cond_lock;
  GCond queue_cond;
  GThread *updates_thread;
  gboolean thread_return;

  guint width;
  guint height;
  guint32 format;
  struct wayland_tbm_client *tbm_client;
};

struct _GstWaylandSrcClass
{
  GstPushSrcClass parent;
};

#ifdef TIZEN_PROFILE_LITE
struct ion_mmu_data {
        int master_id;
        int fd_buffer;
        unsigned long iova_addr;
        size_t iova_size;
};
#endif

GType
gst_wayland_src_get_type (void)
    G_GNUC_CONST;

/*MFC Buffer alignment macros*/
#define S5P_FIMV_DEC_BUF_ALIGN                  (8 * 1024)
#define S5P_FIMV_ENC_BUF_ALIGN                  (8 * 1024)
#define S5P_FIMV_NV12M_HALIGN                   16
#define S5P_FIMV_NV12M_LVALIGN                  16
#define S5P_FIMV_NV12M_CVALIGN                  8
#define S5P_FIMV_NV12MT_HALIGN                  128
#define S5P_FIMV_NV12MT_VALIGN                  64
#define S5P_FIMV_NV12M_SALIGN                   2048
#define S5P_FIMV_NV12MT_SALIGN                  8192

#define ALIGN(x, a)       (((x) + (a) - 1) & ~((a) - 1))

/* Buffer alignment defines */
#define SZ_1M                                   0x00100000
#define S5P_FIMV_D_ALIGN_PLANE_SIZE             64

#define S5P_FIMV_MAX_FRAME_SIZE                 (2 * SZ_1M)
#define S5P_FIMV_NUM_PIXELS_IN_MB_ROW           16
#define S5P_FIMV_NUM_PIXELS_IN_MB_COL           16

/* Macro */
#define ALIGN_TO_4KB(x)   ((((x) + (1 << 12) - 1) >> 12) << 12)
#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))
#define CHOOSE_MAX_SIZE(a,b) ((a) > (b) ? (a) : (b))
G_END_DECLS
#endif /* __GST_WAYLAND_VIDEO_SOURCE_H__ */
