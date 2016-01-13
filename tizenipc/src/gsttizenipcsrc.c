/*
 * GStreamer Tizen IPC source
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include "gsttizenipcsrc.h"

#define DEFAULT_SOCKET_PATH "/tmp/tizenipc.0"
#define DEFAULT_SHM_PATH    "/tizenipcshm"
#define DEFAULT_PERMISSIONS (S_IRUSR|S_IWUSR|S_IRGRP)
#define DEFAULT_BACKLOG     5
#define CLIENT_RESPONSE_TIMEOUT (G_TIME_SPAN_MILLISECOND * 200)
#define BUFFER_WAIT_TIMEOUT     (G_TIME_SPAN_MILLISECOND * 3000)

GST_DEBUG_CATEGORY(gst_debug_tizenipc_src);

#define GST_CAT_DEFAULT gst_debug_tizenipc_src

static GstStaticPadTemplate srctemplate = \
    GST_STATIC_PAD_TEMPLATE("src",
                            GST_PAD_SRC,
                            GST_PAD_ALWAYS,
                            GST_STATIC_CAPS_ANY);

#define gst_tizenipc_src_parent_class parent_class
G_DEFINE_TYPE(GstTizenipcSrc, gst_tizenipc_src, GST_TYPE_PUSH_SRC);

/* signals */
enum {
  LAST_SIGNAL
};

/* properties */
enum {
  PROP_0,
  PROP_SOCKET_PATH,
  PROP_IS_LIVE
};



static void gst_tizenipc_src_finalize(GObject *object);
static void gst_tizenipc_src_set_property(GObject *object, guint prop_id,
                                          const GValue *value, GParamSpec *pspec);
static void gst_tizenipc_src_get_property(GObject *object, guint prop_id,
                                          GValue *value, GParamSpec *pspec);
static gboolean gst_tizenipc_src_start(GstBaseSrc *bsrc);
static gboolean gst_tizenipc_src_stop(GstBaseSrc *bsrc);
static GstFlowReturn gst_tizenipc_src_create(GstPushSrc *psrc, GstBuffer **outbuf);
static gboolean gst_tizenipc_src_unlock(GstBaseSrc *bsrc);
static gboolean gst_tizenipc_src_unlock_stop(GstBaseSrc *bsrc);
static GstStateChangeReturn gst_tizenipc_src_change_state(GstElement *element, GstStateChange transition);
static void gst_tizenipc_src_buffer_finalize(GstTizenipcSrcBuffer *buffer);



static gboolean _tizenipc_src_prepare_to_read(GstTizenipcSrc *self)
{
  struct sockaddr_un addr_un;
  struct sockaddr *address = NULL;
  socklen_t address_len = 0;
  int flags = 0;

  if (self == NULL) {
    GST_ERROR("NULL instance");
    return FALSE;
  }

  if (self->socket_path == NULL) {
    GST_ERROR_OBJECT(self, "socket path is NULL");
    return FALSE;
  }

  if (self->bufmgr == NULL) {
    GST_ERROR_OBJECT(self, "tbm bufmgr is not initialized");
    return FALSE;
  }

  GST_INFO_OBJECT(self, "start");

  /* socket connection */
  self->socket_fd = socket(PF_UNIX, SOCK_STREAM, 0);
  if (self->socket_fd < 0) {
    GST_ERROR_OBJECT(self, "failed to open socket");
    goto _PREPARE_FAILED;
  }

  flags = fcntl(self->socket_fd, F_GETFL, 0);
  if (flags < 0) {
    GST_ERROR_OBJECT(self, "failed to fcntl F_GETFL for socket fd %d", self->socket_fd);
    goto _PREPARE_FAILED;
  }

  if (fcntl(self->socket_fd, F_SETFL, flags|FD_CLOEXEC) < 0) {
    GST_ERROR_OBJECT(self, "failed to fcntl F_SETFL FD_CLOEXEC for socket fd %d", self->socket_fd);
    goto _PREPARE_FAILED;
  }

  addr_un.sun_family = AF_UNIX;
  strncpy(addr_un.sun_path, self->socket_path, sizeof(addr_un.sun_path)-1);

  address = (struct sockaddr *)(&addr_un);
  address_len = sizeof(addr_un);

  if (connect(self->socket_fd, address, address_len) < 0) {
    GST_ERROR_OBJECT(self, "failed to connect for socket fd %d", self->socket_fd);
    goto _PREPARE_FAILED;
  }

  /* gst poll init */
  gst_poll_set_flushing(self->poll, FALSE);
  gst_poll_fd_init(&self->pollfd);
  self->pollfd.fd = self->socket_fd;
  gst_poll_add_fd(self->poll, &self->pollfd);
  gst_poll_fd_ctl_read(self->poll, &self->pollfd, TRUE);

  GST_INFO_OBJECT(self, "done - socket fd %d", self->socket_fd);

  return TRUE;

_PREPARE_FAILED:
  if (self->socket_fd >= 0) {
    shutdown(self->socket_fd, SHUT_RDWR);
    close(self->socket_fd);
    self->socket_fd = -1;
  }

  if (self->socket_path) {
    unlink(self->socket_path);
  }

  return FALSE;
}


