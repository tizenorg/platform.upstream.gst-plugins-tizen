/*
 * GStreamer Tizen IPC sink
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
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include "gsttizenipcsink.h"

#define DEFAULT_SOCKET_PATH "/tmp/tizenipc.0"
#define DEFAULT_SHM_PATH    "/tizenipcshm"
#define DEFAULT_PERMISSIONS (S_IRUSR|S_IWUSR|S_IRGRP)
#define DEFAULT_BACKLOG     5
#define CLIENT_RESPONSE_TIMEOUT (G_TIME_SPAN_MILLISECOND * 200)
#define BUFFER_WAIT_TIMEOUT     (G_TIME_SPAN_MILLISECOND * 3000)

GST_DEBUG_CATEGORY(gst_debug_tizenipc_sink);

#define GST_CAT_DEFAULT gst_debug_tizenipc_sink

static GstStaticPadTemplate sinktemplate = \
    GST_STATIC_PAD_TEMPLATE("sink",
                            GST_PAD_SINK,
                            GST_PAD_ALWAYS,
                            GST_STATIC_CAPS_ANY);

#define gst_tizenipc_sink_parent_class parent_class
G_DEFINE_TYPE(GstTizenipcSink, gst_tizenipc_sink, GST_TYPE_BASE_SINK);

/* signals */
enum {
  SIGNAL_CLIENT_CONNECTED,
  SIGNAL_CLIENT_DISCONNECTED,
  LAST_SIGNAL
};

/* properties */
enum {
  PROP_0,
  PROP_SOCKET_PATH,
  PROP_PERMISSIONS
};



static gboolean _prepare_tizenipc_sink(GstTizenipcSink *self, guint shm_size);
static gboolean _add_buffer_to_list(GstTizenipcSink *self, GstBuffer *buf, int *tbm_key);
static gboolean _remove_buffer_from_list(GstTizenipcSink *self, int *tbm_key);

static void gst_tizenipc_sink_finalize(GObject *object);
static void gst_tizenipc_sink_set_property(GObject *object, guint prop_id,
                                           const GValue *value, GParamSpec *pspec);
static void gst_tizenipc_sink_get_property(GObject *object, guint prop_id,
                                           GValue *value, GParamSpec *pspec);
static gboolean gst_tizenipc_sink_start(GstBaseSink *bsink);
static gboolean gst_tizenipc_sink_stop(GstBaseSink *bsink);
static GstFlowReturn gst_tizenipc_sink_render(GstBaseSink *bsink, GstBuffer *buf);
static gboolean gst_tizenipc_sink_event(GstBaseSink *bsink, GstEvent *event);
static gboolean gst_tizenipc_sink_unlock(GstBaseSink *bsink);
static gboolean gst_tizenipc_sink_unlock_stop(GstBaseSink *bsink);

static gpointer _gst_poll_thread_func(gpointer data);

static guint signals[LAST_SIGNAL] = {0, 0};




