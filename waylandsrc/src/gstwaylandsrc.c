/*
 * GStreamer Wayland video source
 *
 * Copyright (C) 2015 Hyunjun Ko <zzoon.ko@samsung.com>
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
#include <xf86drm.h>
#include <mm_types.h>
#include "tizen-screenshooter-client-protocol.h"
#include "tizen-buffer-pool-client-protocol.h"
#include "gstwaylandsrc.h"

GST_DEBUG_CATEGORY_STATIC (waylandsrc_debug);
#define GST_CAT_DEFAULT waylandsrc_debug

#define NUM_BUFFERS 3
#define DEFAULT_WIDTH 640
#define DEFAULT_HEIGHT 480

//#define DUMP_BUFFER
#define USE_MM_VIDEO_BUFFER

#define C(b,m)              (char)(((b) >> (m)) & 0xFF)
#define B(c,s)              ((((unsigned int)(c)) & 0xff) << (s))
#define FOURCC(a,b,c,d)     (B(d,24) | B(c,16) | B(b,8) | B(a,0))
#define FOURCC_STR(id)      C(id,0), C(id,8), C(id,16), C(id,24)
#define FOURCC_ARGB         FOURCC('A','R','G','B')
#define FOURCC_RGB32        FOURCC('R','G','B','4')
#define FOURCC_I420         FOURCC('I','4','2','0')
#define FOURCC_NV12         FOURCC('N','V','1','2')
#define FOURCC_SN12         FOURCC('S','N','1','2')
#define FOURCC_ST12         FOURCC('S','T','1','2')

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("ARGB") ";"
        GST_VIDEO_CAPS_MAKE ("NV12") ";" GST_VIDEO_CAPS_MAKE ("SN12"))
    );

enum
{
  PROP_0,
  PROP_DISPLAY_NAME,
  PROP_USE_TBM
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

static gboolean gst_wayland_src_set_caps (GstBaseSrc * psrc, GstCaps * caps);

static gboolean gst_wayland_src_start (GstBaseSrc * basesrc);

static gboolean gst_wayland_src_stop (GstBaseSrc * basesrc);

static gboolean gst_wayland_src_unlock (GstBaseSrc * basesrc);

static GstFlowReturn gst_wayland_src_create (GstPushSrc * psrc,
    GstBuffer ** buf);

static void *gst_wayland_src_capture_thread (GstWaylandSrc * src);
static gboolean gst_wayland_src_thread_start (GstWaylandSrc * src);
static void gst_wayland_src_gst_buffer_unref (gpointer data);

static void
handle_global (void *data, struct wl_registry *registry,
    uint32_t name, const char *interface, uint32_t version);

static void
handle_global_remove (void *data, struct wl_registry *registry, uint32_t name);

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
mirror_handle_dequeued (void *data,
    struct tizen_screenmirror *tizen_screenmirror, struct wl_buffer *buffer);

static void
mirror_handle_content (void *data,
    struct tizen_screenmirror *tizen_screenmirror, uint32_t content);

static void
mirror_handle_stop (void *data, struct tizen_screenmirror *tizen_screenmirror);

static const struct tizen_screenmirror_listener mirror_listener = {
  mirror_handle_dequeued,
  mirror_handle_content,
  mirror_handle_stop
};

static void
mirror_handle_dequeued (void *data,
    struct tizen_screenmirror *tizen_screenmirror, struct wl_buffer *buffer)
{
  struct output_buffer *out_buffer;
  GstWaylandSrc *src = data;
  GstBuffer *gst_buffer;
  GstClock *clock;
  GstClockTime base_time, next_capture_ts;
  gint64 next_frame_no;

  clock = gst_element_get_clock (GST_ELEMENT (src));
  if (!clock) {
    GST_ERROR_OBJECT (src, "Failed to get clock");
    return;
  }

  wl_list_for_each (out_buffer, &src->buffer_list, link) {
    if (out_buffer->wl_buffer != buffer)
      continue;

    base_time = gst_element_get_base_time (GST_ELEMENT (src));
    next_capture_ts = gst_clock_get_time (clock) - base_time;
    next_frame_no = gst_util_uint64_scale (next_capture_ts,
        src->fps_n, GST_SECOND * src->fps_d);

    GST_DEBUG_OBJECT (src, "last_frame_no : %" G_GUINT64_FORMAT
        " expected frame no : %" G_GUINT64_FORMAT,
        src->last_frame_no, next_frame_no);

    if (next_frame_no == src->last_frame_no) {
      GstClockID id;
      GstClockReturn ret;

      next_frame_no += 1;
      next_capture_ts = gst_util_uint64_scale (next_frame_no,
          src->fps_d * GST_SECOND, src->fps_n);

      id = gst_clock_new_single_shot_id (clock, next_capture_ts + base_time);

      GST_DEBUG_OBJECT (src, "Waiting for next frame time %" G_GUINT64_FORMAT,
          next_capture_ts);
      ret = gst_clock_id_wait (id, NULL);

      gst_clock_id_unref (id);
      if (ret == GST_CLOCK_UNSCHEDULED) {
        /* Gotjwoken up by the unlock function */
        GST_ERROR_OBJECT (src, "GST_CLOCK_UNSCHEDULED returned");
        continue;
      }
    }

    src->last_frame_no = next_frame_no;

    GST_DEBUG_OBJECT (src, "Buffer [%d] dequeued",
        wl_proxy_get_id ((struct wl_proxy *) buffer));

    if (src->use_tbm) {
      if (src->format == TIZEN_BUFFER_POOL_FORMAT_ARGB8888) {
        out_buffer->gst_buffer = gst_buffer_new ();
        gst_buffer_append_memory (out_buffer->gst_buffer,
            gst_memory_new_wrapped (GST_MEMORY_FLAG_NO_SHARE,
                tbm_bo_map(out_buffer->bo[0], TBM_DEVICE_CPU, TBM_OPTION_READ).ptr,
                out_buffer->size, 0, out_buffer->size, (gpointer) out_buffer,
                gst_wayland_src_gst_buffer_unref));
      } else if (src->format == TIZEN_BUFFER_POOL_FORMAT_NV12) {
#ifndef USE_MM_VIDEO_BUFFER
        out_buffer->gst_buffer = gst_buffer_new ();
        gst_buffer_append_memory (out_buffer->gst_buffer,
            gst_memory_new_wrapped (GST_MEMORY_FLAG_READONLY,
                tbm_bo_map(out_buffer->bo[0], TBM_DEVICE_CPU, TBM_OPTION_READ).ptr,
                tbm_bo_size(out_buffer->bo[0]), 0, tbm_bo_size(out_buffer->bo[0]),
                (gpointer) out_buffer, gst_wayland_src_gst_buffer_unref));
        gst_buffer_append_memory (out_buffer->gst_buffer,
            gst_memory_new_wrapped (GST_MEMORY_FLAG_READONLY,
                tbm_bo_map(out_buffer->bo[1], TBM_DEVICE_CPU, TBM_OPTION_READ).ptr,
                tbm_bo_size(out_buffer->bo[1]), 0, tbm_bo_size(out_buffer->bo[1]),
                (gpointer) out_buffer, NULL));
#else
        MMVideoBuffer *mm_video_buf = NULL;
        mm_video_buf = (MMVideoBuffer *) malloc (sizeof (MMVideoBuffer));
        if (mm_video_buf == NULL) {
          GST_ERROR_OBJECT (src, "failed to alloc MMVideoBuffer");
          return;
        }

        memset (mm_video_buf, 0x00, sizeof (MMVideoBuffer));

        mm_video_buf->type = MM_VIDEO_BUFFER_TYPE_TBM_BO;
        mm_video_buf->handle.bo[0] = out_buffer->bo[0];
        mm_video_buf->handle.bo[1] = out_buffer->bo[1];
        GST_INFO_OBJECT (src, "BO : %p %p", mm_video_buf->handle.bo[0], mm_video_buf->handle.bo[1]);

        mm_video_buf->size[0] = (src->width * src->height);
        mm_video_buf->size[1] = (src->width * (src->height >> 1));
        mm_video_buf->width[0] = src->width;
        mm_video_buf->height[0] = src->height;
        mm_video_buf->format = MM_PIXEL_FORMAT_NV12;
        mm_video_buf->width[1] = src->width;
        mm_video_buf->height[1] = src->height >> 1;

        mm_video_buf->stride_width[0] = GST_ROUND_UP_16 (mm_video_buf->width[0]);
        mm_video_buf->stride_height[0] = GST_ROUND_UP_16 (mm_video_buf->height[0]);
        mm_video_buf->stride_width[1] = GST_ROUND_UP_16 (mm_video_buf->width[1]);
        mm_video_buf->stride_height[1] = GST_ROUND_UP_16 (mm_video_buf->height[1]);
        mm_video_buf->is_secured = 0;

        out_buffer->gst_buffer = gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY,
            out_buffer, sizeof (out_buffer), 0, sizeof (out_buffer), (gpointer) out_buffer,
            gst_wayland_src_gst_buffer_unref);

        gst_buffer_append_memory (out_buffer->gst_buffer,
            gst_memory_new_wrapped (GST_MEMORY_FLAG_READONLY,
                mm_video_buf, sizeof (*mm_video_buf), 0, sizeof (*mm_video_buf),
                mm_video_buf, g_free));

#ifdef DUMP_BUFFER
        static int dump_cnt = 0;
        void *data, *data1;
        FILE *fp;

        data = tbm_bo_map (out_buffer->bo[0], TBM_DEVICE_CPU, TBM_OPTION_READ).ptr;
        if (!data) {
          GST_ERROR_OBJECT (src, "get tbm bo handle failed: %s", strerror (errno));
          return;
        }
        data1 = tbm_bo_map (out_buffer->bo[1], TBM_DEVICE_CPU, TBM_OPTION_READ).ptr;
        if (!data1) {
          GST_ERROR_OBJECT (src, "get tbm bo handle failed: %s", strerror (errno));
          return;
        }

        fp = fopen ("/root/raw.dump", "a");
        if (fp == NULL)
          return;

        if (100 < dump_cnt  && dump_cnt < 150) {
          fwrite ((char *) data, tbm_bo_size(out_buffer->bo[0]), 1, fp);
          fwrite ((char *) data1, tbm_bo_size(out_buffer->bo[1]), 1, fp);
          GST_ERROR_OBJECT (src, "Dump :%d\n", out_buffer->size);
        }

        dump_cnt++;
        fclose (fp);
#endif

#endif
      }
    } else {
      out_buffer->gst_buffer = gst_buffer_new ();
      gst_buffer_append_memory (out_buffer->gst_buffer,
          gst_memory_new_wrapped (GST_MEMORY_FLAG_NO_SHARE, out_buffer->data,
              out_buffer->size, 0, out_buffer->size, (gpointer) out_buffer,
              gst_wayland_src_gst_buffer_unref));
    }

    gst_buffer = out_buffer->gst_buffer;
    GST_BUFFER_DTS (gst_buffer) = GST_CLOCK_TIME_NONE;
    GST_BUFFER_PTS (gst_buffer) = next_capture_ts;

    g_mutex_lock (&src->queue_lock);
    g_queue_push_tail (src->buf_queue, out_buffer);
    g_mutex_unlock (&src->queue_lock);

    /* Notify src_create to get new item */
    GST_DEBUG_OBJECT (src, "signal");
    g_cond_signal (&src->queue_cond);
    break;
  }

  return;
}