static gboolean _tizenipc_src_stop_to_read(GstTizenipcSrc *self)
{
  gint64 wait_end_time = 0;

  if (self == NULL) {
    GST_ERROR("NULL instance");
    return FALSE;
  }

  GST_INFO_OBJECT(self, "start - socket fd %d, live buffer count %d",
                        self->socket_fd, self->live_buffer_count);

  /* wait for buffers */
  g_mutex_lock(&self->buffer_lock);

  while (self->live_buffer_count > 0) {
    wait_end_time = g_get_monotonic_time () + BUFFER_WAIT_TIMEOUT;
    if (!g_cond_wait_until(&self->buffer_cond, &self->buffer_lock, wait_end_time)) {
      GST_WARNING_OBJECT(self, "wait timeout - current count %d",
                               self->live_buffer_count);
      break;
    } else {
      GST_WARNING_OBJECT(self, "signal received - current count %d",
                               self->live_buffer_count);
    }
  }

  g_mutex_unlock(&self->buffer_lock);

  if (self->socket_fd >= 0) {
    shutdown(self->socket_fd, SHUT_RDWR);
    close(self->socket_fd);
    self->socket_fd = -1;
  }

  if (self->socket_path) {
    unlink(self->socket_path);
  }

  if (self->shm_mapped_area) {
    munmap(self->shm_mapped_area, self->shm_mapped_size);
    self->shm_mapped_area = MAP_FAILED;
  }

  if (self->shm_fd) {
    close(self->shm_fd);
    self->shm_fd = -1;
  }

  GST_INFO_OBJECT(self, "done");

  return TRUE;
}


/* ---------------------- */
/*      MAIN METHODS      */
/* ---------------------- */

static void gst_tizenipc_src_init(GstTizenipcSrc *self)
{
  g_mutex_init(&self->buffer_lock);
  g_cond_init(&self->buffer_cond);

  self->socket_fd = -1;
  self->shm_fd = -1;
  self->shm_mapped_area = MAP_FAILED;
  self->bufmgr = tbm_bufmgr_init(-1);
  self->socket_path = g_strdup(DEFAULT_SOCKET_PATH);
  if (self->socket_path == NULL) {
    GST_ERROR_OBJECT(self, "failed to dup socket path [%s]", DEFAULT_SOCKET_PATH);
  }
  self->poll = gst_poll_new(TRUE);
  if (self->poll == NULL) {
    GST_ERROR_OBJECT(self, "failed to get gst poll");
  } else {
    gst_poll_fd_init(&self->pollfd);
  }

  return;
}

