/*
 * GStreamer Tizen IPC src
 *
 * Copyright (C) 2015 Jeongmo Yang <jm80.yang@samsung.com>
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

#ifndef __GST_TIZEN_IPC_SRC_H__
#define __GST_TIZEN_IPC_SRC_H__

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include <gst/base/gstbasesrc.h>
#include <tbm_bufmgr.h>
#include <mm_types.h>

G_BEGIN_DECLS

#define GST_TYPE_TIZENIPC_SRC (gst_tizenipc_src_get_type())
#define GST_TIZENIPC_SRC(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_TIZENIPC_SRC, GstTizenipcSrc))
#define GST_TIZENIPC_SRC_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_TIZENIPC_SRC, GstTizenipcSrcClass))
#define GST_IS_TIZENIPC_SRC(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_TIZENIPC_SRC))
#define GST_IS_TIZENIPC_SRC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_TIZENIPC_SRC))
#define GST_TIZENIPC_SRC_GET_CLASS(inst) (G_TYPE_INSTANCE_GET_CLASS((inst), GST_TYPE_TIZENIPC_SRC, GstTizenipcSrcClass))

#define GST_TIZENIPC_BUFFER_MAX 30

typedef struct _GstTizenipcSrc GstTizenipcSrc;
typedef struct _GstTizenipcSrcClass GstTizenipcSrcClass;
typedef struct _GstTizenipcSrcBuffer GstTizenipcSrcBuffer;
typedef struct _GstTizenipcMessage GstTizenipcMessage;

struct _GstTizenipcSrc {
  GstPushSrc parent;

  /* ipc */
  int socket_fd;
  int shm_fd;
  GstPoll *poll;
  GstPollFD pollfd;
  gchar *shm_path;
  gchar *shm_mapped_area;
  gint shm_mapped_size;

  /* Property */
  gchar *socket_path;

  /* buffer management */
  tbm_bufmgr bufmgr;
  guint live_buffer_count;
  GMutex buffer_lock;
  GCond buffer_cond;
};

struct _GstTizenipcSrcClass {
  GstPushSrcClass parent_class;
};

struct _GstTizenipcSrcBuffer {
  GstBuffer *gst_buf;
  MMVideoBuffer *mm_buf;
  int tbm_key[MM_VIDEO_BUFFER_PLANE_MAX];
  GstTizenipcSrc *self;
};


enum {
  TIZEN_IPC_SHM_PATH = 0,
  TIZEN_IPC_BUFFER_NEW,
  TIZEN_IPC_BUFFER_RECEIVED,
  TIZEN_IPC_BUFFER_RELEASE
};

struct _GstTizenipcMessage {
  int type;
  union {
    int size;
    int tbm_key[MM_VIDEO_BUFFER_PLANE_MAX];
  };
};

GType
gst_tizenipc_src_get_type (void)
    G_GNUC_CONST;

G_END_DECLS

#endif /* __GST_TIZEN_IPC_SRC_H__ */
