#ifndef TIZEN_SCREENSHOOTER_CLIENT_PROTOCOL_H
#define TIZEN_SCREENSHOOTER_CLIENT_PROTOCOL_H

#ifdef  __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stddef.h>
#include "wayland-client.h"

  struct wl_client;
  struct wl_resource;

  struct tizen_screenshooter;
  struct tizen_screenmirror;

  extern const struct wl_interface tizen_screenshooter_interface;
  extern const struct wl_interface tizen_screenmirror_interface;

#define TIZEN_SCREENSHOOTER_GET_SCREENMIRROR	0

  static inline void
      tizen_screenshooter_set_user_data (struct tizen_screenshooter
      *tizen_screenshooter, void *user_data)
  {
    wl_proxy_set_user_data ((struct wl_proxy *) tizen_screenshooter, user_data);
  }

  static inline void *tizen_screenshooter_get_user_data (struct
      tizen_screenshooter *tizen_screenshooter)
  {
    return wl_proxy_get_user_data ((struct wl_proxy *) tizen_screenshooter);
  }

  static inline void
      tizen_screenshooter_destroy (struct tizen_screenshooter
      *tizen_screenshooter)
  {
    wl_proxy_destroy ((struct wl_proxy *) tizen_screenshooter);
  }

  static inline struct tizen_screenmirror
      *tizen_screenshooter_get_screenmirror (struct tizen_screenshooter
      *tizen_screenshooter, struct wl_output *output)
  {
    struct wl_proxy *id;

    id = wl_proxy_marshal_constructor ((struct wl_proxy *) tizen_screenshooter,
        TIZEN_SCREENSHOOTER_GET_SCREENMIRROR, &tizen_screenmirror_interface,
        NULL, output);

    return (struct tizen_screenmirror *) id;
  }

#ifndef TIZEN_SCREENMIRROR_CONTENT_ENUM
#define TIZEN_SCREENMIRROR_CONTENT_ENUM
  enum tizen_screenmirror_content
  {
    TIZEN_SCREENMIRROR_CONTENT_NORMAL = 0,
    TIZEN_SCREENMIRROR_CONTENT_VIDEO = 1,
  };
#endif /* TIZEN_SCREENMIRROR_CONTENT_ENUM */

#ifndef TIZEN_SCREENMIRROR_STRETCH_ENUM
#define TIZEN_SCREENMIRROR_STRETCH_ENUM
  enum tizen_screenmirror_stretch
  {
    TIZEN_SCREENMIRROR_STRETCH_KEEP_RATIO = 0,
    TIZEN_SCREENMIRROR_STRETCH_FULLY = 1,
  };
#endif /* TIZEN_SCREENMIRROR_STRETCH_ENUM */

/**
 * tizen_screenmirror - interface for screenmirror
 * @dequeued: dequeued event
 * @content: content changed event
 * @stop: stop event
 *
 * A client can use this interface to get stream images of screen. Before
 * starting, queue all buffers. Then, start a screenmirror. After starting,
 * a dequeued event will occur when drawing a captured image on a buffer is
 * finished. You might need to queue the dequeued buffer again to get a new
 * image from display server.
 */
  struct tizen_screenmirror_listener
  {
        /**
	 * dequeued - dequeued event
	 * @buffer: dequeued buffer which contains a captured image
	 *
	 * occurs when drawing a captured image on a buffer is finished
	 */
    void (*dequeued) (void *data,
        struct tizen_screenmirror * tizen_screenmirror,
        struct wl_buffer * buffer);
        /**
	 * content - content changed event
	 * @content: (none)
	 *
	 * occurs when the content of a captured image is changed.
	 * (normal or video)
	 */
    void (*content) (void *data,
        struct tizen_screenmirror * tizen_screenmirror, uint32_t content);
        /**
	 * stop - stop event
	 *
	 * occurs when the screenmirror is stopped eventually
	 */
    void (*stop) (void *data, struct tizen_screenmirror * tizen_screenmirror);
  };

  static inline int
      tizen_screenmirror_add_listener (struct tizen_screenmirror
      *tizen_screenmirror, const struct tizen_screenmirror_listener *listener,
      void *data)
  {
    return wl_proxy_add_listener ((struct wl_proxy *) tizen_screenmirror,
        (void (**)(void)) listener, data);
  }

#define TIZEN_SCREENMIRROR_DESTROY	0
#define TIZEN_SCREENMIRROR_SET_STRETCH	1
#define TIZEN_SCREENMIRROR_QUEUE	2
#define TIZEN_SCREENMIRROR_DEQUEUE	3
#define TIZEN_SCREENMIRROR_START	4
#define TIZEN_SCREENMIRROR_STOP	5

  static inline void
      tizen_screenmirror_set_user_data (struct tizen_screenmirror
      *tizen_screenmirror, void *user_data)
  {
    wl_proxy_set_user_data ((struct wl_proxy *) tizen_screenmirror, user_data);
  }

  static inline void *tizen_screenmirror_get_user_data (struct
      tizen_screenmirror *tizen_screenmirror)
  {
    return wl_proxy_get_user_data ((struct wl_proxy *) tizen_screenmirror);
  }

  static inline void
      tizen_screenmirror_destroy (struct tizen_screenmirror *tizen_screenmirror)
  {
    wl_proxy_marshal ((struct wl_proxy *) tizen_screenmirror,
        TIZEN_SCREENMIRROR_DESTROY);

    wl_proxy_destroy ((struct wl_proxy *) tizen_screenmirror);
  }

  static inline void
      tizen_screenmirror_set_stretch (struct tizen_screenmirror
      *tizen_screenmirror, uint32_t stretch)
  {
    wl_proxy_marshal ((struct wl_proxy *) tizen_screenmirror,
        TIZEN_SCREENMIRROR_SET_STRETCH, stretch);
  }

  static inline void
      tizen_screenmirror_queue (struct tizen_screenmirror *tizen_screenmirror,
      struct wl_buffer *buffer)
  {
    wl_proxy_marshal ((struct wl_proxy *) tizen_screenmirror,
        TIZEN_SCREENMIRROR_QUEUE, buffer);
  }

  static inline void
      tizen_screenmirror_dequeue (struct tizen_screenmirror *tizen_screenmirror,
      struct wl_buffer *buffer)
  {
    wl_proxy_marshal ((struct wl_proxy *) tizen_screenmirror,
        TIZEN_SCREENMIRROR_DEQUEUE, buffer);
  }

  static inline void
      tizen_screenmirror_start (struct tizen_screenmirror *tizen_screenmirror)
  {
    wl_proxy_marshal ((struct wl_proxy *) tizen_screenmirror,
        TIZEN_SCREENMIRROR_START);
  }

  static inline void
      tizen_screenmirror_stop (struct tizen_screenmirror *tizen_screenmirror)
  {
    wl_proxy_marshal ((struct wl_proxy *) tizen_screenmirror,
        TIZEN_SCREENMIRROR_STOP);
  }

#ifdef  __cplusplus
}
#endif

#endif