static gboolean _prepare_tizenipc_sink(GstTizenipcSink *self, guint shm_size)
{
  int i = 0;
  int flags = 0;
  struct sockaddr_un addr_un;
  struct sockaddr *address = NULL;
  socklen_t address_len = 0;
  gchar shm_path[32] = {'\0',};

  if (self == NULL) {
    GST_ERROR("NULL handle");
    return FALSE;
  }

  GST_INFO_OBJECT(self, "start - shared memory size %u", shm_size);

  /* open socket */
  self->socket_fd = socket(PF_UNIX, SOCK_STREAM, 0);
  if (self->socket_fd < 0) {
    GST_ERROR_OBJECT(self, "failed to create socket");
    return FALSE;
  }

  flags = fcntl(self->socket_fd, F_GETFL, NULL);
  if (flags < 0) {
    GST_ERROR_OBJECT(self, "failed to fcntl F_GETFL");
    goto _FAILED;
  }

  /* set non-block mode */
  if (fcntl (self->socket_fd, F_SETFL, flags|O_NONBLOCK) < 0) {
    GST_ERROR_OBJECT(self, "failed to fcntl F_SETFL");
    goto _FAILED;
  }

  memset(&addr_un, 0x0, sizeof(addr_un));

  addr_un.sun_family = AF_UNIX;
  strncpy(addr_un.sun_path, self->socket_path, sizeof(addr_un.sun_path) - 1);

  address = (struct sockaddr *)(&addr_un);
  address_len = sizeof(addr_un);

  unlink(self->socket_path);

  /* bind socket */
  while (bind(self->socket_fd, address, address_len) < 0) {
    if (errno != EADDRINUSE) {
      GST_ERROR_OBJECT(self, "failed to bind. errno %d", errno);
      goto _FAILED;
    }

    if (i > 256) {
      GST_ERROR_OBJECT(self, "no more free socket name");
      goto _FAILED;
    }

    snprintf(addr_un.sun_path, sizeof(addr_un.sun_path), "%s.%d", self->socket_path, i);
    i++;

    unlink(addr_un.sun_path);
  }

  if (self->socket_path_result) {
    g_free(self->socket_path_result);
    self->socket_path_result = NULL;
  }

  self->socket_path_result = g_strdup(addr_un.sun_path);
  if (self->socket_path_result == NULL) {
    GST_ERROR_OBJECT(self, "failed to copy string %s", addr_un.sun_path);
    goto _FAILED;
  }

  if (chmod(self->socket_path_result, self->permissions) < 0) {
    GST_ERROR_OBJECT(self, "failed to chmod %s - %d", addr_un.sun_path, self->permissions);
    goto _FAILED;
  }

  if (listen(self->socket_fd, DEFAULT_BACKLOG) < 0) {
    GST_ERROR_OBJECT(self, "failed to listen");
    goto _FAILED;
  }

  /* create shared memory */
  i = 0;
  do {
    snprintf(shm_path, 32, "%s.%d", DEFAULT_SHM_PATH, i++);
    self->shm_fd = shm_open(shm_path, O_RDWR|O_CREAT|O_EXCL, self->permissions);
  } while (self->shm_fd < 0 && errno == EEXIST);

  if (self->shm_fd < 0) {
    GST_ERROR_OBJECT(self, "failed to open shared memory [%s], errno [%d]", shm_path, errno);
    goto _FAILED;
  }

  if (self->shm_path) {
    g_free(self->shm_path);
    self->shm_path = NULL;
  }

  self->shm_path = g_strdup(shm_path);
  if (self->shm_path == NULL) {
    GST_ERROR_OBJECT(self, "failed to copy shared memory path");
    goto _FAILED;
  }

  if (ftruncate(self->shm_fd, shm_size) < 0) {
    GST_ERROR_OBJECT(self, "failed to resize shm to %d", shm_size);
    goto _FAILED;
  }

  self->shm_mapped_area = (gchar *)mmap(NULL,
                                        shm_size,
                                        PROT_READ | PROT_WRITE,
                                        MAP_SHARED,
                                        self->shm_fd,
                                        0);
  if (self->shm_mapped_area == MAP_FAILED) {
    GST_ERROR_OBJECT(self, "failed to mmap for shared memory");
    goto _FAILED;
  }

  self->shm_mapped_size = shm_size;

  /* create gst poll and thread for poll */
  self->poll = gst_poll_new(TRUE);
  if (self->poll == NULL) {
    GST_ERROR_OBJECT(self, "failed to create gst poll");
    goto _FAILED;
  }
  gst_poll_fd_init(&self->pollfd);
  self->pollfd.fd = self->socket_fd;
  gst_poll_add_fd(self->poll, &self->pollfd);
  gst_poll_fd_ctl_read(self->poll, &self->pollfd, TRUE);

  self->poll_thread_run = TRUE;
  self->poll_thread = g_thread_try_new("gsttizenipcsink_poll_thread",
                                       _gst_poll_thread_func,
                                       self,
                                       NULL);
  if (self->poll_thread == NULL) {
    GST_ERROR_OBJECT(self, "failed to create thread for gst poll");
    self->poll_thread_run = FALSE;
    goto _FAILED;
  }

  GST_INFO_OBJECT(self, "done - shm %p", self->shm_mapped_area);

  return TRUE;

_FAILED:
  if (self->poll) {
    gst_poll_free(self->poll);
    self->poll = NULL;
  }

  if (self->shm_fd > -1) {
    close(self->shm_fd);
    self->shm_fd = -1;
  }

  if (self->socket_fd > -1) {
    close(self->socket_fd);
    self->socket_fd = -1;
  }

  return FALSE;
}