static void gst_tizenipc_src_class_init(GstTizenipcSrcClass *klass)
{
  GObjectClass *gobject_class = NULL;
  GstElementClass *gstelement_class = NULL;
  GstBaseSrcClass *gstbasesrc_class = NULL;
  GstPushSrcClass *gstpush_src_class = NULL;
  GParamSpec *pspec = NULL;

  gobject_class = (GObjectClass *)klass;
  gstelement_class = (GstElementClass *)klass;
  gstbasesrc_class = (GstBaseSrcClass *)klass;
  gstpush_src_class = (GstPushSrcClass *)klass;

  gobject_class->set_property = gst_tizenipc_src_set_property;
  gobject_class->get_property = gst_tizenipc_src_get_property;
  gobject_class->finalize = gst_tizenipc_src_finalize;

  gstelement_class->change_state = gst_tizenipc_src_change_state;

  gstbasesrc_class->start = GST_DEBUG_FUNCPTR(gst_tizenipc_src_start);
  gstbasesrc_class->stop = GST_DEBUG_FUNCPTR(gst_tizenipc_src_stop);
  gstbasesrc_class->unlock = GST_DEBUG_FUNCPTR(gst_tizenipc_src_unlock);
  gstbasesrc_class->unlock_stop = GST_DEBUG_FUNCPTR(gst_tizenipc_src_unlock_stop);

  gstpush_src_class->create = gst_tizenipc_src_create;

  /* property */
  pspec = g_param_spec_string("socket-path",
                              "Path to the control socket",
                              "The path to the control socket used to handle IPC",
                              NULL,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  if (pspec) {
    g_object_class_install_property(gobject_class, PROP_SOCKET_PATH, pspec);
  } else {
    GST_ERROR("failed to get pspec for \"socket-path\"");
  }

  pspec = g_param_spec_boolean ("is-live", "Is this a live source",
                                "True if the element cannot produce data in PAUSED",
                                FALSE,
                                G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  if (pspec) {
    g_object_class_install_property (gobject_class, PROP_IS_LIVE, pspec);
  } else {
    GST_ERROR("failed to get pspec for \"is-live\"");
  }

  gst_element_class_add_pad_template(gstelement_class,
                                     gst_static_pad_template_get(&srctemplate));

  gst_element_class_set_static_metadata(gstelement_class,
                                        "Tizen IPC Source",
                                        "Source",
                                        "Receive data via IPC from the tizenipcsink",
                                        "Jeongmo Yang <jm80.yang@samsung.com>");

  return;
}


static void gst_tizenipc_src_finalize(GObject *object)
{
  GstTizenipcSrc *self = NULL;

  if (object == NULL) {
    GST_ERROR("NULL object");
    return;
  }

  self = GST_TIZENIPC_SRC(object);
  if (self == NULL) {
    GST_ERROR_OBJECT(object, "failed to cast to GST_TIZENIPC_SRC with %p", object);
    return;
  }

  GST_INFO_OBJECT(self, "start");

  g_mutex_clear(&self->buffer_lock);
  g_cond_clear(&self->buffer_cond);

  if (self->socket_path) {
    g_free(self->socket_path);
    self->socket_path = NULL;
  }

  if (self->shm_path) {
    g_free(self->shm_path);
    self->shm_path = NULL;
  }

  if (self->poll) {
    gst_poll_free(self->poll);
    self->poll = NULL;
  }

  if (self->bufmgr) {
    tbm_bufmgr_deinit(self->bufmgr);
    self->bufmgr = NULL;
  }

  GST_INFO_OBJECT(self, "done");

  G_OBJECT_CLASS(parent_class)->finalize(object);

  return;
}


static void gst_tizenipc_src_set_property(GObject *object, guint prop_id,
                                          const GValue *value, GParamSpec *pspec)
{
  GstTizenipcSrc *self = NULL;

  if (object == NULL) {
    GST_ERROR("NULL object");
    return;
  }

  self = GST_TIZENIPC_SRC(object);
  if (self == NULL) {
    GST_ERROR_OBJECT(object, "failed to cast to GST_TIZENIPC_SRC with %p", object);
    return;
  }

  switch (prop_id) {
    case PROP_SOCKET_PATH:
    {
      gchar *temp_string = NULL;

      GST_OBJECT_LOCK(object);

      temp_string = g_value_dup_string(value);
      if (temp_string) {
        if (self->socket_path) {
          g_free(self->socket_path);
          self->socket_path = NULL;
        }
        self->socket_path = temp_string;
      } else {
        GST_ERROR_OBJECT(object, "failed to copy string [%s]", g_value_get_string(value));
      }

      GST_OBJECT_UNLOCK(object);

      break;
    }
    case PROP_IS_LIVE:
    {
      gboolean is_live = g_value_get_boolean(value);
      GST_INFO_OBJECT(object, "set is-live %d", is_live);
      gst_base_src_set_live(GST_BASE_SRC(object), is_live);
      break;
    }
    default:
      GST_WARNING_OBJECT(object, "unknown property id [%d]", prop_id);;
      break;
  }

  return;
}


static void gst_tizenipc_src_get_property(GObject *object, guint prop_id,
                                          GValue *value, GParamSpec *pspec)
{
  GstTizenipcSrc *self = NULL;

  if (object == NULL) {
    GST_ERROR("NULL object");
    return;
  }

  self = GST_TIZENIPC_SRC(object);
  if (self == NULL) {
    GST_ERROR_OBJECT(object, "failed to cast to GST_TIZENIPC_SRC with %p", object);
    return;
  }

  switch (prop_id) {
    case PROP_SOCKET_PATH:
      GST_OBJECT_LOCK(object);
      g_value_set_string(value, self->socket_path);
      GST_OBJECT_UNLOCK(object);
      break;
    case PROP_IS_LIVE:
      g_value_set_boolean(value, gst_base_src_is_live(GST_BASE_SRC(object)));
      break;
    default:
      GST_WARNING_OBJECT(object, "unknown property id [%d]", prop_id);;
      break;
  }

  return;
}


static gboolean gst_tizenipc_src_start(GstBaseSrc *bsrc)
{
  GstTizenipcSrc *self = NULL;
  gboolean is_live = FALSE;

  if (bsrc == NULL) {
    GST_ERROR("NULL bsrc");
    return FALSE;
  }

  self = GST_TIZENIPC_SRC(bsrc);
  if (self == NULL) {
    GST_ERROR_OBJECT(bsrc, "failed to cast to GST_TIZENIPC_SRC with %p", bsrc);
    return FALSE;
  }

  is_live = gst_base_src_is_live(bsrc);

  GST_INFO_OBJECT(bsrc, "is_live : %d", is_live);

  if (is_live)
    return TRUE;
  else
    return _tizenipc_src_prepare_to_read(self);
}


static gboolean gst_tizenipc_src_stop(GstBaseSrc *bsrc)
{
  GstTizenipcSrc *self = NULL;
  gboolean is_live = FALSE;

  if (bsrc == NULL) {
    GST_ERROR("NULL bsrc");
    return FALSE;
  }

  self = GST_TIZENIPC_SRC(bsrc);
  if (self == NULL) {
    GST_ERROR_OBJECT(bsrc, "failed to cast to GST_TIZENIPC_SRC with %p", bsrc);
    return FALSE;
  }

  is_live = gst_base_src_is_live(bsrc);

  GST_INFO_OBJECT(bsrc, "is_live : %d", is_live);

  if (is_live)
    return TRUE;
  else
    return _tizenipc_src_stop_to_read(self);
}


static void gst_tizenipc_src_buffer_finalize(GstTizenipcSrcBuffer *ipc_buf)
{
  GstTizenipcSrc *self = NULL;
  GstTizenipcMessage send_msg = {0,};
  MMVideoBuffer *mm_buf = NULL;
  int i = 0;
  int send_len = 0;

  if (ipc_buf == NULL) {
    GST_ERROR("NULL ipc_buf");
    return;
  }

  self = ipc_buf->self;
  mm_buf = ipc_buf->mm_buf;

  /* send message to sink for current tbm key */
  if (self->socket_fd > -1) {
    send_msg.type = TIZEN_IPC_BUFFER_RELEASE;
    memcpy(send_msg.tbm_key, ipc_buf->tbm_key, sizeof(int) * MM_VIDEO_BUFFER_PLANE_MAX);
    send_len = send(self->socket_fd, &send_msg, sizeof(GstTizenipcMessage), MSG_NOSIGNAL);
    if (send_len != sizeof(GstTizenipcMessage)) {
      GST_ERROR_OBJECT(self, "send failed : BUFFER_RELEASE key[0] %d", send_msg.tbm_key[0]);
    }
  } else {
    GST_ERROR_OBJECT(self, "invalid socket fd %d", self->socket_fd);
  }

  if (mm_buf) {
    for (i = 0 ; i < mm_buf->handle_num ; i++) {
      if (mm_buf->handle.bo[i]) {
        tbm_bo_unref(mm_buf->handle.bo[i]);
        mm_buf->handle.bo[i] = NULL;
      } else {
        break;
      }
    }

    free(mm_buf);
    mm_buf = NULL;
  }

  /* send buffer signal */
  g_mutex_lock(&self->buffer_lock);

  GST_DEBUG_OBJECT(self, "live buffer(tbm key[0] %d) count %d -> %d",
                         ipc_buf->tbm_key[0], self->live_buffer_count, self->live_buffer_count-1);

  self->live_buffer_count--;
  g_cond_signal(&self->buffer_cond);

  g_mutex_unlock(&self->buffer_lock);

  if (self) {
    gst_object_unref(self);
    self = NULL;
  }

  free(ipc_buf);
  ipc_buf = NULL;

  return;
}


static GstFlowReturn gst_tizenipc_src_create(GstPushSrc *psrc, GstBuffer **outbuf)
{
  GstTizenipcSrc *self = NULL;
  GstTizenipcSrcBuffer *ipc_buf = NULL;
  GstTizenipcMessage recv_msg = {0,};
  GstTizenipcMessage send_msg = {0,};
  MMVideoBuffer *mm_buf = NULL;
  GstBuffer *gst_buf = NULL;
  GstMemory *gst_memory = NULL;
  int i = 0;
  int recv_len = 0;
  int send_len = 0;

  if (psrc == NULL) {
    GST_ERROR("NULL psrc");
    return GST_FLOW_ERROR;
  }

  self = GST_TIZENIPC_SRC(psrc);
  if (self == NULL) {
    GST_ERROR_OBJECT(psrc, "failed to cast to GST_TIZENIPC_SRC with %p", psrc);
    return GST_FLOW_ERROR;
  }

  if (outbuf == NULL) {
    GST_ERROR_OBJECT(self, "NULL buffer pointer");
    return GST_FLOW_ERROR;
  }

again:
  if (gst_poll_wait (self->poll, GST_CLOCK_TIME_NONE) < 0) {
    if (errno == EBUSY)
      return GST_FLOW_FLUSHING;
    GST_ELEMENT_ERROR(self, RESOURCE, READ, ("Failed to read from sink"),
        ("Poll failed on fd: %s", strerror (errno)));
    return GST_FLOW_ERROR;
  }

  if (gst_poll_fd_has_closed (self->poll, &self->pollfd)) {
    GST_ELEMENT_ERROR(self, RESOURCE, READ, ("Failed to read from sink"),
        ("Control socket has closed"));
    return GST_FLOW_ERROR;
  }

  if (gst_poll_fd_has_error (self->poll, &self->pollfd)) {
    GST_ELEMENT_ERROR(self, RESOURCE, READ, ("Failed to read from sink"),
        ("Control socket has error"));
    return GST_FLOW_ERROR;
  }

  if (gst_poll_fd_can_read (self->poll, &self->pollfd)) {
    GST_LOG_OBJECT(self, "Reading from sink");

    GST_OBJECT_LOCK(self);

    /* receive message from sink */
    recv_len = recv(self->socket_fd, &recv_msg, sizeof(GstTizenipcMessage), MSG_DONTWAIT);

    GST_OBJECT_UNLOCK(self);

    if (recv_len != sizeof(GstTizenipcMessage)) {
      GST_ERROR_OBJECT(self, "failed to receive message from sink %d : %d",
                             recv_len, sizeof(GstTizenipcMessage));
      return GST_FLOW_ERROR;
    }

    /* handle message */
    if (recv_msg.type == TIZEN_IPC_BUFFER_NEW) {
      /* get new buffer from sink */
      if (self->shm_mapped_area == MAP_FAILED) {
        GST_ERROR_OBJECT(self, "shared memory is not mapped");
        return GST_FLOW_ERROR;
      }

      mm_buf = (MMVideoBuffer *)malloc(sizeof(MMVideoBuffer));
      if (mm_buf) {
        memcpy(mm_buf, self->shm_mapped_area, sizeof(MMVideoBuffer));
        memcpy(send_msg.tbm_key, self->shm_mapped_area + sizeof(MMVideoBuffer), sizeof(int) * MM_VIDEO_BUFFER_PLANE_MAX);

        for (i = 0 ; i < MM_VIDEO_BUFFER_PLANE_MAX ; i++) {
          if (send_msg.tbm_key[i] > 0) {
            tbm_bo_handle bo_handle = {0, };

            GST_LOG_OBJECT(self, "received tbm key[%d] %d", i, send_msg.tbm_key[i]);

            /* import bo from tbm key */
            mm_buf->handle.bo[i] = tbm_bo_import(self->bufmgr, send_msg.tbm_key[i]);
            if (mm_buf->handle.bo[i] == NULL) {
              GST_ERROR_OBJECT(self, "failed to import bo for tbm key %d", send_msg.tbm_key[i]);
              break;
            }

            /* get user address */
            bo_handle = tbm_bo_get_handle(mm_buf->handle.bo[i], TBM_DEVICE_CPU);
            if (bo_handle.ptr == NULL) {
              GST_ERROR_OBJECT(self, "failed to get user address for bo %p, key %d",
                                     mm_buf->handle.bo[i], send_msg.tbm_key[i]);
              break;
            }
            mm_buf->data[i] = bo_handle.ptr;
          } else {
            break;
          }
        }
      } else {
        GST_ERROR_OBJECT(self, "failed to alloc MMVideoBuffer");
      }

      /* send received message */
      send_msg.type = TIZEN_IPC_BUFFER_RECEIVED;

      GST_OBJECT_LOCK(self);

      send_len = send(self->socket_fd, &send_msg, sizeof(GstTizenipcMessage), MSG_NOSIGNAL);

      GST_OBJECT_UNLOCK(self);

      if (send_len != sizeof(GstTizenipcMessage)) {
        GST_ERROR_OBJECT(self, "failed to send RECEIVED message");
      }
    } else if (recv_msg.type == TIZEN_IPC_SHM_PATH) {
      gchar shm_path[32] = {'\0',};
      int shm_size = sizeof(MMVideoBuffer) + (sizeof(int) * MM_VIDEO_BUFFER_PLANE_MAX);

      /* get shm path */
      recv_len = recv(self->socket_fd, shm_path, recv_msg.size, 0);
      if (recv_len != recv_msg.size) {
        GST_ERROR_OBJECT(self, "failed to receive message from sink %d : %d",
                               recv_len, recv_msg.size);
        return GST_FLOW_ERROR;
      }

      GST_INFO_OBJECT(self, "shm path from sink [%s]", shm_path);

      if (self->shm_path) {
        g_free(self->shm_path);
        self->shm_path = NULL;
      }

      self->shm_path = g_strdup(shm_path);
      if (self->shm_path == NULL) {
        GST_ERROR_OBJECT(self, "failed to copy shm path string [%s]", shm_path);
        return GST_FLOW_ERROR;
      }

      /* open shared memory */
      self->shm_fd = shm_open(self->shm_path, O_RDONLY, shm_size);
      if (self->shm_fd < 0) {
        GST_ERROR_OBJECT(self, "failed to open shared memory for shm path [%s], size %d",
                               self->shm_path, shm_size);
        return GST_FLOW_ERROR;
      }

      GST_INFO_OBJECT(self, "opened shm fd %d", self->shm_fd);

      self->shm_mapped_area = mmap(NULL,
                                   shm_size,
                                   PROT_READ,
                                   MAP_SHARED,
                                   self->shm_fd,
                                   0);
      if (self->shm_mapped_area == MAP_FAILED) {
        GST_ERROR_OBJECT(self, "failed to mmap shared memory for fd %d", self->shm_fd);
        close(self->shm_fd);
        self->shm_fd = -1;
        return GST_FLOW_ERROR;
      }

      self->shm_mapped_size = shm_size;

      GST_INFO_OBJECT(self, "mapped shared memory address %p, size %d",
                            self->shm_mapped_area, shm_size);
      goto again;
    } else {
      GST_WARNING_OBJECT(self, "unknown message type %d", recv_msg.type);
      goto again;
    }
  }

  if (mm_buf == NULL) {
    GST_ERROR_OBJECT(self, "NULL mm_buf");
    return GST_FLOW_ERROR;
  }

  /* make gst buffer with mm_buf */
  gst_buf = gst_buffer_new();
  if (gst_buf == NULL) {
    GST_ERROR_OBJECT(self, "failed to create gst buffer");
    goto _CREATE_FAILED;
  }

  /* default memory */
  gst_memory = gst_memory_new_wrapped(0,
                                      mm_buf->data[0],
                                      mm_buf->size[0],
                                      0,
                                      mm_buf->size[0],
                                      NULL,
                                      NULL);
  if (gst_memory == NULL) {
    GST_ERROR_OBJECT(self, "failed to create default gst memory");
    goto _CREATE_FAILED;
  }

  gst_buffer_append_memory(gst_buf, gst_memory);
  gst_memory = NULL;

  /* mm_buf memory */
  gst_memory = gst_memory_new_wrapped(0,
                                      mm_buf,
                                      sizeof(MMVideoBuffer),
                                      0,
                                      sizeof(MMVideoBuffer),
                                      mm_buf,
                                      NULL);
  if (gst_memory == NULL) {
    GST_ERROR_OBJECT(self, "failed to create gst memory for mm_buf");
    goto _CREATE_FAILED;
  }

  gst_buffer_append_memory(gst_buf, gst_memory);
  gst_memory = NULL;

  /* ipc_buf memory */
  ipc_buf = (GstTizenipcSrcBuffer *)malloc(sizeof(GstTizenipcSrcBuffer));
  if (ipc_buf == NULL) {
    GST_ERROR_OBJECT(self, "failed to create GstTizenipcsrcBuffer");
    goto _CREATE_FAILED;
  }

  ipc_buf->self = gst_object_ref(self);
  ipc_buf->gst_buf = gst_buf;
  ipc_buf->mm_buf = mm_buf;
  memcpy(ipc_buf->tbm_key, send_msg.tbm_key, sizeof(int) * MM_VIDEO_BUFFER_PLANE_MAX);

  gst_memory = gst_memory_new_wrapped(0,
                                      ipc_buf,
                                      sizeof(GstTizenipcSrcBuffer),
                                      0,
                                      sizeof(GstTizenipcSrcBuffer),
                                      ipc_buf,
                                      (GDestroyNotify)gst_tizenipc_src_buffer_finalize);
  if (gst_memory == NULL) {
    GST_ERROR_OBJECT(self, "failed to create gst memory for ipc_buf");
    goto _CREATE_FAILED;
  }

  gst_buffer_append_memory(gst_buf, gst_memory);
  gst_memory = NULL;

  g_mutex_lock(&self->buffer_lock);
  self->live_buffer_count++;
  GST_DEBUG_OBJECT(self, "gst buffer %p, live count %d", gst_buf, self->live_buffer_count);
  g_mutex_unlock(&self->buffer_lock);

  *outbuf = gst_buf;

  return GST_FLOW_OK;

_CREATE_FAILED:
  if (ipc_buf) {
    free(ipc_buf);
    ipc_buf = NULL;
  }

  if (mm_buf) {
    free(mm_buf);
    mm_buf = NULL;
  }

  if (gst_memory) {
    gst_memory_unref(gst_memory);
    gst_memory = NULL;
  }

  if (gst_buf) {
    gst_buffer_unref(gst_buf);
    gst_buf = NULL;
  }

  return GST_FLOW_ERROR;
}


static gboolean gst_tizenipc_src_unlock(GstBaseSrc *bsrc)
{
  return TRUE;
}


static gboolean gst_tizenipc_src_unlock_stop(GstBaseSrc *bsrc)
{
  return TRUE;
}


static GstStateChangeReturn gst_tizenipc_src_change_state(GstElement *element, GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstTizenipcSrc *self = NULL;
  gboolean is_live = FALSE;

  if (element == NULL) {
    GST_ERROR("NULL element");
    return FALSE;
  }

  self = GST_TIZENIPC_SRC(element);
  if (self == NULL) {
    GST_ERROR_OBJECT(element, "failed to cast to GST_TIZENIPC_SRC with %p", element);
    return FALSE;
  }

  is_live = gst_base_src_is_live(GST_BASE_SRC(element));

  GST_INFO_OBJECT(self, "transition %d - is_live %d", transition, is_live);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      if (is_live)
        if (!_tizenipc_src_prepare_to_read(self))
          return GST_STATE_CHANGE_FAILURE;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS(parent_class)->change_state(element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      if (is_live)
        if (!_tizenipc_src_stop_to_read(self))
          return GST_STATE_CHANGE_FAILURE;
    default:
      break;
  }

  return ret;
}


static gboolean plugin_init(GstPlugin *plugin)
{
  if (!gst_element_register(plugin,
                            "tizenipcsrc",
                            GST_RANK_PRIMARY,
                            GST_TYPE_TIZENIPC_SRC)) {
    return FALSE;
  }

  GST_DEBUG_CATEGORY_INIT(gst_debug_tizenipc_src,
                          "tizenipcsrc",
                          0,
                          "Tizen IPC source element");

  return TRUE;
}


GST_PLUGIN_DEFINE(GST_VERSION_MAJOR,
                  GST_VERSION_MINOR,
                  tizenipcsrc,
                  "Tizen IPC source to receive multimedia video buffer",
                  plugin_init, VERSION, GST_LICENSE,
                  "Samsung Electronics Co", "http://www.samsung.com")