static void
mirror_handle_content (void *data,
    struct tizen_screenmirror *tizen_screenmirror, uint32_t content)
{
  switch (content) {
    case TIZEN_SCREENMIRROR_CONTENT_NORMAL:
      GST_INFO ("TIZEN_SCREENMIRROR_CONTENT_NORMAL event");
      break;
    case TIZEN_SCREENMIRROR_CONTENT_VIDEO:
      GST_INFO ("TIZEN_SCREENMIRROR_CONTENT_VIDEO event");
      break;
  }
}

static void
mirror_handle_stop (void *data, struct tizen_screenmirror *tizen_screenmirror)
{
  GST_WARNING ("mirror_handle_stop");
}

static struct wl_shm_pool *
make_shm_pool (struct wl_shm *shm, int size, void **data)
{
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
shm_buffer_create (struct wl_shm *shm, gsize width, gsize height)
{
  struct output_buffer *out_buffer = NULL;
  struct wl_shm_pool *pool = NULL;
  void *data = NULL;
  gsize block_size = 0, stride = 0;

  out_buffer = g_malloc0 (sizeof *out_buffer);
  if (!out_buffer)
    return NULL;

  stride = width * 4;
  block_size = stride * height;

  pool = make_shm_pool (shm, block_size, &data);
  if (!pool) {
    return NULL;
  }

  out_buffer->wl_buffer =
      wl_shm_pool_create_buffer (pool, 0, width, height, stride,
      WL_SHM_FORMAT_ARGB8888);
  out_buffer->size = block_size;
  out_buffer->stride = stride;
  out_buffer->data = data;
  out_buffer->bo[0] = NULL;
  out_buffer->bo[1] = NULL;

  wl_shm_pool_destroy (pool);

  return out_buffer;
}

static gboolean
check_format (GstWaylandSrc * src)
{
  struct tbm_buffer_pool_data *pdata = NULL;
  struct tbm_buffer_format *fmt;

  pdata = tizen_buffer_pool_get_user_data (src->tbm_buffer_pool);
  if (pdata == NULL) {
    GST_ERROR_OBJECT (src, "TBM buffer pool DATA is NULL");
    return FALSE;
  }

  wl_list_for_each (fmt, &pdata->support_format_list, link)
      if (fmt->format == src->format)
    return TRUE;

  return FALSE;
}

static void
destroy_buffer (struct output_buffer *obuffer)
{
  if (!obuffer)
    return;

  if (obuffer->wl_buffer)
    wl_buffer_destroy (obuffer->wl_buffer);
  if (obuffer->bo[0])
    tbm_bo_unref (obuffer->bo[0]);
  if (obuffer->bo[1])
    tbm_bo_unref (obuffer->bo[1]);
  if (obuffer->data)
    munmap (obuffer->data, obuffer->size);

  wl_list_remove (&obuffer->link);
  free (obuffer);
}

static void
destroy_mirror_buffer_pool (struct tizen_buffer_pool *pool)
{
  struct tbm_buffer_pool_data *pdata = tizen_buffer_pool_get_user_data (pool);

  if (pdata) {
    struct tbm_buffer_format *fmt, *ff;

    wl_list_for_each_safe (fmt, ff, &pdata->support_format_list, link) {
      wl_list_remove (&fmt->link);
      free (fmt);
    }
    if (pdata->bufmgr)
      tbm_bufmgr_deinit (pdata->bufmgr);
    if (pdata->drm_fd >= 0)
      close (pdata->drm_fd);
    if (pdata->device_name)
      free (pdata->device_name);
    free (pdata);
  }

  tizen_buffer_pool_destroy (pool);
}

static void
buffer_pool_handle_device (void *data,
    struct tizen_buffer_pool *tizen_buffer_pool, const char *device_name)
{
  struct tbm_buffer_pool_data *pdata = (struct tbm_buffer_pool_data *) data;

  if (pdata == NULL) {
    GST_INFO ("Name");
    return;
  }
  if (device_name == NULL) {
    GST_INFO ("Name");
    return;
  }

  pdata->device_name = strdup (device_name);
  GST_INFO ("Name : %d, device name : %s", pdata->name, pdata->device_name);
}

static void
buffer_pool_handle_authenticated (void *data,
    struct tizen_buffer_pool *tizen_buffer_pool)
{
  struct tbm_buffer_pool_data *pdata = (struct tbm_buffer_pool_data *) data;

  GST_INFO ("Name");
  if (pdata == NULL)
    return;

  /* authenticated */
  pdata->authenticated = 1;
  GST_INFO ("Name : %d", pdata->name);
}

static void
buffer_pool_handle_capabilities (void *data,
    struct tizen_buffer_pool *tizen_buffer_pool, uint32_t value)
{
  struct tbm_buffer_pool_data *pdata = (struct tbm_buffer_pool_data *) data;
  drm_magic_t magic;
  GstWaylandSrc *src = pdata->src;

  if (pdata == NULL)
    return;

  GST_INFO ("Name : %d Value : %x", pdata->name, value);

  /* check if buffer_pool has screenmirror capability */
  if (!(value & TIZEN_BUFFER_POOL_CAPABILITY_SCREENMIRROR))
    return;

  pdata->has_capability = 1;

  /* do authenticate only if a pool has the video capability */
#ifdef O_CLOEXEC
  pdata->drm_fd = open (pdata->device_name, O_RDWR | O_CLOEXEC);
  if (pdata->drm_fd == -1 && errno == EINVAL)
#endif
  {
    pdata->drm_fd = open (pdata->device_name, O_RDWR);
    if (pdata->drm_fd != -1)
      fcntl (pdata->drm_fd, F_SETFD, fcntl (pdata->drm_fd,
              F_GETFD) | FD_CLOEXEC);
  }

  if (pdata->drm_fd < 0)
    return;

  if (drmGetMagic (pdata->drm_fd, &magic) != 0) {
    close (pdata->drm_fd);
    pdata->drm_fd = -1;
    return;
  }

  tizen_buffer_pool_authenticate (tizen_buffer_pool, magic);

  if (src)
    wl_display_roundtrip (src->display);
}

static void
buffer_pool_handle_format (void *data,
    struct tizen_buffer_pool *tizen_buffer_pool, uint32_t format)
{
  struct tbm_buffer_pool_data *pdata = (struct tbm_buffer_pool_data *) data;
  struct tbm_buffer_format *fmt;

  if (pdata == NULL)
    return;

  if (!pdata->has_capability)
    return;

  fmt = g_malloc0 (sizeof (struct tbm_buffer_format));
  if (fmt == NULL)
    return;

  fmt->format = format;
  wl_list_insert (&pdata->support_format_list, &fmt->link);

  GST_INFO ("Format : %c%c%c%c", FOURCC_STR (format));
}

static const struct tizen_buffer_pool_listener buffer_pool_listener = {
  buffer_pool_handle_device,
  buffer_pool_handle_authenticated,
  buffer_pool_handle_capabilities,
  buffer_pool_handle_format
};

static struct output_buffer *
tbm_buffer_create (GstWaylandSrc * src)
{
  struct output_buffer *out_buffer = NULL;
  struct tbm_buffer_pool_data *pdata = NULL;
  gsize block_size = 0, stride = 0;

  if (src->tbm_buffer_pool == NULL) {
    GST_ERROR_OBJECT (src, "TBM buffer pool is NULL");
    return FALSE;
  }

  if (!check_format (src)) {
    GST_ERROR_OBJECT (src, "Unsupported format '%c%c%c%c'",
        FOURCC_STR (src->format));
    return NULL;
  }

  pdata = tizen_buffer_pool_get_user_data (src->tbm_buffer_pool);
  if (pdata == NULL || pdata->bufmgr == NULL) {
    GST_ERROR_OBJECT (src, "TBM buffer pool DATA is NULL");
    return FALSE;
  }


  out_buffer = g_malloc0 (sizeof *out_buffer);
  if (!out_buffer)
    return NULL;

  switch (src->format) {
    case TIZEN_BUFFER_POOL_FORMAT_ARGB8888:
    case TIZEN_BUFFER_POOL_FORMAT_XRGB8888:
      stride = src->width * 4;
      block_size = stride * src->height;

      out_buffer->bo[0] =
          tbm_bo_alloc (pdata->bufmgr, block_size, TBM_BO_DEFAULT);
      if (out_buffer->bo[0] == NULL)
        goto failed;

      out_buffer->wl_buffer =
          tizen_buffer_pool_create_buffer (src->tbm_buffer_pool,
          tbm_bo_export (out_buffer->bo[0]),
          src->width, src->height, stride, src->format);

      out_buffer->size = block_size;
      out_buffer->stride = stride;
      break;
    case TIZEN_BUFFER_POOL_FORMAT_NV12:
    case TIZEN_BUFFER_POOL_FORMAT_NV21:
      out_buffer->bo[0] =
          tbm_bo_alloc (pdata->bufmgr, src->width * src->height,
          TBM_BO_DEFAULT);
      if (out_buffer->bo[0] == NULL)
        goto failed;

      out_buffer->bo[1] =
          tbm_bo_alloc (pdata->bufmgr, src->width * src->height * 0.5,
          TBM_BO_DEFAULT);
      if (out_buffer->bo[1] == NULL)
        goto failed;

      out_buffer->wl_buffer =
          tizen_buffer_pool_create_planar_buffer (src->tbm_buffer_pool,
          src->width, src->height, src->format,
          tbm_bo_export (out_buffer->bo[0]), 0, src->width,
          tbm_bo_export (out_buffer->bo[1]), 0, src->width, 0, 0, 0);

      out_buffer->size = src->width * src->height * 1.5;
      break;
    default:
      GST_WARNING_OBJECT (src, "unknown format");
      break;
  }

  if (out_buffer->wl_buffer == NULL)
    goto failed;

  return out_buffer;

failed:
  destroy_buffer (out_buffer);
  return NULL;
}

static gboolean
gst_wayland_src_thread_start (GstWaylandSrc * src)
{
  if (!src->updates_thread) {
    src->updates_thread =
        g_thread_new ("wayland_capture",
        (GThreadFunc) gst_wayland_src_capture_thread, src);

    GST_WARNING_OBJECT (src, "image capture thread start");
  } else {
    GST_LOG_OBJECT (src, "The thread function already running");
  }

  return TRUE;
}

static void *
gst_wayland_src_capture_thread (GstWaylandSrc * src)
{
  struct pollfd pfd;
  struct output *output = NULL;
  struct output_buffer *out_buffer = NULL;
  int i;

  output = gst_wayland_src_active_output (src);
  if (!output)
    return NULL;

  for (i = 0; i < NUM_BUFFERS; i++) {
    if (src->use_tbm) {
      out_buffer = tbm_buffer_create (src);
    } else {
      out_buffer = shm_buffer_create (src->shm, output->width, output->height);
    }

    if (!out_buffer) {
      GST_ERROR ("failed to create buffer\n");
      return NULL;
    }

    out_buffer->output = output;

    wl_list_insert (&src->buffer_list, &out_buffer->link);
    GST_INFO ("Buffer [%d] is inserted", i);
    tizen_screenmirror_queue (src->screenmirror, out_buffer->wl_buffer);
  }

  tizen_screenmirror_start (src->screenmirror);
  wl_display_roundtrip (src->display);

  pfd.fd = wl_display_get_fd (src->display);
  pfd.events = POLLIN;

  /* thread loop */
  while (!src->thread_return) {
    while (wl_display_prepare_read_queue (src->display, src->queue) != 0)
      wl_display_dispatch_queue_pending (src->display, src->queue);

    wl_display_flush (src->display);

    if (poll (&pfd, 1, -1) < 0) {
      wl_display_cancel_read (src->display);
      break;
    } else {
      wl_display_read_events (src->display);
      wl_display_dispatch_queue_pending (src->display, src->queue);
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

  g_object_class_install_property (gc, PROP_USE_TBM,
      g_param_spec_boolean ("use-tbm", "Use Tizen Buffer Object",
          "Use Tizen Buffer Object", FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));


  gst_element_class_set_static_metadata (ec,
      "tizen wayland source", "Source/Video",
      "captures a wayland output", "Hyunjun Ko <zzoon.ko@samsung.com>");
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
  src->fps_n = src->fps_d = 0;
  src->buffer_copy_done = 0;
  wl_list_init (&src->output_list);
  wl_list_init (&src->buffer_list);

  src->buf_queue = g_queue_new ();

  g_mutex_init (&src->queue_lock);
  g_mutex_init (&src->cond_lock);
  g_cond_init (&src->queue_cond);

  src->tbm_buffer_pool = NULL;
  src->format = FOURCC_ARGB;
  src->width = DEFAULT_WIDTH;
  src->height = DEFAULT_HEIGHT;

  src->use_tbm = TRUE;
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
    wl_registry_destroy (src->registry);

  wl_list_for_each_safe (output, output_tmp, &src->output_list, link) {
    if (output->output)
      wl_output_destroy (output->output);
    wl_list_remove (&output->link);
    g_free (output);
  }

  wl_list_for_each_safe (out_buffer, out_buffer_tmp, &src->buffer_list, link) {
    destroy_buffer (out_buffer);
  }

  if (src->shm)
    wl_shm_destroy (src->shm);

  if (src->screenmirror)
    tizen_screenmirror_destroy (src->screenmirror);
  if (src->screenshooter)
    tizen_screenshooter_destroy (src->screenshooter);
  if (src->tbm_buffer_pool)
    tizen_buffer_pool_destroy (src->tbm_buffer_pool);

  if (src->queue)
    wl_event_queue_destroy (src->queue);

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

  wl_display_dispatch_queue (src->display, src->queue);
  wl_display_roundtrip_queue (src->display, src->queue);

  /* check wayland objects */
  if (src->shm == NULL) {
    GST_ERROR_OBJECT (src, "display doesn't support tizen screenshooter\n");
    return FALSE;
  }
  if (src->screenshooter == NULL) {
    GST_ERROR_OBJECT (src, "display doesn't support tizen screenshooter\n");
    return FALSE;
  }

  struct output *output;

  output = gst_wayland_src_active_output (src);
  if (output == NULL) {
    GST_ERROR_OBJECT (src, "output is NULL");
    return FALSE;
  }

  src->screenmirror =
      tizen_screenshooter_get_screenmirror (src->screenshooter, output->output);
  if (src->screenmirror == NULL) {
    GST_ERROR_OBJECT (src, "tizen screenmirror is NULL");
    return FALSE;
  }

  wl_proxy_set_queue ((struct wl_proxy *) src->screenmirror, src->queue);
  tizen_screenmirror_add_listener (src->screenmirror, &mirror_listener, src);
  tizen_screenmirror_set_stretch (src->screenmirror,
      TIZEN_SCREENMIRROR_STRETCH_KEEP_RATIO);

  GST_INFO_OBJECT (src, "gst_wayland_src_connect success");
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
  wl_list_for_each (it, &src->output_list, link) {
    /* return first output */
    return it;
  }

  return NULL;
}

static void
gst_wayland_src_dispose (GObject * object)
{
}

static void
gst_wayland_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstWaylandSrc *src = GST_WAYLAND_SRC (object);

  switch (prop_id) {
    case PROP_DISPLAY_NAME:
      if (src->display) {
        GST_WARNING_OBJECT (src,
            "display name must be set before opening display");
        break;
      }
      if (src->display_name)
        g_free (src->display_name);
      src->display_name = g_strdup (g_value_get_string (value));
      break;
    case PROP_USE_TBM:
      src->use_tbm = g_value_get_boolean (value);
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
    case PROP_USE_TBM:
      g_value_set_boolean (value, src->use_tbm);
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
  caps = GST_BASE_SRC_CLASS (parent_class)->fixate (bsrc, caps);

  return caps;
}

static gboolean
gst_wayland_src_set_caps (GstBaseSrc * psrc, GstCaps * caps)
{
  GstWaylandSrc *src = GST_WAYLAND_SRC (psrc);
  GstStructure *structure;
  const GValue *fps;
  gint width = 0;
  gint height = 0;
  gboolean ret = TRUE;
  const gchar *media_type = NULL;

  GST_WARNING_OBJECT (src, "set_caps : %" GST_PTR_FORMAT, caps);
  if (!src->display)
    return FALSE;

  structure = gst_caps_get_structure (caps, 0);

  ret = gst_structure_get_int (structure, "width", &width);
  if (!ret) {
    GST_WARNING_OBJECT (src, "waylandsrc width not specified in caps");
  } else {
    if (width > 1)
     src->width = (guint) width;
  }

  ret = gst_structure_get_int (structure, "height", &height);
  if (!ret) {
    GST_WARNING_OBJECT (src, "waylandsrc height not specified in caps");
  } else {
    if (height > 1)
      src->height = (guint) height;
  }

  media_type = gst_structure_get_name (structure);
  if (media_type != NULL) {

    if (g_strcmp0 (media_type, "video/x-raw") == 0) {
      const gchar *sformat = NULL;

      sformat = gst_structure_get_string (structure, "format");
      if (!sformat) {
        GST_WARNING_OBJECT (src,
            "waylandsrc format not specified in caps.. Using default ARGB");
      } else {
        src->format = FOURCC (sformat[0], sformat[1], sformat[2], sformat[3]);
        if (src->format == FOURCC_SN12)
          src->format = FOURCC_NV12;
      }
    } else {
      GST_ERROR_OBJECT (src, "type is not video/x-raw");
      return FALSE;
    }
  } else {
    GST_WARNING_OBJECT (src, "waylandsrc media-type not specified in caps");
  }

  fps = gst_structure_get_value (structure, "framerate");
  if (!fps) {
    GST_WARNING_OBJECT (src, "waylandsrc fps not specified in caps");
  } else {
    src->fps_n = gst_value_get_fraction_numerator (fps);
    src->fps_d = gst_value_get_fraction_denominator (fps);

  }

  if (src->use_tbm && src->format == FOURCC_ARGB)
    src->format = TIZEN_BUFFER_POOL_FORMAT_ARGB8888;

  GST_INFO_OBJECT (src, "format:%c%c%c%c, width: %d, height: %d",
      FOURCC_STR (src->format), src->width, src->height);
  GST_INFO_OBJECT (src, "FPS %d/%d", src->fps_n, src->fps_d);
  gst_wayland_src_thread_start (src);

  return TRUE;
}

static gboolean
gst_wayland_src_start (GstBaseSrc * basesrc)
{
  GstWaylandSrc *src = GST_WAYLAND_SRC (basesrc);

  if (!src->display && !gst_wayland_src_connect (src)) {
    GST_ERROR_OBJECT (src, "gst_wayland_src_connect failed");
    return FALSE;
  }

  if (!src->display) {
    GST_ERROR_OBJECT (src, "Not setup");
    return FALSE;
  }

  src->last_frame_no = -1;

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
    out_buffer = (struct output_buffer *) g_queue_pop_head (src->buf_queue);
    GST_DEBUG_OBJECT (src, "Got a buffer.. %d buffers remaining..",
        g_queue_get_length (src->buf_queue));
    g_mutex_unlock (&src->queue_lock);
  }

  if (out_buffer == NULL) {
    GST_ERROR_OBJECT (src, "Failed to get a buffer");
    return GST_FLOW_ERROR;
  }

  *ret_buf = out_buffer->gst_buffer;

  GST_INFO_OBJECT (src,
      "Create gst buffer [%d] for wl_buffer[%d] (Size:%d (%" GST_TIME_FORMAT
      " PTS: %" G_GINT64_FORMAT ")", gst_buffer_get_size (out_buffer->gst_buffer),
      wl_proxy_get_id ((struct wl_proxy *) out_buffer->wl_buffer),
      out_buffer->size, GST_TIME_ARGS (GST_BUFFER_PTS (*ret_buf)),
      GST_BUFFER_PTS (*ret_buf));

  return GST_FLOW_OK;
}

static void
gst_wayland_src_gst_buffer_unref (gpointer data)
{
  struct output_buffer *out_buffer = data;
  struct output *output = out_buffer->output;
  GstWaylandSrc *src = output->src;

  if (src->use_tbm) {
    if (out_buffer->bo[0])
      tbm_bo_unmap (out_buffer->bo[0]);
    if (out_buffer->bo[1])
      tbm_bo_unmap (out_buffer->bo[1]);
  }

  tizen_screenmirror_queue (src->screenmirror, out_buffer->wl_buffer);
  GST_DEBUG ("Buffer [%d] queued",
      wl_proxy_get_id ((struct wl_proxy *) out_buffer->wl_buffer));

  wl_display_sync (src->display);
}

static void
handle_global (void *data, struct wl_registry *registry,
    uint32_t name, const char *interface, uint32_t version)
{
  GstWaylandSrc *src = data;
  struct output *output;

  if (strcmp (interface, "wl_output") == 0) {
    output = g_malloc0 (sizeof *output);
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
  } else if (strcmp (interface, "tizen_buffer_pool") == 0) {
    struct tizen_buffer_pool *pool;
    struct tbm_buffer_pool_data *pdata;

    pool = wl_registry_bind (registry, name, &tizen_buffer_pool_interface, 1);
    if (pool == NULL) {
      GST_ERROR_OBJECT (src, "wl_registry_bind failed");
      return;
    }

    pdata = calloc (1, sizeof (struct tbm_buffer_pool_data));
    if (!pdata) {
      tizen_buffer_pool_destroy (pool);
      GST_ERROR_OBJECT (src, "Allcation buffer pool failed");
      return;
    }

    /* initial value */
    pdata->name = name;
    pdata->drm_fd = -1;
    pdata->src = src;
    wl_list_init (&pdata->support_format_list);

    tizen_buffer_pool_add_listener (pool, &buffer_pool_listener, pdata);

    /* make sure all tizen_buffer_pool's events are handled */
    wl_display_roundtrip_queue (src->display, src->queue);

    if (!pdata->has_capability || !pdata->authenticated)
      destroy_mirror_buffer_pool (pool);
    else {
      pdata->bufmgr = tbm_bufmgr_init (pdata->drm_fd);
      if (!pdata->bufmgr) {
        destroy_mirror_buffer_pool (pool);
        return;
      }

      src->tbm_buffer_pool = pool;
      GST_INFO ("tizen_buffer_pool is binded");
    }
  } else if (strcmp (interface, "tizen_screenshooter") == 0) {
    src->screenshooter =
        wl_registry_bind (registry, name, &tizen_screenshooter_interface, 1);
    if (src->screenshooter == NULL) {
      GST_ERROR_OBJECT (src, "tizen screenshooter is NULL");
      return;
    }
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
  struct output *output = wl_output_get_user_data (wl_output);

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