static gboolean _add_buffer_to_list(GstTizenipcSink *self, GstBuffer *buf, int *tbm_key)
{
  int i = 0;
  int j = 0;
  GstTizenipcBuffer *sended_buffer = NULL;

  if (self == NULL || buf == NULL || tbm_key == NULL) {
    GST_ERROR("NULL parameter %p, %p, %p", self, buf, tbm_key);
    return FALSE;
  }

  g_mutex_lock(&self->buffer_lock);

  sended_buffer = self->sended_buffer;

  for (i = 0 ; i < GST_TIZENIPC_BUFFER_MAX ; i++) {
    /* find empty space */
    if (sended_buffer[i].gst_buf == NULL) {
      self->sended_buffer_count++;

      GST_DEBUG_OBJECT(self, "insert buffer(key[0] %d) to index %d, count %d",
                             tbm_key[0], i, self->sended_buffer_count);

      /* ref gst buffer and set tbm key */
      gst_buffer_ref(buf);
      sended_buffer[i].gst_buf = buf;

      for (j = 0 ; j < MM_VIDEO_BUFFER_PLANE_MAX ; j++) {
        if (tbm_key[j] > 0) {
          sended_buffer[i].tbm_key[j] = tbm_key[j];
        } else {
          break;
        }
      }

      g_mutex_unlock(&self->buffer_lock);

      return TRUE;
    }
  }

  g_mutex_unlock(&self->buffer_lock);

  GST_WARNING_OBJECT(self, "should not be reached here. no space to keep buffer");

  return FALSE;
}


static gboolean _remove_buffer_from_list(GstTizenipcSink *self, int *tbm_key)
{
  int i = 0;
  GstTizenipcBuffer *sended_buffer = NULL;

  if (self == NULL || tbm_key == NULL) {
    GST_ERROR("NULL parameter %p, %p", self, tbm_key);
    return FALSE;
  }

  g_mutex_lock(&self->buffer_lock);

  sended_buffer = self->sended_buffer;

  for (i = 0 ; i < GST_TIZENIPC_BUFFER_MAX ; i++) {
    /* find matched buffer info */
    if (sended_buffer[i].tbm_key[0] == tbm_key[0] &&
        sended_buffer[i].tbm_key[1] == tbm_key[1] &&
        sended_buffer[i].tbm_key[2] == tbm_key[2] &&
        sended_buffer[i].tbm_key[3] == tbm_key[3]) {
      /* remove buffer info and unref gst buffer */
      self->sended_buffer_count--;

      GST_DEBUG_OBJECT(self, "gst buffer %p for key[0] %d, count %d",
                             sended_buffer[i].gst_buf, tbm_key[0], self->sended_buffer_count);

      if (sended_buffer[i].gst_buf) {
        gst_buffer_unref(sended_buffer[i].gst_buf);
        sended_buffer[i].gst_buf = NULL;
      } else {
        GST_WARNING_OBJECT(self, "no gst buffer for key[0] %d", tbm_key[0]);
      }

      sended_buffer[i].tbm_key[0] = 0;
      sended_buffer[i].tbm_key[1] = 0;
      sended_buffer[i].tbm_key[2] = 0;
      sended_buffer[i].tbm_key[3] = 0;

      g_cond_signal(&self->buffer_cond);
      g_mutex_unlock(&self->buffer_lock);

      return TRUE;
    }
  }

  g_cond_signal(&self->buffer_cond);
  g_mutex_unlock(&self->buffer_lock);

  GST_WARNING_OBJECT(self, "could not find matched buffer for tbm_key[0] %d", tbm_key[0]);

  return FALSE;
}



/* ---------------------- */
/*      MAIN METHODS      */
/* ---------------------- */

static void gst_tizenipc_sink_init(GstTizenipcSink *self)
{
  g_mutex_init(&self->buffer_lock);
  g_cond_init(&self->buffer_cond);
  g_mutex_init(&self->ipc_lock);
  g_cond_init(&self->ipc_cond);

  self->socket_fd = -1;
  self->client_fd = -1;
  self->shm_fd = -1;
  self->shm_mapped_area = MAP_FAILED;
  self->is_connected = FALSE;
  self->permissions = DEFAULT_PERMISSIONS;
  self->socket_path = g_strdup(DEFAULT_SOCKET_PATH);
  if (self->socket_path == NULL) {
    GST_ERROR_OBJECT(self, "failed to dup socket path [%s]", DEFAULT_SOCKET_PATH);
  }
  self->sended_buffer = g_new0(GstTizenipcBuffer, GST_TIZENIPC_BUFFER_MAX);
  if (self->sended_buffer == NULL) {
    GST_ERROR_OBJECT(self, "failed to alloc sended_buffer");
  }

  return;
}

static void gst_tizenipc_sink_class_init(GstTizenipcSinkClass *klass)
{
  GObjectClass *gobject_class = NULL;
  GstElementClass *gstelement_class = NULL;
  GstBaseSinkClass *gstbasesink_class = NULL;
  GParamSpec *pspec = NULL;

  if (klass == NULL) {
    GST_ERROR("NULL klass");
    return;
  }

  gobject_class = (GObjectClass *)klass;
  gstelement_class = (GstElementClass *)klass;
  gstbasesink_class = (GstBaseSinkClass *)klass;

  parent_class = g_type_class_peek_parent(klass);

  gobject_class->finalize = gst_tizenipc_sink_finalize;
  gobject_class->set_property = gst_tizenipc_sink_set_property;
  gobject_class->get_property = gst_tizenipc_sink_get_property;

  gstbasesink_class->start = GST_DEBUG_FUNCPTR(gst_tizenipc_sink_start);
  gstbasesink_class->stop = GST_DEBUG_FUNCPTR(gst_tizenipc_sink_stop);
  gstbasesink_class->render = GST_DEBUG_FUNCPTR(gst_tizenipc_sink_render);
  gstbasesink_class->event = GST_DEBUG_FUNCPTR(gst_tizenipc_sink_event);
  gstbasesink_class->unlock = GST_DEBUG_FUNCPTR(gst_tizenipc_sink_unlock);
  gstbasesink_class->unlock_stop = GST_DEBUG_FUNCPTR(gst_tizenipc_sink_unlock_stop);

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

  pspec = g_param_spec_uint("permissions",
                            "Permissions for the IPC",
                            "Permissions for the IPC",
                            0, 07777, DEFAULT_PERMISSIONS,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  if (pspec) {
    g_object_class_install_property (gobject_class, PROP_PERMISSIONS, pspec);
  } else {
    GST_ERROR("failed to get pspec for \"permissions\"");
  }

  /* signal */
  signals[SIGNAL_CLIENT_CONNECTED] = \
    g_signal_new("client-connected",
                 GST_TYPE_TIZENIPC_SINK, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                 g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT);

  signals[SIGNAL_CLIENT_DISCONNECTED] = \
    g_signal_new("client-disconnected",
                 GST_TYPE_TIZENIPC_SINK, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                 g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT);

  gst_element_class_add_pad_template(gstelement_class,
                                     gst_static_pad_template_get(&sinktemplate));

  gst_element_class_set_static_metadata(gstelement_class,
                                        "Tizen IPC Sink",
                                        "Sink",
                                        "Send data via IPC to the tizenipcsrc",
                                        "Jeongmo Yang <jm80.yang@samsung.com>");

  return;
}


static void gst_tizenipc_sink_finalize(GObject *object)
{
  GstTizenipcSink *self = NULL;

  if (object == NULL) {
    GST_ERROR("NULL object");
    return;
  }

  self = GST_TIZENIPC_SINK(object);
  if (self == NULL) {
    GST_ERROR_OBJECT(object, "failed to cast to GST_TIZENIPC_SINK with %p", object);
    return;
  }

  g_mutex_clear(&self->ipc_lock);
  g_cond_clear(&self->ipc_cond);
  g_mutex_clear(&self->buffer_lock);
  g_cond_clear(&self->buffer_cond);

  if (self->socket_path) {
    g_free(self->socket_path);
    self->socket_path = NULL;
  }

  if (self->socket_path_result) {
    g_free(self->socket_path_result);
    self->socket_path_result = NULL;
  }

  if (self->shm_path) {
    g_free(self->shm_path);
    self->shm_path = NULL;
  }

  G_OBJECT_CLASS(parent_class)->finalize(object);

  return;
}


static void gst_tizenipc_sink_set_property(GObject *object, guint prop_id,
                                           const GValue *value, GParamSpec *pspec)
{
  GstTizenipcSink *self = NULL;

  if (object == NULL) {
    GST_ERROR("NULL object");
    return;
  }

  self = GST_TIZENIPC_SINK(object);
  if (self == NULL) {
    GST_ERROR_OBJECT(object, "failed to cast to GST_TIZENIPC_SINK with %p", object);
    return;
  }

  GST_OBJECT_LOCK(object);

  switch (prop_id) {
    case PROP_SOCKET_PATH:
    {
      gchar *temp_string = g_value_dup_string(value);
      if (temp_string) {
        if (self->socket_path) {
          g_free(self->socket_path);
          self->socket_path = NULL;
        }
        self->socket_path = temp_string;
      } else {
        GST_ERROR_OBJECT(object, "failed to copy string [%s]", g_value_get_string(value));
      }
      break;
    }
    case PROP_PERMISSIONS:
      self->permissions = g_value_get_uint(value);
      break;
    default:
      GST_WARNING_OBJECT(object, "unknown property id [%d]", prop_id);;
      break;
  }

  GST_OBJECT_UNLOCK(object);

  return;
}


static void gst_tizenipc_sink_get_property(GObject *object, guint prop_id,
                                           GValue *value, GParamSpec *pspec)
{
  GstTizenipcSink *self = NULL;

  if (object == NULL) {
    GST_ERROR("NULL object");
    return;
  }

  self = GST_TIZENIPC_SINK(object);
  if (self == NULL) {
    GST_ERROR_OBJECT(object, "failed to cast to GST_TIZENIPC_SINK with %p", object);
    return;
  }

  GST_OBJECT_LOCK(object);

  switch (prop_id) {
    case PROP_SOCKET_PATH:
      g_value_set_string(value, self->socket_path);
      break;
    case PROP_PERMISSIONS:
      g_value_set_uint(value, self->permissions);
      break;
    default:
      GST_WARNING_OBJECT(object, "unknown property id [%d]", prop_id);;
      break;
  }

  GST_OBJECT_UNLOCK(object);

  return;
}


static gboolean gst_tizenipc_sink_start(GstBaseSink *bsink)
{
  GstTizenipcSink *self = NULL;

  if (bsink == NULL) {
    GST_ERROR("NULL bsink");
    return FALSE;
  }

  self = GST_TIZENIPC_SINK(bsink);
  if (self == NULL) {
    GST_ERROR_OBJECT(bsink, "failed to cast to GST_TIZENIPC_SINK with %p", bsink);
    return FALSE;
  }

  /* check socket path and buffer list */
  if (self->socket_path == NULL ||
      self->sended_buffer == NULL) {
    GST_ERROR_OBJECT(self, "socket path[%p] or sended buffer [%p] is NULL",
                           self->socket_path, self->sended_buffer);
    return FALSE;
  }

  /* create socket and shared memory for sending buffer */
  if (!_prepare_tizenipc_sink(self, sizeof(MMVideoBuffer) + sizeof(int)*MM_VIDEO_BUFFER_PLANE_MAX)) {
    GST_ERROR_OBJECT(self, "prepare failed");
    return FALSE;
  }

  return TRUE;
}


static gboolean gst_tizenipc_sink_stop(GstBaseSink *bsink)
{
  GstTizenipcSink *self = NULL;
  gint64 wait_end_time = 0;

  if (bsink == NULL) {
    GST_ERROR("NULL bsink");
    return FALSE;
  }

  self = GST_TIZENIPC_SINK(bsink);
  if (self == NULL) {
    GST_ERROR_OBJECT(bsink, "failed to cast to GST_TIZENIPC_SINK with %p", bsink);
    return FALSE;
  }

  GST_INFO_OBJECT(self, "start");

  /* stop poll thread */
  self->poll_thread_run = FALSE;
  if (self->poll) {
    gst_poll_set_flushing(self->poll, TRUE);
  }

  if (self->poll_thread) {
    GST_INFO_OBJECT(self, "join poll thread %p", self->poll_thread);

    g_thread_join(self->poll_thread);
    self->poll_thread = NULL;
  } else {
    GST_WARNING_OBJECT(self, "no poll thread");
  }

  /* wait for sended buffer */
  g_mutex_lock(&self->buffer_lock);

  while (self->sended_buffer_count > 0) {
    wait_end_time = g_get_monotonic_time () + BUFFER_WAIT_TIMEOUT;
    if (!g_cond_wait_until(&self->ipc_cond, &self->ipc_lock, wait_end_time)) {
      GST_WARNING_OBJECT(self, "wait timeout - current count %d",
                               self->sended_buffer_count);
      break;
    } else {
      GST_WARNING_OBJECT(self, "signal received - current count %d",
                               self->sended_buffer_count);
    }
  }

  g_mutex_unlock(&self->buffer_lock);

  /* close client */
  if (self->client_fd >= 0) {
    GST_INFO_OBJECT(self, "close client fd %d", self->client_fd);

    shutdown(self->client_fd, SHUT_RDWR);
    close(self->client_fd);
    g_signal_emit(self, signals[SIGNAL_CLIENT_DISCONNECTED], 0, self->client_fd);
    self->client_fd = -1;
  } else {
    GST_WARNING_OBJECT(self, "no client");
  }

  /* release shared memory */
  if (self->shm_fd >= 0) {
    if (self->shm_mapped_area != MAP_FAILED) {
      munmap(self->shm_mapped_area, self->shm_mapped_size);
      self->shm_mapped_area = MAP_FAILED;
      self->shm_mapped_size = 0;
    }

    close(self->shm_fd);
    self->shm_fd = -1;

    if (self->shm_path) {
      shm_unlink(self->shm_path);
      g_free(self->shm_path);
      self->shm_path = NULL;
    }
  }

  /* release gst poll */
  if (self->poll) {
    GST_INFO_OBJECT(self, "close gst poll %p", self->poll);

    gst_poll_free(self->poll);
    self->poll = NULL;
  } else {
    GST_WARNING_OBJECT(self, "no gst poll");
  }

  /* close socket */
  if (self->socket_fd >= 0) {
    GST_INFO_OBJECT(self, "close main socket %d", self->socket_fd);

    shutdown(self->socket_fd, SHUT_RDWR);
    close(self->socket_fd);
    self->socket_fd = -1;
  } else {
    GST_WARNING_OBJECT(self, "socket is not opened");
  }

  if (self->socket_path_result) {
    unlink(self->socket_path_result);
  }

  GST_INFO_OBJECT(self, "done");

  return TRUE;
}


static GstFlowReturn gst_tizenipc_sink_render(GstBaseSink *bsink, GstBuffer *buf)
{
  GstTizenipcSink *self = NULL;
  GstTizenipcMessage msg = {0, };
  MMVideoBuffer *mm_buf = NULL;
  GstMemory *mm_buf_memory = NULL;
  GstMapInfo map_info = GST_MAP_INFO_INIT;
  gint64 wait_end_time = 0;
  int tbm_key[MM_VIDEO_BUFFER_PLANE_MAX] = {0, };
  int i = 0;

  if (bsink == NULL) {
    GST_ERROR("NULL bsink");
    return GST_FLOW_ERROR;
  }

  self = GST_TIZENIPC_SINK(bsink);
  if (self == NULL) {
    GST_ERROR_OBJECT(bsink, "failed to cast to GST_TIZENIPC_SINK with %p", bsink);
    return GST_FLOW_ERROR;
  }

  if (buf == NULL) {
    GST_ERROR_OBJECT(self, "NULL buffer");
    return GST_FLOW_ERROR;
  }

  g_mutex_lock(&self->ipc_lock);

  if (self->client_fd < 0) {
    GST_WARNING_OBJECT(self, "no client is connected");
    goto _SKIP_BUFFER;
  }

  /* get mm_buf from gst buffer */
  if (gst_buffer_n_memory(buf) <= 1) {
    GST_WARNING_OBJECT(self, "invalid memory number %d", gst_buffer_n_memory(buf));
    goto _SKIP_BUFFER;
  }

  mm_buf_memory = gst_buffer_peek_memory(buf, 1);
  if (mm_buf_memory == NULL) {
    GST_WARNING_OBJECT(self, "failed to peek memory 1 for %p", buf);
    goto _SKIP_BUFFER;
  }

  if (gst_memory_map(mm_buf_memory, &map_info, GST_MAP_READ) == FALSE) {
    GST_WARNING_OBJECT(self, "failed to map memory %p", mm_buf_memory);
    goto _SKIP_BUFFER;
  }

  mm_buf = (MMVideoBuffer *)map_info.data;

  gst_memory_unmap(mm_buf_memory, &map_info);

  if (mm_buf == NULL) {
    GST_WARNING_OBJECT(self, "NULL mm_buf");
    goto _SKIP_BUFFER;
  }

  GST_LOG_OBJECT(self, "MMVideoBuffer info - %p, num handle %d",
                       mm_buf, mm_buf->handle_num);

  /* export bo to pass buffer to client process */
  for (i = 0 ; i < mm_buf->handle_num ; i++) {
    if (mm_buf->handle.bo[i]) {
      tbm_key[i] = tbm_bo_export(mm_buf->handle.bo[i]);
      GST_LOG_OBJECT(self, "export tbm key[index:%d] %d", i, tbm_key[i]);
      if (tbm_key[i] <= 0) {
        GST_ERROR_OBJECT(self, "failed to export bo[%d] %p", i, mm_buf->handle.bo[i]);
        goto _SKIP_BUFFER;
      }
    } else {
      break;
    }
  }

  /* keep and send buffer */
  if (_add_buffer_to_list(self, buf, tbm_key) == FALSE) {
    GST_ERROR_OBJECT(self, "failed to add to list for buffer %p and key[0] %d", buf, tbm_key[0]);
    goto _SKIP_BUFFER;
  }

  /* set command type and size */
  msg.type = TIZEN_IPC_BUFFER_NEW;
  msg.size = sizeof(MMVideoBuffer) + sizeof(tbm_key);

  /* copy zero copy info to shared memory */
  memcpy(self->shm_mapped_area, mm_buf, sizeof(MMVideoBuffer));
  memcpy(self->shm_mapped_area + sizeof(MMVideoBuffer), tbm_key, sizeof(tbm_key));

  /* send data */
  if (send(self->client_fd, &msg, sizeof(GstTizenipcMessage), MSG_NOSIGNAL) != sizeof(GstTizenipcMessage)) {
    GST_ERROR_OBJECT(self, "failed to send buffer to src");
    goto _SKIP_BUFFER_AFTER_ADD_TO_LIST;
  }

  /* wait for client's response */
  GST_LOG_OBJECT(self, "Wait for client's response");

  wait_end_time = g_get_monotonic_time () + CLIENT_RESPONSE_TIMEOUT;

  if (!g_cond_wait_until(&self->ipc_cond, &self->ipc_lock, wait_end_time)) {
    GST_ERROR_OBJECT(self, "response wait timeout[%lld usec]", CLIENT_RESPONSE_TIMEOUT);
    goto _SKIP_BUFFER_AFTER_ADD_TO_LIST;
  } else {
    GST_LOG_OBJECT(self, "response received.");
  }

  g_mutex_unlock(&self->ipc_lock);

  return GST_FLOW_OK;

_SKIP_BUFFER_AFTER_ADD_TO_LIST:
  _remove_buffer_from_list(self, tbm_key);

_SKIP_BUFFER:
  g_mutex_unlock(&self->ipc_lock);

  return GST_FLOW_OK;
}


static gboolean gst_tizenipc_sink_event(GstBaseSink *bsink, GstEvent *event)
{
  GstTizenipcSink *self = NULL;

  if (bsink == NULL) {
    GST_ERROR("NULL object");
    return FALSE;
  }

  self = GST_TIZENIPC_SINK(bsink);
  if (self == NULL) {
    GST_ERROR_OBJECT(bsink, "failed to cast to GST_TIZENIPC_SINK with %p", bsink);
    return FALSE;
  }

  switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_EOS:
      /* wait for sended buffer */
      break;
    default:
      break;
  }

  return GST_BASE_SINK_CLASS(parent_class)->event(bsink, event);
}


static gboolean gst_tizenipc_sink_unlock(GstBaseSink *bsink)
{
  return TRUE;
}


static gboolean gst_tizenipc_sink_unlock_stop(GstBaseSink *bsink)
{
  return TRUE;
}


static gpointer _gst_poll_thread_func(gpointer data)
{
  GstTizenipcSink *self = NULL;
  GstClockTime timeout = GST_CLOCK_TIME_NONE;

  if (data == NULL) {
    GST_ERROR("NULL data");
    return NULL;
  }

  self = GST_TIZENIPC_SINK(data);
  if (self == NULL) {
    GST_ERROR("failed to cast GST_TIZENIPC_SINK");
    return NULL;
  }

  GST_INFO_OBJECT(self, "start");

  while (self->poll_thread_run) {
    if (gst_poll_wait(self->poll, timeout) < 0) {
      GST_ERROR_OBJECT(self, "failed to wait gst poll. errno %d", errno);
      return NULL;
    }

    timeout = GST_CLOCK_TIME_NONE;

    if (self->poll_thread_run == FALSE) {
      GST_INFO_OBJECT(self, "stop poll thread");
      return NULL;
    }

    if (gst_poll_fd_has_closed(self->poll, &self->pollfd)) {
      GST_ERROR_OBJECT(self, "failed to read from socket fd. It's closed.");
      return NULL;
    }

    if (gst_poll_fd_has_error(self->poll, &self->pollfd)) {
      GST_ERROR_OBJECT(self, "failed to read from socket fd. It has error.");
      return NULL;
    }

    if (gst_poll_fd_can_read(self->poll, &self->pollfd)) {
      GstTizenipcMessage msg = {0, };

      /* connect client */
      self->client_fd = accept(self->socket_fd, NULL, NULL);
      if (self->client_fd < 0) {
        GST_ERROR_OBJECT(self, "can not connect client");
        continue;
      }

      GST_INFO_OBJECT(self, "client accpeted : fd %d", self->client_fd);

      /* send shard memory path */
      msg.type = TIZEN_IPC_SHM_PATH;
      msg.size = strlen(self->shm_path) + 1;
      if (send(self->client_fd, &msg, sizeof(GstTizenipcMessage), MSG_NOSIGNAL) != sizeof(GstTizenipcMessage)) {
        GST_ERROR_OBJECT(self, "failed to send shard memory path 1");
        close(self->client_fd);
        self->client_fd = -1;
        continue;
      }

      if (send(self->client_fd, self->shm_path, strlen(self->shm_path) + 1, MSG_NOSIGNAL) != (strlen(self->shm_path) + 1)) {
        GST_ERROR_OBJECT(self, "failed to send shard memory path 2");
        close(self->client_fd);
        self->client_fd = -1;
        continue;
      }

      GST_INFO_OBJECT(self, "send shm path done - %s", self->shm_path);

      gst_poll_fd_init(&self->client_pollfd);
      self->client_pollfd.fd = self->client_fd;
      gst_poll_add_fd(self->poll, &self->client_pollfd);
      gst_poll_fd_ctl_read(self->poll, &self->client_pollfd, TRUE);

      g_signal_emit(self, signals[SIGNAL_CLIENT_CONNECTED], 0, self->client_pollfd.fd);
      timeout = 0;
      continue;
    }

    if (self->client_fd > -1) {
      if (gst_poll_fd_has_closed(self->poll, &self->client_pollfd)) {
        GST_WARNING_OBJECT(self, "client is gone, closing");
        goto close_client;
      }

      if (gst_poll_fd_has_error(self->poll, &self->client_pollfd)) {
        GST_WARNING_OBJECT(self, "client fd has error, closing");
        goto close_client;
      }

      /* handle message from client */
      if (gst_poll_fd_can_read(self->poll, &self->client_pollfd)) {
        GstTizenipcMessage msg = {0, };
        int *tbm_key = NULL;

        if (recv(self->client_fd, &msg, sizeof(GstTizenipcMessage), MSG_DONTWAIT) != sizeof(GstTizenipcMessage)) {
          GST_ERROR_OBJECT(self, "failed to receive message from client, closing");
          goto close_client;
        }

        switch (msg.type) {
        case TIZEN_IPC_BUFFER_NEW:
          GST_WARNING_OBJECT(self, "BUFFER_NEW???");
          break;
        case TIZEN_IPC_BUFFER_RECEIVED:
          GST_LOG_OBJECT(self, "response message received");
          g_mutex_lock(&self->ipc_lock);
          g_cond_signal(&self->ipc_cond);
          g_mutex_unlock(&self->ipc_lock);
          break;
        case TIZEN_IPC_BUFFER_RELEASE:
          tbm_key = msg.tbm_key;

          GST_LOG_OBJECT(self, "BUFFER_RELEASE : tbm key %d %d %d %d",
                               tbm_key[0], tbm_key[1], tbm_key[2], tbm_key[3]);

          _remove_buffer_from_list(self, tbm_key);
          break;
        default:
          GST_WARNING_OBJECT(self, "unknown type of message : %d", msg.type);
          break;
        }
      }

      continue;

    close_client:
      g_mutex_lock(&self->ipc_lock);

      GST_INFO_OBJECT(self, "close client fd %d", self->client_fd);

      gst_poll_remove_fd(self->poll, &self->client_pollfd);
      close(self->client_fd);
      self->client_fd = -1;

      g_mutex_unlock(&self->ipc_lock);

      g_signal_emit(self, signals[SIGNAL_CLIENT_DISCONNECTED], 0, self->client_pollfd.fd);

      continue;
    }
  }

  GST_INFO_OBJECT(self, "end");

  return NULL;
}


static gboolean plugin_init(GstPlugin *plugin)
{
  if (!gst_element_register(plugin,
                            "tizenipcsink",
                            GST_RANK_PRIMARY,
                            GST_TYPE_TIZENIPC_SINK)) {
    return FALSE;
  }

  GST_DEBUG_CATEGORY_INIT(gst_debug_tizenipc_sink,
                          "tizenipcsink",
                          0,
                          "Tizen IPC sink element");

  return TRUE;
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR,
                  GST_VERSION_MINOR,
                  tizenipcsink,
                  "Tizen IPC sink to deliver multimedia video buffer",
                  plugin_init, VERSION, GST_LICENSE,
                  "Samsung Electronics Co", "http://www.samsung.com")
