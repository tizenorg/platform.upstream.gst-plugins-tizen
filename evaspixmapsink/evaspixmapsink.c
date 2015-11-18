/*
 * EvasPixmapSink
 *
 * Copyright (c) 2000 - 2012 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Sangchul Lee <sc11.lee@samsung.com>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* Our interfaces */
#include <gst/video/navigation.h>
#include <gst/video/colorbalance.h>
//#include <gst/video/propertyprobe.h>
#include <gst/video/gstvideosink.h>
/* Helper functions */
#include <gst/video/video.h>

/* Object header */
#include "evaspixmapsink.h"

#ifdef GST_EXT_XV_ENHANCEMENT
/* Samsung extension headers */
/* For xv extension header for buffer transfer (output) */
#include "xv_types.h"

/* headers for drm */
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <X11/Xmd.h>
#include <dri2/dri2.h>
#include <libdrm/drm.h>

//#define DUMP_IMG
#ifdef DUMP_IMG
#include <stdio.h>
#endif
typedef enum {
	BUF_SHARE_METHOD_PADDR = 0,
	BUF_SHARE_METHOD_FD
} buf_share_method_t;

/* max channel count *********************************************************/
#define SCMN_IMGB_MAX_PLANE         (4)

/* image buffer definition ***************************************************

    +------------------------------------------+ ---
    |                                          |  ^
    |     a[], p[]                             |  |
    |     +---------------------------+ ---    |  |
    |     |                           |  ^     |  |
    |     |<---------- w[] ---------->|  |     |  |
    |     |                           |  |     |  |
    |     |                           |        |
    |     |                           |  h[]   |  e[]
    |     |                           |        |
    |     |                           |  |     |  |
    |     |                           |  |     |  |
    |     |                           |  v     |  |
    |     +---------------------------+ ---    |  |
    |                                          |  v
    +------------------------------------------+ ---

    |<----------------- s[] ------------------>|
*/

typedef struct
{
	/* width of each image plane */
	int      w[SCMN_IMGB_MAX_PLANE];
	/* height of each image plane */
	int      h[SCMN_IMGB_MAX_PLANE];
	/* stride of each image plane */
	int      s[SCMN_IMGB_MAX_PLANE];
	/* elevation of each image plane */
	int      e[SCMN_IMGB_MAX_PLANE];
	/* user space address of each image plane */
	void   * a[SCMN_IMGB_MAX_PLANE];
	/* physical address of each image plane, if needs */
	void   * p[SCMN_IMGB_MAX_PLANE];
	/* color space type of image */
	int      cs;
	/* left postion, if needs */
	int      x;
	/* top position, if needs */
	int      y;
	/* to align memory */
	int      __dummy2;
	/* arbitrary data */
	int      data[16];
	/* dma buf fd */
	int dma_buf_fd[SCMN_IMGB_MAX_PLANE];
	/* buffer share method */
	int buf_share_method;
} SCMN_IMGB;
#endif

/* Debugging category */
#include <gst/gstinfo.h>
GST_DEBUG_CATEGORY_STATIC (gst_debug_evaspixmapsink);
#define GST_CAT_DEFAULT gst_debug_evaspixmapsink
GST_DEBUG_CATEGORY_STATIC (GST_CAT_PERFORMANCE);

#ifdef GST_EXT_XV_ENHANCEMENT

enum {
    DEGREE_0,
    DEGREE_90,
    DEGREE_180,
    DEGREE_270,
    DEGREE_NUM,
};

enum {
    DISP_GEO_METHOD_LETTER_BOX = 0,
    DISP_GEO_METHOD_ORIGIN_SIZE,
    DISP_GEO_METHOD_FULL_SCREEN,
    DISP_GEO_METHOD_CROPPED_FULL_SCREEN,
    DISP_GEO_METHOD_CUSTOM_ROI,
    DISP_GEO_METHOD_NUM,
};

#define DEF_DISPLAY_GEOMETRY_METHOD DISP_GEO_METHOD_LETTER_BOX

#define GST_TYPE_EVASPIXMAPSINK_DISPLAY_GEOMETRY_METHOD (gst_evaspixmapsink_display_geometry_method_get_type())

static GType
gst_evaspixmapsink_display_geometry_method_get_type(void)
{
	static GType evaspixmapsink_display_geometry_method_type = 0;
	static const GEnumValue display_geometry_method_type[] = {
		{ 0, "Letter box", "LETTER_BOX"},
		{ 1, "Origin size", "ORIGIN_SIZE"},
		{ 2, "Full-screen", "FULL_SCREEN"},
		{ 3, "Cropped Full-screen", "CROPPED_FULL_SCREEN"},
		{ 4, "Explicitely described destination ROI", "CUSTOM_ROI"},
		{ 5, NULL, NULL},
	};

	if (!evaspixmapsink_display_geometry_method_type) {
		evaspixmapsink_display_geometry_method_type = g_enum_register_static("GstEvasPixmapSinkDisplayGeometryMethodType", display_geometry_method_type);
	}

	return evaspixmapsink_display_geometry_method_type;
}
#endif /* GST_EXT_XV_ENHANCEMENT */

typedef struct
{
  unsigned long flags;
  unsigned long functions;
  unsigned long decorations;
  long input_mode;
  unsigned long status;
}
MotifWmHints, MwmHints;

#define MWM_HINTS_DECORATIONS   (1L << 1)

static void gst_evaspixmapsink_reset (GstEvasPixmapSink *evaspixmapsink);
static void gst_evaspixmap_buffer_finalize (GstEvasPixmapBuffer *evaspixmapbuf);
static void gst_evaspixmapsink_navigation_init (GstNavigationInterface *iface);
static void gst_evaspixmapsink_colorbalance_init (GstColorBalanceInterface *iface);
static void gst_evaspixmapsink_property_probe_interface_init (GstPropertyProbeInterface *iface);
static void gst_evaspixmapsink_xcontext_clear (GstEvasPixmapSink *evaspixmapsink);
static void gst_evaspixmapsink_xpixmap_destroy (GstEvasPixmapSink *evaspixmapsink, GstXPixmap *xpixmap);
static void gst_evaspixmapsink_xpixmap_update_geometry (GstEvasPixmapSink *evaspixmapsink);
static gboolean gst_evaspixmap_buffer_put (GstEvasPixmapSink *evaspixmapsink, GstEvasPixmapBuffer *evaspixmapbuf);
static gboolean gst_evaspixmapsink_xpixmap_link (GstEvasPixmapSink *evaspixmapsink);
static void gst_evaspixmapsink_xpixmap_clear (GstEvasPixmapSink *evaspixmapsink, GstXPixmap *xpixmap);
static gint gst_evaspixmapsink_get_format_from_caps (GstEvasPixmapSink *evaspixmapsink, GstCaps *caps);
static void drm_close_gem(GstEvasPixmapSink *evaspixmapsink, unsigned int gem_handle);
#ifdef DUMP_IMG
int util_write_rawdata (const char *file, const void *data, unsigned int size);
int g_cnt = 0;
#endif
/* Default template - initiated with class struct to allow gst-register to work
   without X running */
static GstStaticPadTemplate gst_evaspixmapsink_sink_template_factory =
    GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw, "
        "framerate = (fraction) [ 0, MAX ], "
        "width = (int) [ 1, MAX ], " "height = (int) [ 1, MAX ]")
    );

enum
{
  PROP_0,
  PROP_CONTRAST,
  PROP_BRIGHTNESS,
  PROP_HUE,
  PROP_SATURATION,
  PROP_DISPLAY,
  PROP_SYNCHRONOUS,
  PROP_PIXEL_ASPECT_RATIO,
  PROP_FORCE_ASPECT_RATIO,
  PROP_DEVICE,
  PROP_DEVICE_NAME,
  PROP_DOUBLE_BUFFER,
  PROP_AUTOPAINT_COLORKEY,
  PROP_COLORKEY,
  PROP_PIXMAP_WIDTH,
  PROP_PIXMAP_HEIGHT,
#ifdef GST_EXT_XV_ENHANCEMENT
  PROP_DISPLAY_GEOMETRY_METHOD,
  PROP_ZOOM,
  PROP_DST_ROI_X,
  PROP_DST_ROI_Y,
  PROP_DST_ROI_W,
  PROP_DST_ROI_H,
  PROP_STOP_VIDEO,
#endif
  PROP_EVAS_OBJECT,
  PROP_VISIBLE,
  PROP_ORIGIN_SIZE,
};

#define gst_evaspixmapsink_parent_class parent_class

/* ============================================================= */
/*                                                               */
/*                       Private Methods                         */
/*                                                               */
/* ============================================================= */

/* evaspixmap buffers */

#define GST_TYPE_EVASPIXMAP_BUFFER (gst_evaspixmap_buffer_get_type())

#define GST_IS_EVASPIXMAP_BUFFER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_EVASPIXMAP_BUFFER))
#define GST_EVASPIXMAP_BUFFER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_EVASPIXMAP_BUFFER, GstEvasPixmapBuffer))

G_DEFINE_BOXED_TYPE(GstEvasPixmapBuffer, gst_evaspixmap_buffer, NULL, gst_evaspixmap_buffer_finalize);

static void
ecore_pipe_callback_handler (void *data, void *buffer, unsigned int nbyte)
{
	GstEvasPixmapSink *evaspixmapsink = (GstEvasPixmapSink*)data;
	GST_DEBUG_OBJECT (evaspixmapsink,"[START]");

	if (!data ) {
		GST_WARNING_OBJECT (evaspixmapsink,"data is NULL..");
		return;
	}
	if (!evaspixmapsink->eo) {
		GST_WARNING_OBJECT (evaspixmapsink,"evas object is NULL..");
		return;
	}

	/* mapping evas object with xpixmap */
	if (evaspixmapsink->do_link) {
		GST_DEBUG_OBJECT (evaspixmapsink,"do link");
		if (evaspixmapsink->xpixmap->pixmap) {
			Evas_Native_Surface surf;
			surf.version = EVAS_NATIVE_SURFACE_VERSION;
			surf.type = EVAS_NATIVE_SURFACE_X11;
			surf.data.x11.visual = ecore_x_default_visual_get(ecore_x_display_get(), ecore_x_default_screen_get());
			surf.data.x11.pixmap = evaspixmapsink->xpixmap->pixmap;
			evas_object_image_native_surface_set(evaspixmapsink->eo, &surf);
			evaspixmapsink->do_link = FALSE;
		} else {
			GST_WARNING_OBJECT (evaspixmapsink,"pixmap is NULL..");
			return;
		}
	} else {
		GST_DEBUG_OBJECT (evaspixmapsink,"update");
		/* update evas image object size */
		if (evaspixmapsink->use_origin_size) {
			evas_object_geometry_get(evaspixmapsink->eo, NULL, NULL, &evaspixmapsink->w, &evaspixmapsink->h);
		}
		evas_object_image_pixels_dirty_set (evaspixmapsink->eo, 1);
		evas_object_image_fill_set(evaspixmapsink->eo, 0, 0, evaspixmapsink->w, evaspixmapsink->h);
		evas_object_image_data_update_add(evaspixmapsink->eo, 0, 0, evaspixmapsink->w, evaspixmapsink->h);
	}

	GST_DEBUG_OBJECT (evaspixmapsink,"[END]");
}

static void
evas_callback_resize_event (void *data, Evas *e, Evas_Object *obj, void *event_info)
{
	int w = 0;
	int h = 0;
	float former_ratio = 0;
	float ratio = 0;
	float abs_margin = 0;

	GstEvasPixmapSink *evaspixmapsink = (GstEvasPixmapSink *)data;
	GST_DEBUG_OBJECT (evaspixmapsink,"[START]");

	evas_object_geometry_get(evaspixmapsink->eo, NULL, NULL, &w, &h);
	GST_DEBUG_OBJECT (evaspixmapsink,"resized : w(%d), h(%d)", w, h);
	if (!evaspixmapsink->use_origin_size &&
			(evaspixmapsink->w != w || evaspixmapsink->h != h)) {
		former_ratio = (float)evaspixmapsink->w / evaspixmapsink->h;
		ratio = (float)w / h;
		evaspixmapsink->w = w;
		evaspixmapsink->h = h;

#ifdef COMPARE_RATIO
		GST_DEBUG_OBJECT (evaspixmapsink,"resized : ratio(%.3f=>%.3f)", former_ratio, ratio);
		if ( former_ratio >= ratio ) {
			abs_margin = former_ratio - ratio;
		} else {
			abs_margin = ratio - former_ratio;
		}
		/* re-link_pixmap can only be set when ratio is changed */
		if ( abs_margin >= MARGIN_OF_ERROR ) {
#endif
			if (!gst_evaspixmapsink_xpixmap_link(evaspixmapsink)) {
				GST_ERROR_OBJECT (evaspixmapsink,"link evas image object with pixmap failed...");
				return;
			}
#ifdef COMPARE_RATIO
		}
#endif
	}

	gst_evaspixmap_buffer_put (evaspixmapsink, evaspixmapsink->evas_pixmap_buf);

	GST_DEBUG_OBJECT (evaspixmapsink,"[END]");
}

static inline gboolean
is_evas_image_object (Evas_Object *obj)
{
	const char *type;
	if (!obj) {
		return FALSE;
	}
	type = evas_object_type_get (obj);
	if (!type) {
		return FALSE;
	}
	if (strcmp (type, "image") == 0) {
		return TRUE;
	}
	return FALSE;
}

static void
evas_callback_del_event (void *data, Evas *e, Evas_Object *obj, void *event_info)
{
	GstEvasPixmapSink *evaspixmapsink = data;
	if (!evaspixmapsink) {
		GST_WARNING ("evaspixmapsink is NULL..");
		return;
	}
	GST_DEBUG_OBJECT (evaspixmapsink,"[START]");

	evas_object_event_callback_del(evaspixmapsink->eo, EVAS_CALLBACK_RESIZE, evas_callback_resize_event);
	if (evaspixmapsink->eo) {
		evas_object_image_native_surface_set(evaspixmapsink->eo, NULL);
		evaspixmapsink->eo = NULL;
	}

	GST_DEBUG_OBJECT (evaspixmapsink,"[END]");
}

/* X11 stuff */
static gboolean error_caught = FALSE;

static int
gst_evaspixmapsink_handle_xerror (Display * display, XErrorEvent * xevent)
{
  char error_msg[1024];

  XGetErrorText (display, xevent->error_code, error_msg, 1024);
  GST_DEBUG ("evaspixmapsink triggered an XError. error: %s", error_msg);
  error_caught = TRUE;
  return 0;
}

#ifdef DUMP_IMG
int util_write_rawdata(const char *file, const void *data, unsigned int size)
{
  FILE *fp;

  fp = fopen(file, "wb");
  if (fp == NULL)
  {
    GST_WARNING("fopen fail... size : %d", size);
    return -1;
  }
  fwrite((char*)data, sizeof(char), size, fp);
  fclose(fp);

  return 0;
}
#endif

#ifdef HAVE_XSHM
/* This function checks that it is actually really possible to create an image
   using XShm */
static gboolean
gst_evaspixmapsink_check_xshm_calls (GstXContext * xcontext)
{
  XvImage *xvimage;
  XShmSegmentInfo SHMInfo;
  gint size;
  int (*handler) (Display *, XErrorEvent *);
  gboolean result = FALSE;
  gboolean did_attach = FALSE;

  g_return_val_if_fail (xcontext != NULL, FALSE);

  /* Sync to ensure any older errors are already processed */
  XSync (xcontext->disp, FALSE);

  /* Set defaults so we don't free these later unnecessarily */
  SHMInfo.shmaddr = ((void *) -1);
  SHMInfo.shmid = -1;

  /* Setting an error handler to catch failure */
  error_caught = FALSE;
  handler = XSetErrorHandler (gst_evaspixmapsink_handle_xerror);

  /* Trying to create a 1x1 picture */
  GST_DEBUG ("XvShmCreateImage of 1x1");
  xvimage = XvShmCreateImage (xcontext->disp, xcontext->xv_port_id,
      xcontext->im_format, NULL, 1, 1, &SHMInfo);

  /* Might cause an error, sync to ensure it is noticed */
  XSync (xcontext->disp, FALSE);
  if (!xvimage || error_caught) {
    GST_WARNING ("could not XvShmCreateImage a 1x1 image");
    goto beach;
  }
  size = xvimage->data_size;

  SHMInfo.shmid = shmget (IPC_PRIVATE, size, IPC_CREAT | 0777);
  if (SHMInfo.shmid == -1) {
    GST_WARNING ("could not get shared memory of %d bytes", size);
    goto beach;
  }

  SHMInfo.shmaddr = shmat (SHMInfo.shmid, NULL, 0);
  if (SHMInfo.shmaddr == ((void *) -1)) {
    GST_WARNING ("Failed to shmat: %s", g_strerror (errno));
    /* Clean up the shared memory segment */
    shmctl (SHMInfo.shmid, IPC_RMID, NULL);
    goto beach;
  }

  xvimage->data = SHMInfo.shmaddr;
  SHMInfo.readOnly = FALSE;

  if (XShmAttach (xcontext->disp, &SHMInfo) == 0) {
    GST_WARNING ("Failed to XShmAttach");
    /* Clean up the shared memory segment */
    shmctl (SHMInfo.shmid, IPC_RMID, NULL);
    goto beach;
  }

  /* Sync to ensure we see any errors we caused */
  XSync (xcontext->disp, FALSE);

  /* Delete the shared memory segment as soon as everyone is attached.
   * This way, it will be deleted as soon as we detach later, and not
   * leaked if we crash. */
  shmctl (SHMInfo.shmid, IPC_RMID, NULL);

  if (!error_caught) {
    GST_DEBUG ("XServer ShmAttached to 0x%x, id 0x%lx", SHMInfo.shmid,
        SHMInfo.shmseg);

    did_attach = TRUE;
    /* store whether we succeeded in result */
    result = TRUE;
  } else {
    GST_WARNING ("MIT-SHM extension check failed at XShmAttach. "
        "Not using shared memory.");
  }

beach:
  /* Sync to ensure we swallow any errors we caused and reset error_caught */
  XSync (xcontext->disp, FALSE);

  error_caught = FALSE;
  XSetErrorHandler (handler);

  if (did_attach) {
    GST_DEBUG ("XServer ShmDetaching from 0x%x id 0x%lx",
        SHMInfo.shmid, SHMInfo.shmseg);
    XShmDetach (xcontext->disp, &SHMInfo);
    XSync (xcontext->disp, FALSE);
  }
  if (SHMInfo.shmaddr != ((void *) -1))
    shmdt (SHMInfo.shmaddr);
  if (xvimage)
    XFree (xvimage);
  return result;
}
#endif /* HAVE_XSHM */

/* This function destroys a GstEvasPixmap handling XShm availability */
static void
gst_evaspixmap_buffer_destroy (GstEvasPixmapBuffer *evaspixmapbuf)
{
	GstEvasPixmapSink *evaspixmapsink;

	GST_DEBUG_OBJECT (evaspixmapsink,"Destroying buffer");

	evaspixmapsink = evaspixmapbuf->evaspixmapsink;
	if (G_UNLIKELY (evaspixmapsink == NULL)) {
		goto no_sink;
	}

	g_return_if_fail (GST_IS_EVASPIXMAPSINK (evaspixmapsink));

	GST_OBJECT_LOCK (evaspixmapsink);

	/* We might have some buffers destroyed after changing state to NULL */
	if (evaspixmapsink->xcontext == NULL) {
		GST_DEBUG_OBJECT (evaspixmapsink,"Destroying XvImage after Xcontext");
#ifdef HAVE_XSHM
		/* Need to free the shared memory segment even if the x context
		* was already cleaned up */
		if (evaspixmapbuf->SHMInfo.shmaddr != ((void *) -1)) {
			shmdt (evaspixmapbuf->SHMInfo.shmaddr);
		}
#endif
		goto beach;
	}
	g_mutex_lock (evaspixmapsink->x_lock);

#ifdef HAVE_XSHM
	if (evaspixmapsink->xcontext->use_xshm) {
		if (evaspixmapbuf->SHMInfo.shmaddr != ((void *) -1)) {
			GST_DEBUG_OBJECT (evaspixmapsink,"XServer ShmDetaching from 0x%x id 0x%lx", evaspixmapbuf->SHMInfo.shmid, evaspixmapbuf->SHMInfo.shmseg);
			XShmDetach (evaspixmapsink->xcontext->disp, &evaspixmapbuf->SHMInfo);
			XSync (evaspixmapsink->xcontext->disp, FALSE);
			shmdt (evaspixmapbuf->SHMInfo.shmaddr);
		}
		if (evaspixmapbuf->xvimage)
		XFree (evaspixmapbuf->xvimage);
	} else
#endif /* HAVE_XSHM */
	{
		if (evaspixmapbuf->xvimage) {
			if (evaspixmapbuf->xvimage->data) {
				g_free (evaspixmapbuf->xvimage->data);
			}
			XFree (evaspixmapbuf->xvimage);
		}
	}

	XSync (evaspixmapsink->xcontext->disp, FALSE);

	g_mutex_unlock (evaspixmapsink->x_lock);

beach:
	GST_OBJECT_UNLOCK (evaspixmapsink);
	evaspixmapbuf->evaspixmapsink = NULL;
	gst_object_unref (evaspixmapsink);
    gst_buffer_unref(evaspixmapbuf->buffer);
    free(evaspixmapbuf);
	return;

	no_sink:
	{
		GST_WARNING_OBJECT (evaspixmapsink,"no sink found");
		return;
	}
}

static void
gst_evaspixmap_buffer_finalize (GstEvasPixmapBuffer *evaspixmapbuf)
{
	GstEvasPixmapSink *evaspixmapsink;

	evaspixmapsink = evaspixmapbuf->evaspixmapsink;
	if (G_UNLIKELY (evaspixmapsink == NULL)) {
		GST_WARNING_OBJECT (evaspixmapsink,"no sink found");
		return;
	}
	g_return_if_fail (GST_IS_EVASPIXMAPSINK (evaspixmapsink));

	 /* If our geometry changed we can't reuse that image. */
	GST_LOG_OBJECT (evaspixmapsink,"destroy image as sink is shutting down");
	gst_evaspixmap_buffer_destroy (evaspixmapbuf);
}

static void
gst_evaspixmap_buffer_free (GstEvasPixmapBuffer *evaspixmapbuf)
{
  /* make sure it is not recycled */
  evaspixmapbuf->width = -1;
  evaspixmapbuf->height = -1;
  g_boxed_free(GST_TYPE_EVASPIXMAP_BUFFER, evaspixmapbuf);
}

static void
gst_evaspixmap_buffer_init (GstEvasPixmapBuffer *evaspixmapbuf, gpointer g_class)
{
#ifdef HAVE_XSHM
  evaspixmapbuf->SHMInfo.shmaddr = ((void *) -1);
  evaspixmapbuf->SHMInfo.shmid = -1;
#endif
}

static void
gst_evaspixmap_buffer_class_init (gpointer g_class, gpointer class_data)
{
}

/* This function handles GstEvasPixmapBuffer creation depending on XShm availability */
static GstEvasPixmapBuffer*
gst_evaspixmap_buffer_new (GstEvasPixmapSink *evaspixmapsink, GstCaps *caps)
{
	GstEvasPixmapBuffer *evaspixmapbuf = NULL;
	GstStructure *structure = NULL;
	gboolean succeeded = FALSE;
	int (*handler) (Display *, XErrorEvent *);

	g_return_val_if_fail (GST_IS_EVASPIXMAPSINK (evaspixmapsink), NULL);

	if (caps == NULL) {
		return NULL;
	}

	evaspixmapbuf = (GstEvasPixmapBuffer*) malloc (sizeof(GstEvasPixmapBuffer));
	GST_DEBUG_OBJECT (evaspixmapsink,"Creating new EvasPixmapBuffer");
	evaspixmapbuf->buffer = gst_buffer_new();

	structure = gst_caps_get_structure (caps, 0);

	if (!gst_structure_get_int (structure, "width", &evaspixmapbuf->width) || !gst_structure_get_int (structure, "height", &evaspixmapbuf->height)) {
		GST_WARNING_OBJECT (evaspixmapsink,"failed getting geometry from caps %" GST_PTR_FORMAT, caps);
	}

	GST_LOG_OBJECT (evaspixmapsink,"creating %dx%d", evaspixmapbuf->width, evaspixmapbuf->height);
#ifdef GST_EXT_XV_ENHANCEMENT
	GST_LOG_OBJECT (evaspixmapsink,"aligned size %dx%d", evaspixmapsink->aligned_width, evaspixmapsink->aligned_height);
	if (evaspixmapsink->aligned_width == 0 || evaspixmapsink->aligned_height == 0) {
		GST_INFO_OBJECT (evaspixmapsink,"aligned size is zero. set size of caps.");
		evaspixmapsink->aligned_width = evaspixmapbuf->width;
		evaspixmapsink->aligned_height = evaspixmapbuf->height;
	}
#endif

	evaspixmapbuf->im_format = gst_evaspixmapsink_get_format_from_caps (evaspixmapsink, caps);
	if (evaspixmapbuf->im_format == -1) {
		GST_WARNING_OBJECT (evaspixmapsink,"failed to get format from caps %"GST_PTR_FORMAT, caps);
		GST_ELEMENT_ERROR (evaspixmapsink, RESOURCE, WRITE,("Failed to create output image buffer of %dx%d pixels",
				   evaspixmapbuf->width, evaspixmapbuf->height), ("Invalid input caps"));
		goto beach_unlocked;
	}
	evaspixmapbuf->evaspixmapsink = gst_object_ref (evaspixmapsink);

	g_mutex_lock (evaspixmapsink->x_lock);

	/* Setting an error handler to catch failure */
	error_caught = FALSE;
	handler = XSetErrorHandler (gst_evaspixmapsink_handle_xerror);

#ifdef HAVE_XSHM
	if (evaspixmapsink->xcontext->use_xshm) {
		int expected_size;
		evaspixmapbuf->xvimage = XvShmCreateImage (evaspixmapsink->xcontext->disp, evaspixmapsink->xcontext->xv_port_id, evaspixmapbuf->im_format, NULL,
#ifdef GST_EXT_XV_ENHANCEMENT
		evaspixmapsink->aligned_width, evaspixmapsink->aligned_height, &evaspixmapbuf->SHMInfo);
		if(!evaspixmapbuf->xvimage) {
			GST_ERROR_OBJECT (evaspixmapsink,"XvShmCreateImage() failed");
		}
#else
		evaspixmapbuf->width, evaspixmapbuf->height, &evaspixmapbuf->SHMInfo);
#endif
		if (!evaspixmapbuf->xvimage || error_caught) {
			if (error_caught) {
				GST_ERROR_OBJECT (evaspixmapsink,"error_caught!");
			}
			g_mutex_unlock (evaspixmapsink->x_lock);
			/* Reset error handler */
			error_caught = FALSE;
			XSetErrorHandler (handler);
			/* Push an error */
			GST_ELEMENT_ERROR (evaspixmapsink, RESOURCE, WRITE,("Failed to create output image buffer of %dx%d pixels",evaspixmapbuf->width,
				   evaspixmapbuf->height),("could not XvShmCreateImage a %dx%d image",evaspixmapbuf->width, evaspixmapbuf->height));
			goto beach_unlocked;
		}

		/* we have to use the returned data_size for our shm size */
		evaspixmapbuf->size = evaspixmapbuf->xvimage->data_size;
		GST_LOG_OBJECT (evaspixmapsink,"XShm image size is %" G_GSIZE_FORMAT, evaspixmapbuf->size);

		/* calculate the expected size.  This is only for sanity checking the
		* number we get from X. */
		switch (evaspixmapbuf->im_format) {
		case GST_MAKE_FOURCC ('I', '4', '2', '0'):
		case GST_MAKE_FOURCC ('Y', 'V', '1', '2'):
		{
			gint pitches[3];
			gint offsets[3];
			guint plane;

			offsets[0] = 0;
			pitches[0] = GST_ROUND_UP_4 (evaspixmapbuf->width);
			offsets[1] = offsets[0] + pitches[0] * GST_ROUND_UP_2 (evaspixmapbuf->height);
			pitches[1] = GST_ROUND_UP_8 (evaspixmapbuf->width) / 2;
			offsets[2] =
			offsets[1] + pitches[1] * GST_ROUND_UP_2 (evaspixmapbuf->height) / 2;
			pitches[2] = GST_ROUND_UP_8 (pitches[0]) / 2;

			expected_size =	offsets[2] + pitches[2] * GST_ROUND_UP_2 (evaspixmapbuf->height) / 2;

			for (plane = 0; plane < evaspixmapbuf->xvimage->num_planes; plane++) {
				GST_DEBUG_OBJECT (evaspixmapsink,"Plane %u has a expected pitch of %d bytes, " "offset of %d",
						plane, pitches[plane], offsets[plane]);
			}
			break;
		}
		case GST_MAKE_FOURCC ('Y', 'U', 'Y', '2'):
		case GST_MAKE_FOURCC ('U', 'Y', 'V', 'Y'):
			expected_size = evaspixmapbuf->height * GST_ROUND_UP_4 (evaspixmapbuf->width * 2);
			break;

		#ifdef GST_EXT_XV_ENHANCEMENT
		case GST_MAKE_FOURCC ('S', 'T', '1', '2'):
		case GST_MAKE_FOURCC ('S', 'N', '1', '2'):
		case GST_MAKE_FOURCC ('S', 'U', 'Y', 'V'):
		case GST_MAKE_FOURCC ('S', 'U', 'Y', '2'):
		case GST_MAKE_FOURCC ('S', '4', '2', '0'):
		case GST_MAKE_FOURCC ('S', 'Y', 'V', 'Y'):
			expected_size = sizeof(SCMN_IMGB);
			break;
		#endif
		default:
			expected_size = 0;
			break;
		}
		if (expected_size != 0 && evaspixmapbuf->size != expected_size) {
			GST_WARNING_OBJECT (evaspixmapsink,"unexpected XShm image size (got %" G_GSIZE_FORMAT ", expected %d)", evaspixmapbuf->size, expected_size);
		}

		/* Be verbose about our XvImage stride */
		{
			guint plane;
			for (plane = 0; plane < evaspixmapbuf->xvimage->num_planes; plane++) {
				GST_DEBUG_OBJECT (evaspixmapsink,"Plane %u has a pitch of %d bytes, ""offset of %d", plane,
						evaspixmapbuf->xvimage->pitches[plane],	evaspixmapbuf->xvimage->offsets[plane]);
			}
		}

		evaspixmapbuf->SHMInfo.shmid = shmget (IPC_PRIVATE, evaspixmapbuf->size,IPC_CREAT | 0777);
		if (evaspixmapbuf->SHMInfo.shmid == -1) {
			g_mutex_unlock (evaspixmapsink->x_lock);
			GST_ELEMENT_ERROR (evaspixmapsink, RESOURCE, WRITE,
				("Failed to create output image buffer of %dx%d pixels", evaspixmapbuf->width, evaspixmapbuf->height),
				("could not get shared memory of %" G_GSIZE_FORMAT " bytes",evaspixmapbuf->size));
			goto beach_unlocked;
		}

		evaspixmapbuf->SHMInfo.shmaddr = shmat (evaspixmapbuf->SHMInfo.shmid, NULL, 0);
		if (evaspixmapbuf->SHMInfo.shmaddr == ((void *) -1)) {
			g_mutex_unlock (evaspixmapsink->x_lock);
			GST_ELEMENT_ERROR (evaspixmapsink, RESOURCE, WRITE,
				("Failed to create output image buffer of %dx%d pixels",
				evaspixmapbuf->width, evaspixmapbuf->height),
				("Failed to shmat: %s", g_strerror (errno)));
			/* Clean up the shared memory segment */
			shmctl (evaspixmapbuf->SHMInfo.shmid, IPC_RMID, NULL);
			goto beach_unlocked;
		}

		evaspixmapbuf->xvimage->data = evaspixmapbuf->SHMInfo.shmaddr;
		evaspixmapbuf->SHMInfo.readOnly = FALSE;

		if (XShmAttach (evaspixmapsink->xcontext->disp, &evaspixmapbuf->SHMInfo) == 0) {
		/* Clean up the shared memory segment */
		shmctl (evaspixmapbuf->SHMInfo.shmid, IPC_RMID, NULL);

		g_mutex_unlock (evaspixmapsink->x_lock);
		GST_ELEMENT_ERROR (evaspixmapsink, RESOURCE, WRITE,
			("Failed to create output image buffer of %dx%d pixels",
			evaspixmapbuf->width, evaspixmapbuf->height), ("Failed to XShmAttach"));
		goto beach_unlocked;
		}

		XSync (evaspixmapsink->xcontext->disp, FALSE);

		/* Delete the shared memory segment as soon as we everyone is attached.
		* This way, it will be deleted as soon as we detach later, and not
		* leaked if we crash. */
		shmctl (evaspixmapbuf->SHMInfo.shmid, IPC_RMID, NULL);

		GST_DEBUG_OBJECT (evaspixmapsink,"XServer ShmAttached to 0x%x, id 0x%lx", evaspixmapbuf->SHMInfo.shmid, evaspixmapbuf->SHMInfo.shmseg);
	} else
#endif /* HAVE_XSHM */
	{
		evaspixmapbuf->xvimage = XvCreateImage (evaspixmapsink->xcontext->disp,	evaspixmapsink->xcontext->xv_port_id,
#ifdef GST_EXT_XV_ENHANCEMENT
		evaspixmapbuf->im_format, NULL, evaspixmapsink->aligned_width, evaspixmapsink->aligned_height);
#else
		evaspixmapbuf->im_format, NULL, evaspixmapbuf->width, evaspixmapbuf->height);
#endif
		if (!evaspixmapbuf->xvimage || error_caught) {
			g_mutex_unlock (evaspixmapsink->x_lock);
			/* Reset error handler */
			error_caught = FALSE;
			XSetErrorHandler (handler);
			/* Push an error */
			GST_ELEMENT_ERROR (evaspixmapsink, RESOURCE, WRITE,
				("Failed to create outputimage buffer of %dx%d pixels",
				evaspixmapbuf->width, evaspixmapbuf->height),
				("could not XvCreateImage a %dx%d image",
				evaspixmapbuf->width, evaspixmapbuf->height));
			goto beach_unlocked;
		}

		/* we have to use the returned data_size for our image size */
		evaspixmapbuf->size = evaspixmapbuf->xvimage->data_size;
		evaspixmapbuf->xvimage->data = g_malloc (evaspixmapbuf->size);

		XSync (evaspixmapsink->xcontext->disp, FALSE);
	}

	/* Reset error handler */
	error_caught = FALSE;
	XSetErrorHandler (handler);

	succeeded = TRUE;

	gst_buffer_replace_all_memory(evaspixmapbuf->buffer, gst_memory_new_wrapped(GST_MEMORY_FLAG_READONLY,
                                                                                (guchar *) evaspixmapbuf->xvimage->data,
                                                                                evaspixmapbuf->size,
                                                                                0,
                                                                                evaspixmapbuf->size,
                                                                                NULL, NULL));
	g_mutex_unlock (evaspixmapsink->x_lock);

beach_unlocked:
	if (!succeeded) {
		gst_evaspixmap_buffer_free (evaspixmapbuf);
		evaspixmapbuf = NULL;
	}

	return evaspixmapbuf;
}

/* This function puts a GstEvasPixmapBuffer on a GstEvasPixmapSink's pixmap. Returns FALSE
 * if no pixmap was available  */
static gboolean
gst_evaspixmap_buffer_put (GstEvasPixmapSink *evaspixmapsink, GstEvasPixmapBuffer *evaspixmapbuf)
{
	GstVideoRectangle result;

#ifdef GST_EXT_XV_ENHANCEMENT
	GstVideoRectangle src_origin = { 0, 0, 0, 0};
	GstVideoRectangle src_input  = { 0, 0, 0, 0};
	GstVideoRectangle src = { 0, 0, 0, 0};
	GstVideoRectangle dst = { 0, 0, 0, 0};
	int rotate = 0;
	int ret = 0;
#endif

	/* We take the flow_lock. If expose is in there we don't want to run
	concurrently from the data flow thread */
	g_mutex_lock (evaspixmapsink->flow_lock);

	if (G_UNLIKELY (evaspixmapsink->xpixmap == NULL)) {
		GST_WARNING_OBJECT (evaspixmapsink, "xpixmap is NULL. Skip buffer_put." );
		g_mutex_unlock(evaspixmapsink->flow_lock);
		return FALSE;
	}
	if (evaspixmapsink->visible == FALSE) {
		GST_WARNING_OBJECT (evaspixmapsink, "visible is FALSE. Skip buffer_put." );
		g_mutex_unlock(evaspixmapsink->flow_lock);
		return TRUE;
	}
	if (!evaspixmapbuf) {
		GST_WARNING_OBJECT (evaspixmapsink, "evaspixmapbuf is NULL. Skip buffer_put." );
		g_mutex_unlock(evaspixmapsink->flow_lock);
		return TRUE;
	}

#ifdef GST_EXT_XV_ENHANCEMENT
	gst_evaspixmapsink_xpixmap_update_geometry( evaspixmapsink );

	src.x = src.y = 0;
	src_origin.x = src_origin.y = src_input.x = src_input.y = 0;
	src_input.w = src_origin.w = evaspixmapsink->video_width;
	src_input.h = src_origin.h = evaspixmapsink->video_height;
	src.w = src_origin.w;
	src.h = src_origin.h;

	dst.w = evaspixmapsink->render_rect.w; /* pixmap width */
	dst.h = evaspixmapsink->render_rect.h; /* pixmap heighy */

	if (!evaspixmapsink->use_origin_size) {
		switch (evaspixmapsink->display_geometry_method) {
		case DISP_GEO_METHOD_LETTER_BOX:
			gst_video_sink_center_rect (src, dst, &result, TRUE);
			result.x += evaspixmapsink->render_rect.x;
			result.y += evaspixmapsink->render_rect.y;
			GST_DEBUG_OBJECT (evaspixmapsink, "GEO_METHOD : letter box");
			break;
		case DISP_GEO_METHOD_ORIGIN_SIZE:
			gst_video_sink_center_rect (src, dst, &result, FALSE);
			gst_video_sink_center_rect (dst, src, &src_input, FALSE);
			GST_DEBUG_OBJECT (evaspixmapsink, "GEO_METHOD : origin size");
			break;
		case DISP_GEO_METHOD_CROPPED_FULL_SCREEN:
			GST_DEBUG_OBJECT (evaspixmapsink, "GEO_METHOD : cropped full screen");
			gst_video_sink_center_rect(dst, src, &src_input, TRUE);
			result.x = result.y = 0;
			result.w = dst.w;
			result.h = dst.h;
			break;
		case DISP_GEO_METHOD_FULL_SCREEN:
			result.x = result.y = 0;
			result.w = evaspixmapsink->xpixmap->width;
			result.h = evaspixmapsink->xpixmap->height;
			GST_DEBUG_OBJECT (evaspixmapsink, "GEO_METHOD : full screen");
			break;
		case DISP_GEO_METHOD_CUSTOM_ROI:
			result.x = evaspixmapsink->dst_roi.x;
			result.y = evaspixmapsink->dst_roi.y;
			result.w = evaspixmapsink->dst_roi.w;
			result.h = evaspixmapsink->dst_roi.h;
			GST_DEBUG_OBJECT (evaspixmapsink, "GEO_METHOD : custom roi");
			break;
		default:
			break;
		}
		GST_DEBUG_OBJECT (evaspixmapsink, "GEO_METHOD : src(%dx%d), dst(%dx%d), result(%dx%d), result_x(%d), result_y(%d)",
				src.w,src.h,dst.w,dst.h,result.w,result.h,result.x,result.y);
	} else {
		result.x = result.y = 0;
		result.w = evaspixmapsink->xpixmap->width;
		result.h = evaspixmapsink->xpixmap->height;
		GST_DEBUG_OBJECT (evaspixmapsink, "USE ORIGIN SIZE, no geometry method" );
	}
#else
	if (evaspixmapsink->keep_aspect) {
		GstVideoRectangle src, dst;
		/* We use the calculated geometry from _setcaps as a source to respect
		source and screen pixel aspect ratios. */
		src.w = GST_VIDEO_SINK_WIDTH (evaspixmapsink);
		src.h = GST_VIDEO_SINK_HEIGHT (evaspixmapsink);
		dst.w = evaspixmapsink->render_rect.w;
		dst.h = evaspixmapsink->render_rect.h;

		gst_video_sink_center_rect (src, dst, &result, TRUE);
		result.x += evaspixmapsink->render_rect.x;
		result.y += evaspixmapsink->render_rect.y;
	} else {
		memcpy (&result, &evaspixmapsink->render_rect, sizeof (GstVideoRectangle));
	}
#endif

	g_mutex_lock (evaspixmapsink->x_lock);

  /* We scale to the pixmap's geometry */
#ifdef HAVE_XSHM
	if (evaspixmapsink->xcontext->use_xshm) {
		GST_LOG_OBJECT (evaspixmapsink,"XvShmPutImage with image %dx%d and pixmap %dx%d, from xvimage %"
				GST_PTR_FORMAT,
				evaspixmapbuf->width, evaspixmapbuf->height,
				evaspixmapsink->render_rect.w, evaspixmapsink->render_rect.h, evaspixmapbuf);

#ifdef GST_EXT_XV_ENHANCEMENT
	/* Trim as proper size */
	if (src_input.w % 2 == 1) {
		src_input.w += 1;
	}
	if (src_input.h % 2 == 1) {
		src_input.h += 1;
	}

	GST_LOG_OBJECT (evaspixmapsink, "screen[%dx%d],pixmap[%d,%d,%dx%d],method[%d],rotate[%d],src[%dx%d],dst[%d,%d,%dx%d],input[%d,%d,%dx%d],result[%d,%d,%dx%d]",
		evaspixmapsink->scr_w, evaspixmapsink->scr_h,
		evaspixmapsink->xpixmap->x, evaspixmapsink->xpixmap->y, evaspixmapsink->xpixmap->width, evaspixmapsink->xpixmap->height,
		evaspixmapsink->display_geometry_method, rotate,
		src_origin.w, src_origin.h,
		dst.x, dst.y, dst.w, dst.h,
		src_input.x, src_input.y, src_input.w, src_input.h,
		result.x, result.y, result.w, result.h );

	if (evaspixmapsink->visible) {
		ret = XvShmPutImage (evaspixmapsink->xcontext->disp,
			evaspixmapsink->xcontext->xv_port_id,
			evaspixmapsink->xpixmap->pixmap,
			evaspixmapsink->xpixmap->gc, evaspixmapbuf->xvimage,
			src_input.x, src_input.y, src_input.w, src_input.h,
			result.x, result.y, result.w, result.h, FALSE);
		GST_LOG_OBJECT (evaspixmapsink, "XvShmPutImage return value [%d]", ret );
#ifdef DUMP_IMG
	int ret = 0;
	char file_name[128];
	int dump_size = evaspixmapbuf->xvimage->data_size;

	if(g_cnt<10) {
		GST_INFO ("DUMP IMG_%2.2d : buffer size(%d)", g_cnt, dump_size);
		sprintf(file_name, "/opt/usr/media/DUMP_IMG_%2.2d.dump", g_cnt);
		ret = util_write_rawdata(file_name, evaspixmapbuf->xvimage->data, dump_size);
		if (ret) {
			GST_ERROR_OBJECT (evaspixmapsink, "util_write_rawdata() failed");
		} else
			g_cnt++;
	}
#endif
	} else {
		GST_WARNING_OBJECT (evaspixmapsink, "visible is FALSE. skip this image..." );
	}
#else /* GST_EXT_XV_ENHANCEMENT */
	if (evaspixmapsink->visible) {
	XvShmPutImage (evaspixmapsink->xcontext->disp,
		evaspixmapsink->xcontext->xv_port_id,
		evaspixmapsink->xpixmap->pixmap,
		evaspixmapsink->xpixmap->gc, evaspixmapbuf->xvimage,
		evaspixmapsink->disp_x, evaspixmapsink->disp_y,
		evaspixmapsink->disp_width, evaspixmapsink->disp_height,
		result.x, result.y, result.w, result.h, FALSE);
	} else {
		GST_WARNING_OBJECT (evaspixmapsink, "visible is FALSE. skip this image..." );
	}
#endif /* GST_EXT_XV_ENHANCEMENT */
  } else
#endif /* HAVE_XSHM */
	{
		if (evaspixmapsink->visible) {
		XvPutImage (evaspixmapsink->xcontext->disp,
			evaspixmapsink->xcontext->xv_port_id,
			evaspixmapsink->xpixmap->pixmap,
			evaspixmapsink->xpixmap->gc, evaspixmapbuf->xvimage,
			evaspixmapsink->disp_x, evaspixmapsink->disp_y,
			evaspixmapsink->disp_width, evaspixmapsink->disp_height,
			result.x, result.y, result.w, result.h);
		} else {
			GST_WARNING_OBJECT (evaspixmapsink, "visible is FALSE. skip this image..." );
		}
	}
	XSync (evaspixmapsink->xcontext->disp, FALSE);

	g_mutex_unlock (evaspixmapsink->x_lock);
	g_mutex_unlock (evaspixmapsink->flow_lock);


	return TRUE;
}

static int
drm_init(GstEvasPixmapSink *evaspixmapsink)
{
	Display *dpy;
	int i = 0;
	int eventBase = 0;
	int errorBase = 0;
	int dri2Major = 0;
	int dri2Minor = 0;
	char *driverName = NULL;
	char *deviceName = NULL;
	struct drm_auth auth_arg = {0};

	evaspixmapsink->drm_fd = -1;

	dpy = XOpenDisplay(0);

	/* DRI2 */
	if (!DRI2QueryExtension(dpy, &eventBase, &errorBase)) {
		GST_ERROR_OBJECT (evaspixmapsink,"failed to DRI2QueryExtension()");
		goto ERROR_CASE;
	}

	if (!DRI2QueryVersion(dpy, &dri2Major, &dri2Minor)) {
		GST_ERROR_OBJECT (evaspixmapsink,"failed to DRI2QueryVersion");
		goto ERROR_CASE;
	}

	if (!DRI2Connect(dpy, RootWindow(dpy, DefaultScreen(dpy)), &driverName, &deviceName)) {
		GST_ERROR_OBJECT (evaspixmapsink,"failed to DRI2Connect");
		goto ERROR_CASE;
	}

	if (!driverName || !deviceName) {
		GST_ERROR_OBJECT (evaspixmapsink,"driverName or deviceName is not valid");
		goto ERROR_CASE;
	}

	GST_INFO_OBJECT (evaspixmapsink,"Open drm device : %s", deviceName);

	/* get the drm_fd though opening the deviceName */
	evaspixmapsink->drm_fd = open(deviceName, O_RDWR);
	if (evaspixmapsink->drm_fd < 0) {
		GST_ERROR_OBJECT (evaspixmapsink,"cannot open drm device (%s)", deviceName);
		goto ERROR_CASE;
	}

	/* get magic from drm to authentication */
	if (ioctl(evaspixmapsink->drm_fd, DRM_IOCTL_GET_MAGIC, &auth_arg)) {
		GST_ERROR_OBJECT (evaspixmapsink,"cannot get drm auth magic");
		close(evaspixmapsink->drm_fd);
		evaspixmapsink->drm_fd = -1;
		goto ERROR_CASE;
	}

	if (!DRI2Authenticate(dpy, RootWindow(dpy, DefaultScreen(dpy)),	auth_arg.magic)) {
		GST_ERROR_OBJECT (evaspixmapsink,"cannot get drm authentication from X");
		close(evaspixmapsink->drm_fd);
		evaspixmapsink->drm_fd = -1;
		goto ERROR_CASE;
	}

	/* init gem handle */
	for (i = 0; i < MAX_GEM_BUFFER_NUM; i++) {
		evaspixmapsink->gem_info[i].dmabuf_fd = 0;
		evaspixmapsink->gem_info[i].gem_handle = 0;
		evaspixmapsink->gem_info[i].gem_name = 0;
	}

	XCloseDisplay(dpy);
	free(driverName);
	free(deviceName);

	return 0;

ERROR_CASE:
	XCloseDisplay(dpy);
	if (driverName) {
		free(driverName);
	}
	if (deviceName) {
		free(deviceName);
	}

	return -1;
}

static void
drm_fini(GstEvasPixmapSink *evaspixmapsink)
{
	if (evaspixmapsink->drm_fd >= 0) {
		int i = 0;
		for (i = 0; i < MAX_GEM_BUFFER_NUM; i++) {
			if (evaspixmapsink->gem_info[i].dmabuf_fd > 0) {
				GST_INFO_OBJECT (evaspixmapsink,"close gem_handle(%u)", evaspixmapsink->gem_info[i].gem_handle);
				drm_close_gem(evaspixmapsink, evaspixmapsink->gem_info[i].gem_handle);

				evaspixmapsink->gem_info[i].dmabuf_fd = 0;
				evaspixmapsink->gem_info[i].gem_handle = 0;
				evaspixmapsink->gem_info[i].gem_name = 0;
			} else {
				break;
			}
		}
		GST_INFO_OBJECT (evaspixmapsink,"close drm_fd(%d)", evaspixmapsink->drm_fd);
		close(evaspixmapsink->drm_fd);
		evaspixmapsink->drm_fd = -1;
	}
}

static unsigned int
drm_convert_dmabuf_gemname(GstEvasPixmapSink *evaspixmapsink, int dmabuf_fd)
{
	struct drm_prime_handle prime_arg = {0,};
	struct drm_gem_flink flink_arg = {0,};
	int i = 0;

	if (evaspixmapsink->drm_fd < 0) {
		GST_ERROR_OBJECT (evaspixmapsink,"DRM is not opened");
		return 0;
	}

	if (dmabuf_fd <= 0) {
		GST_DEBUG_OBJECT (evaspixmapsink,"Ignore wrong dmabuf fd(%d)", dmabuf_fd);		/* temporarily change log level to DEBUG for reducing WARNING level log */
		return 0;
	}

	/* check duplicated dmabuf fd */
	for (i = 0 ; i < MAX_GEM_BUFFER_NUM ; i++) {
		if (evaspixmapsink->gem_info[i].dmabuf_fd == dmabuf_fd) {
			GST_LOG_OBJECT (evaspixmapsink,"already got fd(%u) with name(%u)", dmabuf_fd, evaspixmapsink->gem_info[i].gem_name);
			return evaspixmapsink->gem_info[i].gem_name;
		}

		if (evaspixmapsink->gem_info[i].dmabuf_fd == 0) {
			GST_LOG_OBJECT (evaspixmapsink,"empty gem_info[%d] found", i);
			break;
		}
	}

	if (i == MAX_GEM_BUFFER_NUM) {
		GST_WARNING_OBJECT (evaspixmapsink,"too many buffers[dmabuf_fd(%d). skip it]", dmabuf_fd);
		return 0;
	}

	evaspixmapsink->gem_info[i].dmabuf_fd = dmabuf_fd;
	prime_arg.fd = dmabuf_fd;
	if (ioctl(evaspixmapsink->drm_fd, DRM_IOCTL_PRIME_FD_TO_HANDLE, &prime_arg)) {
		GST_ERROR_OBJECT (evaspixmapsink,"non dmabuf fd(%d)", dmabuf_fd);
		return 0;
	}

	evaspixmapsink->gem_info[i].gem_handle = prime_arg.handle;
	GST_LOG_OBJECT (evaspixmapsink,"gem_info[%d].gem_handle = %u", i, prime_arg.handle);

	flink_arg.handle = prime_arg.handle;
	if (ioctl(evaspixmapsink->drm_fd, DRM_IOCTL_GEM_FLINK, &flink_arg)) {
		GST_ERROR_OBJECT (evaspixmapsink,"cannot convert drm handle to name");
		return 0;
	}

	evaspixmapsink->gem_info[i].gem_name = flink_arg.name;
	GST_LOG_OBJECT (evaspixmapsink,"gem_info[%d].gem_name = %u", i, flink_arg.name);

	return flink_arg.name;
}

static void
drm_close_gem(GstEvasPixmapSink *evaspixmapsink, unsigned int gem_handle)
{
	struct drm_gem_close close_arg = {0,};

	if (evaspixmapsink->drm_fd < 0) {
		GST_ERROR_OBJECT (evaspixmapsink,"DRM is not opened");
		return;
	}

	if (gem_handle == 0) {
		GST_ERROR_OBJECT (evaspixmapsink,"invalid gem_handle(%d)",gem_handle);
		return;
	}

	close_arg.handle = gem_handle;
	if (gem_handle > 0 && ioctl(evaspixmapsink->drm_fd, DRM_IOCTL_GEM_CLOSE, &close_arg)) {
		GST_ERROR_OBJECT (evaspixmapsink,"cannot close drm gem handle(%d)", gem_handle);
		return;
	}

	return;
}

G_DEFINE_TYPE_EXTENDED(GstEvasPixmapSink, gst_evaspixmapsink, GST_TYPE_VIDEO_SINK, 0,
                       G_IMPLEMENT_INTERFACE(GST_TYPE_NAVIGATION, gst_evaspixmapsink_navigation_init)
                       G_IMPLEMENT_INTERFACE(GST_TYPE_COLOR_BALANCE, gst_evaspixmapsink_colorbalance_init)
                       G_IMPLEMENT_INTERFACE(GST_TYPE_PROPERTY_PROBE, gst_evaspixmapsink_property_probe_interface_init));


/* This function destroys a GstXPixmap */
static void
gst_evaspixmapsink_xpixmap_destroy (GstEvasPixmapSink *evaspixmapsink, GstXPixmap *xpixmap)
{
	g_return_if_fail (xpixmap != NULL);
	g_return_if_fail (GST_IS_EVASPIXMAPSINK (evaspixmapsink));

	g_mutex_lock (evaspixmapsink->x_lock);

	if(evaspixmapsink->xpixmap->pixmap) {
		XFreePixmap(evaspixmapsink->xcontext->disp, evaspixmapsink->xpixmap->pixmap);
		evaspixmapsink->xpixmap->pixmap = NULL;
		GST_DEBUG_OBJECT (evaspixmapsink,"Free pixmap");
	}

	if (xpixmap->gc) {
		XFreeGC (evaspixmapsink->xcontext->disp, xpixmap->gc);
	}

	XSync (evaspixmapsink->xcontext->disp, FALSE);

	g_mutex_unlock (evaspixmapsink->x_lock);

	g_free (xpixmap);
}

static void
gst_evaspixmapsink_xpixmap_update_geometry (GstEvasPixmapSink *evaspixmapsink)
{
#ifdef GST_EXT_XV_ENHANCEMENT
	Window root_window;
	XWindowAttributes root_attr;

	int cur_pixmap_x = 0;
	int cur_pixmap_y = 0;
	unsigned int cur_pixmap_width = 0;
	unsigned int cur_pixmap_height = 0;
	unsigned int cur_pixmap_border_width = 0;
	unsigned int cur_pixmap_depth = 0;
#else
	XWindowAttributes attr;
#endif
	g_return_if_fail (GST_IS_EVASPIXMAPSINK (evaspixmapsink));

	/* Update the window geometry */
	g_mutex_lock (evaspixmapsink->x_lock);
	if (G_UNLIKELY (evaspixmapsink->xpixmap == NULL)) {
		g_mutex_unlock (evaspixmapsink->x_lock);
		return;
	}

#ifdef GST_EXT_XV_ENHANCEMENT
	/* Get root window and size of current pixmap */
	XGetGeometry( evaspixmapsink->xcontext->disp, evaspixmapsink->xpixmap->pixmap, &root_window,
			&cur_pixmap_x, &cur_pixmap_y, /* relative x, y, for pixmap these are alway 0 */
			&cur_pixmap_width, &cur_pixmap_height,
			&cur_pixmap_border_width, &cur_pixmap_depth ); /* cur_pixmap_border_width, cur_pixmap_depth are not used */

	evaspixmapsink->xpixmap->width = cur_pixmap_width;
	evaspixmapsink->xpixmap->height = cur_pixmap_height;

	evaspixmapsink->xpixmap->x = cur_pixmap_x;
	evaspixmapsink->xpixmap->y = cur_pixmap_y;

	/* Get size of root window == size of screen */
	XGetWindowAttributes(evaspixmapsink->xcontext->disp, root_window, &root_attr);

	evaspixmapsink->scr_w = root_attr.width;
	evaspixmapsink->scr_h = root_attr.height;

	if (!evaspixmapsink->have_render_rect) {
		evaspixmapsink->render_rect.x = evaspixmapsink->render_rect.y = 0;
		evaspixmapsink->render_rect.w = cur_pixmap_width;
		evaspixmapsink->render_rect.h = cur_pixmap_height;
	}

	GST_LOG_OBJECT (evaspixmapsink,"screen size %dx%d, current pixmap geometry %d,%d,%dx%d, render_rect %d,%d,%dx%d",
			evaspixmapsink->scr_w, evaspixmapsink->scr_h,
			evaspixmapsink->xpixmap->x, evaspixmapsink->xpixmap->y,
			evaspixmapsink->xpixmap->width, evaspixmapsink->xpixmap->height,
			evaspixmapsink->render_rect.x, evaspixmapsink->render_rect.y,
			evaspixmapsink->render_rect.w, evaspixmapsink->render_rect.h);
#else
	XGetWindowAttributes (evaspixmapsink->xcontext->disp, evaspixmapsink->xpixmap->pixmap, &attr);

	evaspixmapsink->xpixmap->width = attr.width;
	evaspixmapsink->xpixmap->height = attr.height;

	if (!evaspixmapsink->have_render_rect) {
		evaspixmapsink->render_rect.x = evaspixmapsink->render_rect.y = 0;
		evaspixmapsink->render_rect.w = attr.width;
		evaspixmapsink->render_rect.h = attr.height;
	}
#endif
	g_mutex_unlock (evaspixmapsink->x_lock);
}

static void
gst_evaspixmapsink_xpixmap_clear (GstEvasPixmapSink *evaspixmapsink, GstXPixmap *xpixmap)
{
	g_return_if_fail (xpixmap != NULL);
	g_return_if_fail (GST_IS_EVASPIXMAPSINK (evaspixmapsink));

	if (!xpixmap->pixmap) {
		GST_WARNING_OBJECT (evaspixmapsink,"pixmap was not created..");
		return;
	}

	g_mutex_lock (evaspixmapsink->x_lock);

	if( evaspixmapsink->stop_video ) {
		XvStopVideo (evaspixmapsink->xcontext->disp, evaspixmapsink->xcontext->xv_port_id, xpixmap->pixmap);
	}
	/* Preview area is not updated before other UI is updated in the screen. */
	XSetForeground (evaspixmapsink->xcontext->disp, xpixmap->gc, evaspixmapsink->xcontext->black);
	XFillRectangle (evaspixmapsink->xcontext->disp, xpixmap->pixmap, xpixmap->gc,
		evaspixmapsink->render_rect.x, evaspixmapsink->render_rect.y, evaspixmapsink->render_rect.w, evaspixmapsink->render_rect.h);

	XSync (evaspixmapsink->xcontext->disp, FALSE);

	g_mutex_unlock (evaspixmapsink->x_lock);
}

/* This function commits our internal colorbalance settings to our grabbed Xv
   port. If the xcontext is not initialized yet it simply returns */
static void
gst_evaspixmapsink_update_colorbalance (GstEvasPixmapSink *evaspixmapsink)
{
  GList *channels = NULL;

  g_return_if_fail (GST_IS_EVASPIXMAPSINK (evaspixmapsink));

  /* If we haven't initialized the X context we can't update anything */
  if (evaspixmapsink->xcontext == NULL)
    return;

  /* Don't set the attributes if they haven't been changed, to avoid
   * rounding errors changing the values */
  if (!evaspixmapsink->cb_changed)
    return;

  /* For each channel of the colorbalance we calculate the correct value
     doing range conversion and then set the Xv port attribute to match our
     values. */
  channels = evaspixmapsink->xcontext->channels_list;

  while (channels) {
    if (channels->data && GST_IS_COLOR_BALANCE_CHANNEL (channels->data)) {
      GstColorBalanceChannel *channel = NULL;
      Atom prop_atom;
      gint value = 0;
      gdouble convert_coef;

      channel = GST_COLOR_BALANCE_CHANNEL (channels->data);
      g_object_ref (channel);

      /* Our range conversion coef */
      convert_coef = (channel->max_value - channel->min_value) / 2000.0;

      if (g_ascii_strcasecmp (channel->label, "XV_HUE") == 0) {
        value = evaspixmapsink->hue;
      } else if (g_ascii_strcasecmp (channel->label, "XV_SATURATION") == 0) {
        value = evaspixmapsink->saturation;
      } else if (g_ascii_strcasecmp (channel->label, "XV_CONTRAST") == 0) {
        value = evaspixmapsink->contrast;
      } else if (g_ascii_strcasecmp (channel->label, "XV_BRIGHTNESS") == 0) {
        value = evaspixmapsink->brightness;
      } else {
        g_warning ("got an unknown channel %s", channel->label);
        g_object_unref (channel);
        return;
      }

      /* Committing to Xv port */
      g_mutex_lock (evaspixmapsink->x_lock);
      prop_atom =
          XInternAtom (evaspixmapsink->xcontext->disp, channel->label, True);
      if (prop_atom != None) {
        int xv_value;
        xv_value =
            floor (0.5 + (value + 1000) * convert_coef + channel->min_value);
        XvSetPortAttribute (evaspixmapsink->xcontext->disp,
            evaspixmapsink->xcontext->xv_port_id, prop_atom, xv_value);
      }
      g_mutex_unlock (evaspixmapsink->x_lock);

      g_object_unref (channel);
    }
    channels = g_list_next (channels);
  }
}

static void
gst_lookup_xv_port_from_adaptor (GstXContext *xcontext, XvAdaptorInfo *adaptors, int adaptor_no)
{
  gint j;
  gint res;

  /* Do we support XvImageMask ? */
  if (!(adaptors[adaptor_no].type & XvImageMask)) {
    GST_DEBUG ("XV Adaptor %s has no support for XvImageMask", adaptors[adaptor_no].name);
    return;
  }

  /* We found such an adaptor, looking for an available port */
  for (j = 0; j < adaptors[adaptor_no].num_ports && !xcontext->xv_port_id; j++) {
    /* We try to grab the port */
    res = XvGrabPort (xcontext->disp, adaptors[adaptor_no].base_id + j, 0);
    if (Success == res) {
      xcontext->xv_port_id = adaptors[adaptor_no].base_id + j;
      GST_DEBUG ("XV Adaptor %s with %ld ports", adaptors[adaptor_no].name, adaptors[adaptor_no].num_ports);
    } else {
      GST_DEBUG ("GrabPort %d for XV Adaptor %s failed: %d", j, adaptors[adaptor_no].name, res);
    }
  }
}

/* This function generates a caps with all supported format by the first
   Xv grabable port we find. We store each one of the supported formats in a
   format list and append the format to a newly created caps that we return
   If this function does not return NULL because of an error, it also grabs
   the port via XvGrabPort */
static GstCaps*
gst_evaspixmapsink_get_xv_support (GstEvasPixmapSink *evaspixmapsink, GstXContext *xcontext)
{
  gint i;
  XvAdaptorInfo *adaptors;
  gint nb_formats;
  XvImageFormatValues *formats = NULL;
  guint nb_encodings;
  XvEncodingInfo *encodings = NULL;
  gulong max_w = G_MAXINT, max_h = G_MAXINT;
  GstCaps *caps = NULL;
  GstCaps *rgb_caps = NULL;

  g_return_val_if_fail (xcontext != NULL, NULL);

  /* First let's check that XVideo extension is available */
  if (!XQueryExtension (xcontext->disp, "XVideo", &i, &i, &i)) {
    GST_ELEMENT_ERROR (evaspixmapsink, RESOURCE, SETTINGS,
        ("Could not initialise Xv output"),
        ("XVideo extension is not available"));
    return NULL;
  }

  /* Then we get adaptors list */
  if (Success != XvQueryAdaptors (xcontext->disp, xcontext->root,
          &xcontext->nb_adaptors, &adaptors)) {
    GST_ELEMENT_ERROR (evaspixmapsink, RESOURCE, SETTINGS,
        ("Could not initialise Xv output"),
        ("Failed getting XV adaptors list"));
    return NULL;
  }

  xcontext->xv_port_id = 0;

  GST_DEBUG_OBJECT (evaspixmapsink,"Found %u XV adaptor(s)", xcontext->nb_adaptors);

  xcontext->adaptors =
      (gchar **) g_malloc0 (xcontext->nb_adaptors * sizeof (gchar *));

  /* Now fill up our adaptor name array */
  for (i = 0; i < xcontext->nb_adaptors; i++) {
    xcontext->adaptors[i] = g_strdup (adaptors[i].name);
  }

  if (evaspixmapsink->adaptor_no < xcontext->nb_adaptors) {
    /* Find xv port from user defined adaptor */
    gst_lookup_xv_port_from_adaptor (xcontext, adaptors, evaspixmapsink->adaptor_no);
  }

  if (!xcontext->xv_port_id) {
    /* Now search for an adaptor that supports XvImageMask */
    for (i = 0; i < xcontext->nb_adaptors && !xcontext->xv_port_id; i++) {
      gst_lookup_xv_port_from_adaptor (xcontext, adaptors, i);
      evaspixmapsink->adaptor_no = i;
    }
  }

  XvFreeAdaptorInfo (adaptors);

  if (!xcontext->xv_port_id) {
    evaspixmapsink->adaptor_no = -1;
    GST_ELEMENT_ERROR (evaspixmapsink, RESOURCE, BUSY,
        ("Could not initialise Xv output"), ("No port available"));
    return NULL;
  }

  /* Set XV_AUTOPAINT_COLORKEY and XV_DOUBLE_BUFFER and XV_COLORKEY */
  {
    int count, todo = 3;
    XvAttribute *const attr = XvQueryPortAttributes (xcontext->disp,
        xcontext->xv_port_id, &count);
    static const char autopaint[] = "XV_AUTOPAINT_COLORKEY";
    static const char dbl_buffer[] = "XV_DOUBLE_BUFFER";
    static const char colorkey[] = "XV_COLORKEY";

    GST_DEBUG_OBJECT (evaspixmapsink,"Checking %d Xv port attributes", count);

    evaspixmapsink->have_autopaint_colorkey = FALSE;
    evaspixmapsink->have_double_buffer = FALSE;
    evaspixmapsink->have_colorkey = FALSE;

    for (i = 0; ((i < count) && todo); i++)
      if (!strcmp (attr[i].name, autopaint)) {
        const Atom atom = XInternAtom (xcontext->disp, autopaint, False);

        /* turn on autopaint colorkey */
        XvSetPortAttribute (xcontext->disp, xcontext->xv_port_id, atom,
            (evaspixmapsink->autopaint_colorkey ? 1 : 0));
        todo--;
        evaspixmapsink->have_autopaint_colorkey = TRUE;
      } else if (!strcmp (attr[i].name, dbl_buffer)) {
        const Atom atom = XInternAtom (xcontext->disp, dbl_buffer, False);

        XvSetPortAttribute (xcontext->disp, xcontext->xv_port_id, atom,
            (evaspixmapsink->double_buffer ? 1 : 0));
        todo--;
        evaspixmapsink->have_double_buffer = TRUE;
      } else if (!strcmp (attr[i].name, colorkey)) {
        /* Set the colorkey, default is something that is dark but hopefully
         * won't randomly appear on the screen elsewhere (ie not black or greys)
         * can be overridden by setting "colorkey" property
         */
        const Atom atom = XInternAtom (xcontext->disp, colorkey, False);
        guint32 ckey = 0;
        gboolean set_attr = TRUE;
        guint cr, cg, cb;

        /* set a colorkey in the right format RGB565/RGB888
         * We only handle these 2 cases, because they're the only types of
         * devices we've encountered. If we don't recognise it, leave it alone
         */
        cr = (evaspixmapsink->colorkey >> 16);
        cg = (evaspixmapsink->colorkey >> 8) & 0xFF;
        cb = (evaspixmapsink->colorkey) & 0xFF;
        switch (xcontext->depth) {
          case 16:             /* RGB 565 */
            cr >>= 3;
            cg >>= 2;
            cb >>= 3;
            ckey = (cr << 11) | (cg << 5) | cb;
            break;
          case 24:
          case 32:             /* RGB 888 / ARGB 8888 */
            ckey = (cr << 16) | (cg << 8) | cb;
            break;
          default:
            GST_DEBUG_OBJECT (evaspixmapsink,"Unknown bit depth %d for Xv Colorkey - not adjusting", xcontext->depth);
            set_attr = FALSE;
            break;
        }

        if (set_attr) {
          ckey = CLAMP (ckey, (guint32) attr[i].min_value,
              (guint32) attr[i].max_value);
          GST_LOG_OBJECT (evaspixmapsink,"Setting color key for display depth %d to 0x%x", xcontext->depth, ckey);

          XvSetPortAttribute (xcontext->disp, xcontext->xv_port_id, atom,
              (gint) ckey);
        }
        todo--;
        evaspixmapsink->have_colorkey = TRUE;
      }

    XFree (attr);
  }

  /* Get the list of encodings supported by the adapter and look for the
   * XV_IMAGE encoding so we can determine the maximum width and height
   * supported */
  XvQueryEncodings (xcontext->disp, xcontext->xv_port_id, &nb_encodings,
      &encodings);

  for (i = 0; i < nb_encodings; i++) {
    GST_LOG_OBJECT (evaspixmapsink,
        "Encoding %d, name %s, max wxh %lux%lu rate %d/%d",
        i, encodings[i].name, encodings[i].width, encodings[i].height,
        encodings[i].rate.numerator, encodings[i].rate.denominator);
    if (strcmp (encodings[i].name, "XV_IMAGE") == 0) {
      max_w = encodings[i].width;
      max_h = encodings[i].height;
#ifdef GST_EXT_XV_ENHANCEMENT
      evaspixmapsink->scr_w = max_w;
      evaspixmapsink->scr_h = max_h;
#endif
    }
  }

  XvFreeEncodingInfo (encodings);

  /* We get all image formats supported by our port */
  formats = XvListImageFormats (xcontext->disp,
      xcontext->xv_port_id, &nb_formats);
  caps = gst_caps_new_empty ();
  for (i = 0; i < nb_formats; i++) {
    GstCaps *format_caps = NULL;
    gboolean is_rgb_format = FALSE;
    GstVideoFormat vformat;

    /* We set the image format of the xcontext to an existing one. This
       is just some valid image format for making our xshm calls check before
       caps negotiation really happens. */
    xcontext->im_format = formats[i].id;

    switch (formats[i].type) {
      case XvRGB:
      {
        XvImageFormatValues *fmt = &(formats[i]);
        gint endianness = G_BIG_ENDIAN;

        if (fmt->byte_order == LSBFirst) {
          /* our caps system handles 24/32bpp RGB as big-endian. */
          if (fmt->bits_per_pixel == 24 || fmt->bits_per_pixel == 32) {
            fmt->red_mask = GUINT32_TO_BE (fmt->red_mask);
            fmt->green_mask = GUINT32_TO_BE (fmt->green_mask);
            fmt->blue_mask = GUINT32_TO_BE (fmt->blue_mask);

            if (fmt->bits_per_pixel == 24) {
              fmt->red_mask >>= 8;
              fmt->green_mask >>= 8;
              fmt->blue_mask >>= 8;
            }
          } else
            endianness = G_LITTLE_ENDIAN;
        }

        format_caps = gst_caps_new_simple ("video/x-raw",
            "endianness", G_TYPE_INT, endianness,
            "depth", G_TYPE_INT, fmt->depth,
            "bpp", G_TYPE_INT, fmt->bits_per_pixel,
            "red_mask", G_TYPE_INT, fmt->red_mask,
            "green_mask", G_TYPE_INT, fmt->green_mask,
            "blue_mask", G_TYPE_INT, fmt->blue_mask,
            "width", GST_TYPE_INT_RANGE, 1, max_w,
            "height", GST_TYPE_INT_RANGE, 1, max_h,
            "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1, NULL);

        is_rgb_format = TRUE;
        break;
      }
      case XvYUV:
        vformat = gst_video_format_from_fourcc (formats[i].id);
        if (vformat == GST_VIDEO_FORMAT_UNKNOWN)
          break;
        format_caps = gst_caps_new_simple ("video/x-raw",
            "format", G_TYPE_STRING, gst_video_format_to_string (vformat),
            "width", GST_TYPE_INT_RANGE, 1, max_w,
            "height", GST_TYPE_INT_RANGE, 1, max_h,
            "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1, NULL);
        break;
      default:
        g_assert_not_reached ();
        break;
    }

    if (format_caps) {
      GstEvasPixmapFormat *format = NULL;

      format = g_new0 (GstEvasPixmapFormat, 1);
      if (format) {
        format->format = formats[i].id;
        format->caps = gst_caps_copy (format_caps);
        xcontext->formats_list = g_list_append (xcontext->formats_list, format);
      }

      if (is_rgb_format) {
        if (rgb_caps == NULL)
          rgb_caps = format_caps;
        else
          gst_caps_append (rgb_caps, format_caps);
      } else
        gst_caps_append (caps, format_caps);
    }
  }

  /* Collected all caps into either the caps or rgb_caps structures.
   * Append rgb_caps on the end of YUV, so that YUV is always preferred */
  if (rgb_caps)
    gst_caps_append (caps, rgb_caps);

  if (formats)
    XFree (formats);

  GST_DEBUG_OBJECT (evaspixmapsink,"Generated the following caps: %" GST_PTR_FORMAT, caps);

  if (gst_caps_is_empty (caps)) {
    gst_caps_unref (caps);
    XvUngrabPort (xcontext->disp, xcontext->xv_port_id, 0);
    GST_ELEMENT_ERROR (evaspixmapsink, STREAM, WRONG_TYPE, (NULL),
        ("No supported format found"));
    return NULL;
  }

  return caps;
}

static gpointer
gst_evaspixmapsink_event_thread (GstEvasPixmapSink * evaspixmapsink)
{
	g_return_val_if_fail (GST_IS_EVASPIXMAPSINK (evaspixmapsink), NULL);
	int damage_base = 0;
	int damage_err_base = 0;
	int damage_case = 0;
	XEvent e;

	GST_OBJECT_LOCK (evaspixmapsink);

	if (!XDamageQueryExtension(evaspixmapsink->xcontext->disp, &damage_base, &damage_err_base)) {
		GST_ERROR_OBJECT (evaspixmapsink,"XDamageQueryExtension() failed");
		return NULL;
	}
	damage_case = (int)damage_base + XDamageNotify;

	while (evaspixmapsink->running) {
		GST_OBJECT_UNLOCK (evaspixmapsink);
		if (evaspixmapsink->xpixmap) {
			g_mutex_lock (evaspixmapsink->x_lock);
			while (XPending (evaspixmapsink->xcontext->disp)) {
				XNextEvent (evaspixmapsink->xcontext->disp, &e);
				if (e.type == damage_case ) {
					XDamageNotifyEvent *damage_ev = (XDamageNotifyEvent *)&e;
					if (damage_ev->drawable == evaspixmapsink->xpixmap->pixmap) {
						ecore_pipe_write(evaspixmapsink->epipe, evaspixmapsink, sizeof(GstEvasPixmapSink));
						GST_DEBUG_OBJECT (evaspixmapsink,"event_handler : after call ecore_pipe_write()");
					}
					XDamageSubtract (evaspixmapsink->xcontext->disp, evaspixmapsink->damage, None, None );
				}
			}
			g_mutex_unlock (evaspixmapsink->x_lock);
		} else {
			GST_DEBUG_OBJECT (evaspixmapsink,"event_handler : what(%d)? skip..", e.type);
			break;
		}
		g_usleep (G_USEC_PER_SEC / 40);
		GST_OBJECT_LOCK (evaspixmapsink);
	}
	GST_OBJECT_UNLOCK (evaspixmapsink);
	return NULL;
}

static void
gst_evaspixmapsink_manage_event_thread (GstEvasPixmapSink *evaspixmapsink)
{
	GThread *thread = NULL;

	/* don't start the thread too early */
	if (evaspixmapsink->xcontext == NULL) {
		GST_ERROR_OBJECT (evaspixmapsink,"xcontext is NULL..");
		return;
	}

	GST_OBJECT_LOCK (evaspixmapsink);

	if (!evaspixmapsink->event_thread) {
		/* Setup our event listening thread */
		GST_DEBUG_OBJECT (evaspixmapsink,"run xevent thread");
		evaspixmapsink->running = TRUE;
		evaspixmapsink->event_thread = g_thread_create ( (GThreadFunc) gst_evaspixmapsink_event_thread, evaspixmapsink, TRUE, NULL);
	} else {
		GST_WARNING_OBJECT (evaspixmapsink,"there already existed the event_thread.. keep going");
		/* Do not finalize the thread in here, Only finalize the thread by calling gst_evaspixmapsink_reset() */
	}

	GST_OBJECT_UNLOCK (evaspixmapsink);
}


/* This function calculates the pixel aspect ratio based on the properties
 * in the xcontext structure and stores it there. */
static void
gst_evaspixmapsink_calculate_pixel_aspect_ratio (GstXContext *xcontext)
{
  static const gint par[][2] = {
    {1, 1},                     /* regular screen */
    {16, 15},                   /* PAL TV */
    {11, 10},                   /* 525 line Rec.601 video */
    {54, 59},                   /* 625 line Rec.601 video */
    {64, 45},                   /* 1280x1024 on 16:9 display */
    {5, 3},                     /* 1280x1024 on 4:3 display */
    {4, 3}                      /*  800x600 on 16:9 display */
  };
  gint i;
  gint index;
  gdouble ratio;
  gdouble delta;

#define DELTA(idx) (ABS (ratio - ((gdouble) par[idx][0] / par[idx][1])))

  /* first calculate the "real" ratio based on the X values;
   * which is the "physical" w/h divided by the w/h in pixels of the display */
  ratio = (gdouble) (xcontext->widthmm * xcontext->height)
      / (xcontext->heightmm * xcontext->width);

  /* DirectFB's X in 720x576 reports the physical dimensions wrong, so
   * override here */
  if (xcontext->width == 720 && xcontext->height == 576) {
    ratio = 4.0 * 576 / (3.0 * 720);
  }
  GST_DEBUG ("calculated pixel aspect ratio: %f", ratio);
  /* now find the one from par[][2] with the lowest delta to the real one */
  delta = DELTA (0);
  index = 0;

  for (i = 1; i < sizeof (par) / (sizeof (gint) * 2); ++i) {
    gdouble this_delta = DELTA (i);

    if (this_delta < delta) {
      index = i;
      delta = this_delta;
    }
  }

  GST_DEBUG ("Decided on index %d (%d/%d)", index,
      par[index][0], par[index][1]);

  g_free (xcontext->par);
  xcontext->par = g_new0 (GValue, 1);
  g_value_init (xcontext->par, GST_TYPE_FRACTION);
  gst_value_set_fraction (xcontext->par, par[index][0], par[index][1]);
  GST_DEBUG ("set xcontext PAR to %d/%d",
      gst_value_get_fraction_numerator (xcontext->par),
      gst_value_get_fraction_denominator (xcontext->par));
}

/* This function gets the X Display and global info about it. Everything is
   stored in our object and will be cleaned when the object is disposed. Note
   here that caps for supported format are generated without any window or
   image creation */
static GstXContext*
gst_evaspixmapsink_xcontext_get (GstEvasPixmapSink *evaspixmapsink)
{
	GstXContext *xcontext = NULL;
	XPixmapFormatValues *px_formats = NULL;
	gint nb_formats = 0, i, j, N_attr;
	XvAttribute *xv_attr;
	Atom prop_atom;
	const char *channels[4] = { "XV_HUE", "XV_SATURATION", "XV_BRIGHTNESS", "XV_CONTRAST"};

	g_return_val_if_fail (GST_IS_EVASPIXMAPSINK (evaspixmapsink), NULL);

	xcontext = g_new0 (GstXContext, 1);
	xcontext->im_format = 0;

	g_mutex_lock (evaspixmapsink->x_lock);

	xcontext->disp = XOpenDisplay (evaspixmapsink->display_name);

	if (!xcontext->disp) {
		g_mutex_unlock (evaspixmapsink->x_lock);
		g_free (xcontext);
		GST_ELEMENT_ERROR (evaspixmapsink, RESOURCE, WRITE, ("Could not initialise Xv output"), ("Could not open display"));
		return NULL;
	}

	xcontext->screen = DefaultScreenOfDisplay (xcontext->disp);
	xcontext->screen_num = DefaultScreen (xcontext->disp);
	xcontext->visual = DefaultVisual (xcontext->disp, xcontext->screen_num);
	xcontext->root = DefaultRootWindow (xcontext->disp);
	xcontext->white = XWhitePixel (xcontext->disp, xcontext->screen_num);
	xcontext->black = XBlackPixel (xcontext->disp, xcontext->screen_num);
	xcontext->depth = DefaultDepthOfScreen (xcontext->screen);

	xcontext->width = DisplayWidth (xcontext->disp, xcontext->screen_num);
	xcontext->height = DisplayHeight (xcontext->disp, xcontext->screen_num);
	xcontext->widthmm = DisplayWidthMM (xcontext->disp, xcontext->screen_num);
	xcontext->heightmm = DisplayHeightMM (xcontext->disp, xcontext->screen_num);

	GST_DEBUG_OBJECT (evaspixmapsink,"X reports %dx%d pixels and %d mm x %d mm", xcontext->width, xcontext->height, xcontext->widthmm, xcontext->heightmm);

	gst_evaspixmapsink_calculate_pixel_aspect_ratio (xcontext);
	/* We get supported pixmap formats at supported depth */
	px_formats = XListPixmapFormats (xcontext->disp, &nb_formats);

	if (!px_formats) {
		XCloseDisplay (xcontext->disp);
		g_mutex_unlock (evaspixmapsink->x_lock);
		g_free (xcontext->par);
		g_free (xcontext);
		GST_ELEMENT_ERROR (evaspixmapsink, RESOURCE, SETTINGS,
		("Could not initialise Xv output"), ("Could not get pixel formats"));
		return NULL;
	}

	/* We get bpp value corresponding to our running depth */
	for (i = 0; i < nb_formats; i++) {
		if (px_formats[i].depth == xcontext->depth)
		xcontext->bpp = px_formats[i].bits_per_pixel;
	}

	XFree (px_formats);

	xcontext->endianness = (ImageByteOrder (xcontext->disp) == LSBFirst) ? G_LITTLE_ENDIAN : G_BIG_ENDIAN;

	/* our caps system handles 24/32bpp RGB as big-endian. */
	if ((xcontext->bpp == 24 || xcontext->bpp == 32) && xcontext->endianness == G_LITTLE_ENDIAN) {
		xcontext->endianness = G_BIG_ENDIAN;
		xcontext->visual->red_mask = GUINT32_TO_BE (xcontext->visual->red_mask);
		xcontext->visual->green_mask = GUINT32_TO_BE (xcontext->visual->green_mask);
		xcontext->visual->blue_mask = GUINT32_TO_BE (xcontext->visual->blue_mask);
		if (xcontext->bpp == 24) {
			xcontext->visual->red_mask >>= 8;
			xcontext->visual->green_mask >>= 8;
			xcontext->visual->blue_mask >>= 8;
		}
	}

	xcontext->caps = gst_evaspixmapsink_get_xv_support (evaspixmapsink, xcontext);

	if (!xcontext->caps) {
		XCloseDisplay (xcontext->disp);
		g_mutex_unlock (evaspixmapsink->x_lock);
		g_free (xcontext->par);
		g_free (xcontext);
		/* GST_ELEMENT_ERROR is thrown by gst_evaspixmapsink_get_xv_support */
		return NULL;
	}
	#ifdef HAVE_XSHM
	/* Search for XShm extension support */
	if (XShmQueryExtension (xcontext->disp) && gst_evaspixmapsink_check_xshm_calls (xcontext)) {
		xcontext->use_xshm = TRUE;
		GST_DEBUG_OBJECT (evaspixmapsink,"evaspixmapsink is using XShm extension");
	} else
	#endif /* HAVE_XSHM */
	{
		xcontext->use_xshm = FALSE;
		GST_DEBUG_OBJECT (evaspixmapsink,"evaspixmapsink is not using XShm extension");
	}

	xv_attr = XvQueryPortAttributes (xcontext->disp, xcontext->xv_port_id, &N_attr);

	/* Generate the channels list */
	for (i = 0; i < (sizeof (channels) / sizeof (char *)); i++) {
		XvAttribute *matching_attr = NULL;

		/* Retrieve the property atom if it exists. If it doesn't exist,
		* the attribute itself must not either, so we can skip */
		prop_atom = XInternAtom (xcontext->disp, channels[i], True);
		if (prop_atom == None) {
			continue;
		}

		if (xv_attr != NULL) {
			for (j = 0; j < N_attr && matching_attr == NULL; ++j) {
				if (!g_ascii_strcasecmp (channels[i], xv_attr[j].name)) {
					matching_attr = xv_attr + j;
				}
			}
		}

		if (matching_attr) {
			GstColorBalanceChannel *channel;
			channel = g_object_new (GST_TYPE_COLOR_BALANCE_CHANNEL, NULL);
			channel->label = g_strdup (channels[i]);
			channel->min_value = matching_attr->min_value;
			channel->max_value = matching_attr->max_value;

			xcontext->channels_list = g_list_append (xcontext->channels_list, channel);

			/* If the colorbalance settings have not been touched we get Xv values
				as defaults and update our internal variables */
			if (!evaspixmapsink->cb_changed) {
				gint val;
				XvGetPortAttribute (xcontext->disp, xcontext->xv_port_id, prop_atom, &val);
				/* Normalize val to [-1000, 1000] */
				val = floor (0.5 + -1000 + 2000 * (val - channel->min_value) /	(double) (channel->max_value - channel->min_value));

				if (!g_ascii_strcasecmp (channels[i], "XV_HUE")) {
					evaspixmapsink->hue = val;
				} else if (!g_ascii_strcasecmp (channels[i], "XV_SATURATION")) {
					evaspixmapsink->saturation = val;
				} else if (!g_ascii_strcasecmp (channels[i], "XV_BRIGHTNESS")) {
					evaspixmapsink->brightness = val;
				} else if (!g_ascii_strcasecmp (channels[i], "XV_CONTRAST")) {
					evaspixmapsink->contrast = val;
				}
			}
		}
	}

	if (xv_attr) {
		XFree (xv_attr);
	}

	g_mutex_unlock (evaspixmapsink->x_lock);

	return xcontext;
}

/* This function cleans the X context. Closing the Display, releasing the XV
   port and unrefing the caps for supported formats. */
static void
gst_evaspixmapsink_xcontext_clear (GstEvasPixmapSink *evaspixmapsink)
{
  GList *formats_list, *channels_list;
  GstXContext *xcontext;
  gint i = 0;

  g_return_if_fail (GST_IS_EVASPIXMAPSINK (evaspixmapsink));

  GST_OBJECT_LOCK (evaspixmapsink);
  if (evaspixmapsink->xcontext == NULL) {
    GST_OBJECT_UNLOCK (evaspixmapsink);
    return;
  }

  /* Take the XContext from the sink and clean it up */
  xcontext = evaspixmapsink->xcontext;
  evaspixmapsink->xcontext = NULL;

  GST_OBJECT_UNLOCK (evaspixmapsink);

  formats_list = xcontext->formats_list;

  while (formats_list) {
    GstEvasPixmapFormat *format = formats_list->data;

    gst_caps_unref (format->caps);
    g_free (format);
    formats_list = g_list_next (formats_list);
  }

  if (xcontext->formats_list)
    g_list_free (xcontext->formats_list);

  channels_list = xcontext->channels_list;

  while (channels_list) {
    GstColorBalanceChannel *channel = channels_list->data;

    g_object_unref (channel);
    channels_list = g_list_next (channels_list);
  }

  if (xcontext->channels_list)
    g_list_free (xcontext->channels_list);

  gst_caps_unref (xcontext->caps);

  for (i = 0; i < xcontext->nb_adaptors; i++) {
    g_free (xcontext->adaptors[i]);
  }

  g_free (xcontext->adaptors);

  g_free (xcontext->par);

  g_mutex_lock (evaspixmapsink->x_lock);

  GST_DEBUG_OBJECT (evaspixmapsink,"Closing display and freeing X Context");

  XvUngrabPort (xcontext->disp, xcontext->xv_port_id, 0);

  XCloseDisplay (xcontext->disp);

  g_mutex_unlock (evaspixmapsink->x_lock);

  g_free (xcontext);
}

/* Element stuff */

/* This function tries to get a format matching with a given caps in the
   supported list of formats we generated in gst_evaspixmapsink_get_xv_support */
static gint
gst_evaspixmapsink_get_format_from_caps (GstEvasPixmapSink *evaspixmapsink, GstCaps *caps)
{
  GList *list = NULL;

  g_return_val_if_fail (GST_IS_EVASPIXMAPSINK (evaspixmapsink), 0);

  list = evaspixmapsink->xcontext->formats_list;

  while (list) {
    GstEvasPixmapFormat *format = list->data;

    if (format) {
      if (gst_caps_can_intersect (caps, format->caps)) {
        return format->format;
      }
    }
    list = g_list_next (list);
  }

  return -1;
}

static GstCaps *
gst_evaspixmapsink_getcaps (GstBaseSink *bsink, GstCaps *filter)
{
  GstEvasPixmapSink *evaspixmapsink;
  GstCaps* pad_caps;

  evaspixmapsink = GST_EVASPIXMAPSINK (bsink);

  if(filter)
    GST_LOG_OBJECT (evaspixmapsink,"filter caps %" GST_PTR_FORMAT, filter);
  if(evaspixmapsink->xcontext)
    GST_LOG_OBJECT (evaspixmapsink,"context caps %" GST_PTR_FORMAT, evaspixmapsink->xcontext->caps);

  if (evaspixmapsink->xcontext)
    return filter ? gst_caps_intersect(filter, evaspixmapsink->xcontext->caps)
            : gst_caps_ref (evaspixmapsink->xcontext->caps);

  pad_caps = gst_pad_get_pad_template_caps (GST_VIDEO_SINK_PAD(evaspixmapsink));
  return
      filter ? gst_caps_intersect(filter, pad_caps) : gst_caps_copy (pad_caps);
}

static gboolean
gst_evaspixmapsink_setcaps (GstBaseSink *bsink, GstCaps *caps)
{
	GstEvasPixmapSink *evaspixmapsink;
	GstStructure *structure;
	guint32 im_format = 0;
	gboolean ret;
	gint video_width, video_height;
	gint disp_x, disp_y;
	gint disp_width, disp_height;
	gint video_par_n, video_par_d;        /* video's PAR */
	gint display_par_n, display_par_d;    /* display's PAR */
	const GValue *caps_par;
	const GValue *caps_disp_reg;
	const GValue *fps;
	guint num, den;
#ifdef GST_EXT_XV_ENHANCEMENT
	gboolean enable_last_buffer;
#endif

	evaspixmapsink = GST_EVASPIXMAPSINK (bsink);

	GST_DEBUG_OBJECT (evaspixmapsink,"In setcaps. Possible caps %" GST_PTR_FORMAT ", setting caps %" GST_PTR_FORMAT, evaspixmapsink->xcontext->caps, caps);

	if (!gst_caps_can_intersect (evaspixmapsink->xcontext->caps, caps)) {
		goto incompatible_caps;
	}

	evaspixmapsink->negotiated_caps = gst_caps_copy(caps);
	structure = gst_caps_get_structure (caps, 0);
	ret = gst_structure_get_int (structure, "width", &video_width);
	ret &= gst_structure_get_int (structure, "height", &video_height);
	fps = gst_structure_get_value (structure, "framerate");
	ret &= (fps != NULL);

	if (!ret) {
		goto incomplete_caps;
	}

#ifdef GST_EXT_XV_ENHANCEMENT
	evaspixmapsink->aligned_width = video_width;
	evaspixmapsink->aligned_height = video_height;

	/* get enable-last-buffer */
	g_object_get(G_OBJECT(evaspixmapsink), "enable-last-buffer", &enable_last_buffer, NULL);
	GST_INFO_OBJECT (evaspixmapsink,"current enable-last-buffer : %d", enable_last_buffer);
	/* flush if enable-last-buffer is TRUE */
	if (enable_last_buffer) {
		GST_INFO_OBJECT (evaspixmapsink,"flush last-buffer");
		g_object_set(G_OBJECT(evaspixmapsink), "enable-last-buffer", FALSE, NULL);
		g_object_set(G_OBJECT(evaspixmapsink), "enable-last-buffer", TRUE, NULL);
	}
#endif

	evaspixmapsink->fps_n = gst_value_get_fraction_numerator (fps);
	evaspixmapsink->fps_d = gst_value_get_fraction_denominator (fps);

	evaspixmapsink->video_width = video_width;
	evaspixmapsink->video_height = video_height;

	im_format = gst_evaspixmapsink_get_format_from_caps (evaspixmapsink, caps);
	if (im_format == -1) {
		goto invalid_format;
	}

	/* get aspect ratio from caps if it's present, and
	* convert video width and height to a display width and height
	* using wd / hd = wv / hv * PARv / PARd */

	/* get video's PAR */
	caps_par = gst_structure_get_value (structure, "pixel-aspect-ratio");
	if (caps_par) {
		video_par_n = gst_value_get_fraction_numerator (caps_par);
		video_par_d = gst_value_get_fraction_denominator (caps_par);
	} else {
		video_par_n = 1;
		video_par_d = 1;
	}
	/* get display's PAR */
	if (evaspixmapsink->par) {
		display_par_n = gst_value_get_fraction_numerator (evaspixmapsink->par);
		display_par_d = gst_value_get_fraction_denominator (evaspixmapsink->par);
	} else {
		display_par_n = 1;
		display_par_d = 1;
	}

	/* get the display region */
	caps_disp_reg = gst_structure_get_value (structure, "display-region");
	if (caps_disp_reg) {
		disp_x = g_value_get_int (gst_value_array_get_value (caps_disp_reg, 0));
		disp_y = g_value_get_int (gst_value_array_get_value (caps_disp_reg, 1));
		disp_width = g_value_get_int (gst_value_array_get_value (caps_disp_reg, 2));
		disp_height = g_value_get_int (gst_value_array_get_value (caps_disp_reg, 3));
	} else {
		disp_x = disp_y = 0;
		disp_width = video_width;
		disp_height = video_height;
	}

	if (!gst_video_calculate_display_ratio (&num, &den, video_width, video_height, video_par_n, video_par_d, display_par_n, display_par_d)) {
		goto no_disp_ratio;
	}

	evaspixmapsink->disp_x = disp_x;
	evaspixmapsink->disp_y = disp_y;
	evaspixmapsink->disp_width = disp_width;
	evaspixmapsink->disp_height = disp_height;

	GST_DEBUG_OBJECT (evaspixmapsink,"video width/height: %dx%d, calculated display ratio: %d/%d",	video_width, video_height, num, den);

	/* now find a width x height that respects this display ratio.
	* prefer those that have one of w/h the same as the incoming video
	* using wd / hd = num / den */

	/* start with same height, because of interlaced video */
	/* check hd / den is an integer scale factor, and scale wd with the PAR */
	if (video_height % den == 0) {
		GST_DEBUG_OBJECT (evaspixmapsink,"keeping video height");
		GST_VIDEO_SINK_WIDTH (evaspixmapsink) = (guint) gst_util_uint64_scale_int (video_height, num, den);
		GST_VIDEO_SINK_HEIGHT (evaspixmapsink) = video_height;
	} else if (video_width % num == 0) {
		GST_DEBUG_OBJECT (evaspixmapsink,"keeping video width");
		GST_VIDEO_SINK_WIDTH (evaspixmapsink) = video_width;
		GST_VIDEO_SINK_HEIGHT (evaspixmapsink) = (guint) gst_util_uint64_scale_int (video_width, den, num);
	} else {
		GST_DEBUG_OBJECT (evaspixmapsink,"approximating while keeping video height");
		GST_VIDEO_SINK_WIDTH (evaspixmapsink) = (guint) gst_util_uint64_scale_int (video_height, num, den);
		GST_VIDEO_SINK_HEIGHT (evaspixmapsink) = video_height;
	}
	GST_DEBUG_OBJECT (evaspixmapsink,"scaling to %dx%d", GST_VIDEO_SINK_WIDTH (evaspixmapsink), GST_VIDEO_SINK_HEIGHT (evaspixmapsink));

	/* Creating our window and our image with the display size in pixels */
	if (GST_VIDEO_SINK_WIDTH (evaspixmapsink) <= 0 || GST_VIDEO_SINK_HEIGHT (evaspixmapsink) <= 0) {
		goto no_display_size;
	}

	g_mutex_lock (evaspixmapsink->flow_lock);

	/* We renew our evaspixmap buffer only if size or format changed;
	* the evaspixmap buffer is the same size as the video pixel size */
	if ((evaspixmapsink->evas_pixmap_buf) && ((im_format != evaspixmapsink->evas_pixmap_buf->im_format)
		|| (video_width != evaspixmapsink->evas_pixmap_buf->width) || (video_height != evaspixmapsink->evas_pixmap_buf->height))) {
		GST_DEBUG_OBJECT (evaspixmapsink,"old format %" GST_FOURCC_FORMAT ", new format %" GST_FOURCC_FORMAT,
				GST_FOURCC_ARGS (evaspixmapsink->evas_pixmap_buf->im_format), GST_FOURCC_ARGS (im_format));
		GST_DEBUG_OBJECT (evaspixmapsink,"renewing evaspixmap buffer");
		g_boxed_free(GST_TYPE_EVASPIXMAP_BUFFER, evaspixmapsink->evas_pixmap_buf);
		evaspixmapsink->evas_pixmap_buf = NULL;
	}

	g_mutex_unlock (evaspixmapsink->flow_lock);

	if (evaspixmapsink->eo) {
		if (!gst_evaspixmapsink_xpixmap_link (evaspixmapsink)) {
			GST_ERROR_OBJECT (evaspixmapsink,"link evas image object with pixmap failed...");
			return FALSE;
		} else {
			gst_evaspixmapsink_manage_event_thread (evaspixmapsink);
		}
	} else {
		GST_ERROR_OBJECT (evaspixmapsink,"setcaps success, but there is no evas image object..");
		return FALSE;
	}

	return TRUE;

	/* ERRORS */
	incompatible_caps:
	{
		GST_ERROR_OBJECT (evaspixmapsink,"caps incompatible");
		return FALSE;
	}
	incomplete_caps:
	{
		GST_DEBUG_OBJECT (evaspixmapsink,"Failed to retrieve either width, ""height or framerate from intersected caps");
		return FALSE;
	}
	invalid_format:
	{
		GST_DEBUG_OBJECT (evaspixmapsink,"Could not locate image format from caps %" GST_PTR_FORMAT, caps);
		return FALSE;
	}
	no_disp_ratio:
	{
		GST_ELEMENT_ERROR (evaspixmapsink, CORE, NEGOTIATION, (NULL), ("Error calculating the output display ratio of the video."));
		return FALSE;
	}
	no_display_size:
	{
		GST_ELEMENT_ERROR (evaspixmapsink, CORE, NEGOTIATION, (NULL), ("Error calculating the output display ratio of the video."));
		return FALSE;
	}
}

static GstStateChangeReturn
gst_evaspixmapsink_change_state (GstElement *element, GstStateChange transition)
{
	GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
	GstEvasPixmapSink *evaspixmapsink;

	evaspixmapsink = GST_EVASPIXMAPSINK (element);

	switch (transition) {
	case GST_STATE_CHANGE_NULL_TO_READY:
		GST_DEBUG_OBJECT (evaspixmapsink,"GST_STATE_CHANGE_NULL_TO_READY");

		/* open drm to use gem */
		if (drm_init(evaspixmapsink)) {
			GST_ERROR_OBJECT (evaspixmapsink,"drm_init() failure");
			return GST_STATE_CHANGE_FAILURE;
		}

		/* check if there exist evas image object, need to write code related to making internal evas image object */
		if (!is_evas_image_object (evaspixmapsink->eo)) {
			GST_ERROR_OBJECT (evaspixmapsink,"There is no evas image object..");
			return GST_STATE_CHANGE_FAILURE;
		}

		/* Set xcontext and display */
		if (!evaspixmapsink->xcontext) {
			evaspixmapsink->xcontext = gst_evaspixmapsink_xcontext_get (evaspixmapsink);
			if (!evaspixmapsink->xcontext) {
				GST_ERROR_OBJECT (evaspixmapsink,"could not get xcontext..");
				return GST_STATE_CHANGE_FAILURE;
			}
		}

		/* update object's par with calculated one if not set yet */
		if (!evaspixmapsink->par) {
			evaspixmapsink->par = g_new0 (GValue, 1);
			gst_value_init_and_copy (evaspixmapsink->par, evaspixmapsink->xcontext->par);
			GST_DEBUG_OBJECT (evaspixmapsink,"set calculated PAR on object's PAR");
		}

		/* call XSynchronize with the current value of synchronous */
		GST_DEBUG_OBJECT (evaspixmapsink,"XSynchronize called with %s", evaspixmapsink->synchronous ? "TRUE" : "FALSE");
		XSynchronize (evaspixmapsink->xcontext->disp, evaspixmapsink->synchronous);
		gst_evaspixmapsink_update_colorbalance (evaspixmapsink);
		break;

	case GST_STATE_CHANGE_READY_TO_PAUSED:
		GST_DEBUG_OBJECT (evaspixmapsink,"GST_STATE_CHANGE_READY_TO_PAUSED");
		break;

	case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
		GST_DEBUG_OBJECT (evaspixmapsink,"GST_STATE_CHANGE_PAUSED_TO_PLAYING");
		break;

	default:
		break;
	}

	ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

	switch (transition) {
	case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
		GST_DEBUG_OBJECT (evaspixmapsink,"GST_STATE_CHANGE_PLAYING_TO_PAUSED");
		break;

	case GST_STATE_CHANGE_PAUSED_TO_READY:
		GST_DEBUG_OBJECT (evaspixmapsink,"GST_STATE_CHANGE_PAUSED_TO_READY");
		evaspixmapsink->fps_n = 0;
		evaspixmapsink->fps_d = 1;
		GST_VIDEO_SINK_WIDTH (evaspixmapsink) = 0;
		GST_VIDEO_SINK_HEIGHT (evaspixmapsink) = 0;
		break;

	case GST_STATE_CHANGE_READY_TO_NULL:
		GST_DEBUG_OBJECT (evaspixmapsink,"GST_STATE_CHANGE_READY_TO_NULL");
		gst_evaspixmapsink_reset (evaspixmapsink);
		/* close drm */
		drm_fini(evaspixmapsink);
		break;

	default:
		break;
	}
	return ret;
}

static void
gst_evaspixmapsink_get_times (GstBaseSink *bsink, GstBuffer *buf, GstClockTime *start, GstClockTime *end)
{
  GstEvasPixmapSink *evaspixmapsink;

  evaspixmapsink = GST_EVASPIXMAPSINK (bsink);

  if (GST_BUFFER_TIMESTAMP_IS_VALID (buf)) {
    *start = GST_BUFFER_TIMESTAMP (buf);
    if (GST_BUFFER_DURATION_IS_VALID (buf)) {
      *end = *start + GST_BUFFER_DURATION (buf);
    } else {
      if (evaspixmapsink->fps_n > 0) {
        *end = *start +
            gst_util_uint64_scale_int (GST_SECOND, evaspixmapsink->fps_d,
            evaspixmapsink->fps_n);
      }
    }
  }
}

static GstFlowReturn
gst_evaspixmapsink_show_frame (GstVideoSink *vsink, GstBuffer *buf)
{
	GstMapInfo buf_info = GST_MAP_INFO_INIT;
	GstEvasPixmapSink *evaspixmapsink;
#ifdef GST_EXT_XV_ENHANCEMENT
	XV_PUTIMAGE_DATA_PTR img_data = NULL;
	SCMN_IMGB *scmn_imgb = NULL;
	gint format = 0;
#endif
	evaspixmapsink = GST_EVASPIXMAPSINK (vsink);

#ifdef GST_EXT_XV_ENHANCEMENT
	if( evaspixmapsink->stop_video ) {
		GST_INFO_OBJECT (evaspixmapsink, "Stop video is TRUE. so skip show frame..." );
		return GST_FLOW_OK;
	}
#endif

	if (!evaspixmapsink->evas_pixmap_buf) {
		GST_DEBUG_OBJECT (evaspixmapsink,"creating our evaspixmap buffer");
#ifdef GST_EXT_XV_ENHANCEMENT
		format = gst_evaspixmapsink_get_format_from_caps(evaspixmapsink, evaspixmapsink->negotiated_caps);

		switch (format) {
		case GST_MAKE_FOURCC('S', 'T', '1', '2'):
		case GST_MAKE_FOURCC('S', 'N', '1', '2'):
		case GST_MAKE_FOURCC('S', '4', '2', '0'):
		case GST_MAKE_FOURCC('S', 'U', 'Y', '2'):
		case GST_MAKE_FOURCC('S', 'U', 'Y', 'V'):
		case GST_MAKE_FOURCC('S', 'Y', 'V', 'Y'):
            gst_buffer_map(buf, &buf_info, GST_MAP_READ);
			scmn_imgb = (SCMN_IMGB *)buf_info.data;
			gst_buffer_unmap(buf, &buf_info);
			if(scmn_imgb == NULL) {
				GST_DEBUG_OBJECT (evaspixmapsink, "scmn_imgb is NULL. Skip buffer put..." );
				return GST_FLOW_OK;
			}
			 /* skip buffer if aligned size is smaller than size of caps */
			if (scmn_imgb->s[0] < evaspixmapsink->video_width || scmn_imgb->e[0] < evaspixmapsink->video_height) {
				GST_WARNING_OBJECT (evaspixmapsink,"invalid size[caps:%dx%d,aligned:%dx%d]. Skip this buffer...",
						evaspixmapsink->video_width, evaspixmapsink->video_height, scmn_imgb->s[0], scmn_imgb->e[0]);
				return GST_FLOW_OK;
			}
			evaspixmapsink->aligned_width = scmn_imgb->s[0];
			evaspixmapsink->aligned_height = scmn_imgb->e[0];
			GST_DEBUG_OBJECT (evaspixmapsink,"video width,height[%dx%d]",evaspixmapsink->video_width, evaspixmapsink->video_height);
			GST_INFO_OBJECT (evaspixmapsink,"Use aligned width,height[%dx%d]",evaspixmapsink->aligned_width, evaspixmapsink->aligned_height);
			break;
		default:
			GST_INFO_OBJECT (evaspixmapsink,"Use original width,height of caps");
			break;
		}
#endif
		evaspixmapsink->evas_pixmap_buf = gst_evaspixmap_buffer_new (evaspixmapsink, evaspixmapsink->negotiated_caps);

		if (!evaspixmapsink->evas_pixmap_buf) {
			/* The create method should have posted an informative error */
			goto no_image;
		}
		if (evaspixmapsink->evas_pixmap_buf->size < gst_buffer_get_size (buf)) {
			GST_ELEMENT_ERROR (evaspixmapsink, RESOURCE, WRITE, ("Failed to create output image buffer of %dx%d pixels",	evaspixmapsink->evas_pixmap_buf->width, evaspixmapsink->evas_pixmap_buf->height),("XServer allocated buffer size did not match input buffer"));
			gst_evaspixmap_buffer_destroy (evaspixmapsink->evas_pixmap_buf);
			evaspixmapsink->evas_pixmap_buf = NULL;
			goto no_image;
		}
	}

#ifdef GST_EXT_XV_ENHANCEMENT
	switch (evaspixmapsink->evas_pixmap_buf->im_format) {
	/* Cases for specified formats of Samsung extension */
	case GST_MAKE_FOURCC('S', 'T', '1', '2'):
	case GST_MAKE_FOURCC('S', 'N', '1', '2'):
	case GST_MAKE_FOURCC('S', '4', '2', '0'):
	case GST_MAKE_FOURCC('S', 'U', 'Y', '2'):
	case GST_MAKE_FOURCC('S', 'U', 'Y', 'V'):
	case GST_MAKE_FOURCC('S', 'Y', 'V', 'Y'):
	case GST_MAKE_FOURCC('I', 'T', 'L', 'V'):
	{
		GST_DEBUG_OBJECT (evaspixmapsink, "Samsung extension display format activated. fourcc:%c%c%c%c",
			evaspixmapsink->evas_pixmap_buf->im_format, evaspixmapsink->evas_pixmap_buf->im_format>>8,
                evaspixmapsink->evas_pixmap_buf->im_format>>16, evaspixmapsink->evas_pixmap_buf->im_format>>24);
		GstMapInfo buf_info = GST_MAP_INFO_INIT;

		if (evaspixmapsink->evas_pixmap_buf->xvimage->data) {
			img_data = (XV_PUTIMAGE_DATA_PTR) evaspixmapsink->evas_pixmap_buf->xvimage->data;
			XV_PUTIMAGE_INIT_DATA(img_data);
		    gst_buffer_map(buf, &buf_info, GST_MAP_READ);
			scmn_imgb = (SCMN_IMGB *)buf_info.data;
			gst_buffer_unmap(buf, &buf_info);
			if (scmn_imgb == NULL) {
				GST_DEBUG_OBJECT (evaspixmapsink, "scmn_imgb is NULL. Skip buffer put..." );
				return GST_FLOW_OK;
			}

			if (scmn_imgb->buf_share_method == BUF_SHARE_METHOD_PADDR) {
				img_data->YBuf = (unsigned int)scmn_imgb->p[0];
				img_data->CbBuf = (unsigned int)scmn_imgb->p[1];
				img_data->CrBuf = (unsigned int)scmn_imgb->p[2];
				img_data->BufType = XV_BUF_TYPE_LEGACY;
				if (!img_data->YBuf) {
					GST_WARNING_OBJECT (evaspixmapsink, "img_data->YBuf is NULL. skip buffer put..." );
					return GST_FLOW_OK;
				}
			} else { /* BUF_SHARE_METHOD_FD */
				/* convert dma-buf fd into drm gem name */
				img_data->YBuf = drm_convert_dmabuf_gemname(evaspixmapsink, (int)scmn_imgb->dma_buf_fd[0]);
				img_data->CbBuf = drm_convert_dmabuf_gemname(evaspixmapsink, (int)scmn_imgb->dma_buf_fd[1]);
				img_data->CrBuf = drm_convert_dmabuf_gemname(evaspixmapsink, (int)scmn_imgb->dma_buf_fd[2]);
				img_data->BufType = XV_BUF_TYPE_DMABUF;
				if (!img_data->YBuf) {
					GST_WARNING_OBJECT (evaspixmapsink, "img_data->YBuf is NULL. skip buffer put..." );
					return GST_FLOW_OK;
				}
			}
			GST_LOG_OBJECT(evaspixmapsink, "YBuf[%d], CbBuf[%d], CrBuf[%d]",
					img_data->YBuf, img_data->CbBuf, img_data->CrBuf );
		} else {
			GST_WARNING_OBJECT (evaspixmapsink, "xvimage->data is NULL. skip buffer put..." );
			return GST_FLOW_OK;
		}
		break;
	}
	default:
	{
		GST_DEBUG_OBJECT (evaspixmapsink, "Normal format activated. fourcc:%c%c%c%c",
			evaspixmapsink->evas_pixmap_buf->im_format, evaspixmapsink->evas_pixmap_buf->im_format>>8,
                evaspixmapsink->evas_pixmap_buf->im_format>>16, evaspixmapsink->evas_pixmap_buf->im_format>>24);
		gst_buffer_map(buf, &buf_info, GST_MAP_READ);
		memcpy (evaspixmapsink->evas_pixmap_buf->xvimage->data, buf_info.data,
		MIN (buf_info.size, evaspixmapsink->evas_pixmap_buf->size));
		gst_buffer_unmap(buf, &buf_info);
		break;
	}
	}
#else
	gst_buffer_map(buf, &buf_info, GST_MAP_READ);
	memcpy (evaspixmapsink->evas_pixmap_buf->xvimage->data, buf_info.data, MIN (buf_info.size, evaspixmapsink->evas_pixmap_buf->size));
	gst_buffer_unmap(buf, &buf_info);
#endif
	if (!gst_evaspixmap_buffer_put (evaspixmapsink, evaspixmapsink->evas_pixmap_buf)) {
		goto no_pixmap;
	}

	return GST_FLOW_OK;

	/* ERRORS */
	no_image:
	{
		/* No image available. That's very bad ! */
		GST_WARNING_OBJECT (evaspixmapsink,"could not create image");
		return GST_FLOW_ERROR;
	}
	no_pixmap:
	{
		/* No Pixmap available to put our image into */
		GST_WARNING_OBJECT (evaspixmapsink,"could not output image - no pixmap");
		return GST_FLOW_ERROR;
	}
}

static gboolean
gst_evaspixmapsink_event (GstBaseSink *sink, GstEvent *event)
{
	GstEvasPixmapSink *evaspixmapsink = GST_EVASPIXMAPSINK (sink);

	switch (GST_EVENT_TYPE (event)) {
	case GST_EVENT_FLUSH_START:
		GST_DEBUG_OBJECT (evaspixmapsink,"GST_EVENT_FLUSH_START");
		break;
	case GST_EVENT_FLUSH_STOP:
		GST_DEBUG_OBJECT (evaspixmapsink,"GST_EVENT_FLUSH_STOP");
		break;
	default:
		break;
	}
	if (GST_BASE_SINK_CLASS (parent_class)->event) {
		return GST_BASE_SINK_CLASS (parent_class)->event (sink, event);
	} else {
		return TRUE;
	}
}

/* Interfaces stuff */

static void
gst_evaspixmapsink_navigation_send_event (GstNavigation *navigation, GstStructure *structure)
{
  GstEvasPixmapSink *evaspixmapsink = GST_EVASPIXMAPSINK (navigation);
  GstPad *peer;

  if ((peer = gst_pad_get_peer (GST_VIDEO_SINK_PAD (evaspixmapsink)))) {
    GstEvent *event;
    GstVideoRectangle src, dst, result;
    gdouble x, y, xscale = 1.0, yscale = 1.0;

    event = gst_event_new_navigation (structure);

    /* We take the flow_lock while we look at the window */
    g_mutex_lock (evaspixmapsink->flow_lock);

    if (!evaspixmapsink->xpixmap) {
      g_mutex_unlock (evaspixmapsink->flow_lock);
      return;
    }

    if (evaspixmapsink->keep_aspect) {
      /* We get the frame position using the calculated geometry from _setcaps
         that respect pixel aspect ratios */
      src.w = GST_VIDEO_SINK_WIDTH (evaspixmapsink);
      src.h = GST_VIDEO_SINK_HEIGHT (evaspixmapsink);
      dst.w = evaspixmapsink->render_rect.w;
      dst.h = evaspixmapsink->render_rect.h;

      gst_video_sink_center_rect (src, dst, &result, TRUE);
      result.x += evaspixmapsink->render_rect.x;
      result.y += evaspixmapsink->render_rect.y;
    } else {
      memcpy (&result, &evaspixmapsink->render_rect, sizeof (GstVideoRectangle));
    }

    g_mutex_unlock (evaspixmapsink->flow_lock);

    /* We calculate scaling using the original video frames geometry to include
       pixel aspect ratio scaling. */
    xscale = (gdouble) evaspixmapsink->video_width / result.w;
    yscale = (gdouble) evaspixmapsink->video_height / result.h;

    /* Converting pointer coordinates to the non scaled geometry */
    if (gst_structure_get_double (structure, "pointer_x", &x)) {
      x = MIN (x, result.x + result.w);
      x = MAX (x - result.x, 0);
      gst_structure_set (structure, "pointer_x", G_TYPE_DOUBLE,
          (gdouble) x * xscale, NULL);
    }
    if (gst_structure_get_double (structure, "pointer_y", &y)) {
      y = MIN (y, result.y + result.h);
      y = MAX (y - result.y, 0);
      gst_structure_set (structure, "pointer_y", G_TYPE_DOUBLE,
          (gdouble) y * yscale, NULL);
    }

    gst_pad_send_event (peer, event);
    gst_object_unref (peer);
  }
}

static void
gst_evaspixmapsink_navigation_init (GstNavigationInterface *iface)
{
  iface->send_event = gst_evaspixmapsink_navigation_send_event;
}

static const GList*
gst_evaspixmapsink_colorbalance_list_channels (GstColorBalance *balance)
{
  GstEvasPixmapSink *evaspixmapsink = GST_EVASPIXMAPSINK (balance);

  g_return_val_if_fail (GST_IS_EVASPIXMAPSINK (evaspixmapsink), NULL);

  if (evaspixmapsink->xcontext)
    return evaspixmapsink->xcontext->channels_list;
  else
    return NULL;
}

static void
gst_evaspixmapsink_colorbalance_set_value (GstColorBalance *balance, GstColorBalanceChannel *channel, gint value)
{
  GstEvasPixmapSink *evaspixmapsink = GST_EVASPIXMAPSINK (balance);

  g_return_if_fail (GST_IS_EVASPIXMAPSINK (evaspixmapsink));
  g_return_if_fail (channel->label != NULL);

  evaspixmapsink->cb_changed = TRUE;

  /* Normalize val to [-1000, 1000] */
  value = floor (0.5 + -1000 + 2000 * (value - channel->min_value) /
      (double) (channel->max_value - channel->min_value));

  if (g_ascii_strcasecmp (channel->label, "XV_HUE") == 0) {
    evaspixmapsink->hue = value;
  } else if (g_ascii_strcasecmp (channel->label, "XV_SATURATION") == 0) {
    evaspixmapsink->saturation = value;
  } else if (g_ascii_strcasecmp (channel->label, "XV_CONTRAST") == 0) {
    evaspixmapsink->contrast = value;
  } else if (g_ascii_strcasecmp (channel->label, "XV_BRIGHTNESS") == 0) {
    evaspixmapsink->brightness = value;
  } else {
    g_warning ("got an unknown channel %s", channel->label);
    return;
  }

  gst_evaspixmapsink_update_colorbalance (evaspixmapsink);
}

static gint
gst_evaspixmapsink_colorbalance_get_value (GstColorBalance *balance, GstColorBalanceChannel *channel)
{
  GstEvasPixmapSink *evaspixmapsink = GST_EVASPIXMAPSINK (balance);
  gint value = 0;

  g_return_val_if_fail (GST_IS_EVASPIXMAPSINK (evaspixmapsink), 0);
  g_return_val_if_fail (channel->label != NULL, 0);

  if (g_ascii_strcasecmp (channel->label, "XV_HUE") == 0) {
    value = evaspixmapsink->hue;
  } else if (g_ascii_strcasecmp (channel->label, "XV_SATURATION") == 0) {
    value = evaspixmapsink->saturation;
  } else if (g_ascii_strcasecmp (channel->label, "XV_CONTRAST") == 0) {
    value = evaspixmapsink->contrast;
  } else if (g_ascii_strcasecmp (channel->label, "XV_BRIGHTNESS") == 0) {
    value = evaspixmapsink->brightness;
  } else {
    g_warning ("got an unknown channel %s", channel->label);
  }

  /* Normalize val to [channel->min_value, channel->max_value] */
  value = channel->min_value + (channel->max_value - channel->min_value) * (value + 1000) / 2000;

  return value;
}

static void
gst_evaspixmapsink_colorbalance_init (GstColorBalanceInterface *iface)
{
  iface->list_channels = gst_evaspixmapsink_colorbalance_list_channels;
  iface->set_value = gst_evaspixmapsink_colorbalance_set_value;
  iface->get_value = gst_evaspixmapsink_colorbalance_get_value;
}

static const GList *
gst_evaspixmapsink_probe_get_properties (GstPropertyProbe *probe)
{
	GObjectClass *klass = G_OBJECT_GET_CLASS (probe);
	static GList *list = NULL;

	if (!list) {
		list = g_list_append (NULL, g_object_class_find_property (klass, "device"));
		list = g_list_append (list, g_object_class_find_property (klass, "autopaint-colorkey"));
		list = g_list_append (list, g_object_class_find_property (klass, "double-buffer"));
		list = g_list_append (list, g_object_class_find_property (klass, "colorkey"));
	}

	return list;
}

static void
gst_evaspixmapsink_probe_probe_property (GstPropertyProbe *probe, guint prop_id, const GParamSpec *pspec)
{
  GstEvasPixmapSink *evaspixmapsink = GST_EVASPIXMAPSINK (probe);

  switch (prop_id) {
    case PROP_DEVICE:
    case PROP_AUTOPAINT_COLORKEY:
    case PROP_DOUBLE_BUFFER:
    case PROP_COLORKEY:
      GST_DEBUG_OBJECT (evaspixmapsink,"probing device list and get capabilities");
      if (!evaspixmapsink->xcontext) {
        GST_DEBUG_OBJECT (evaspixmapsink,"generating xcontext");
        evaspixmapsink->xcontext = gst_evaspixmapsink_xcontext_get (evaspixmapsink);
        if (!evaspixmapsink->xcontext) {
          GST_ERROR_OBJECT (evaspixmapsink,"could not get xcontext..");
        }
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (probe, prop_id, pspec);
      break;
  }
}

static gboolean
gst_evaspixmapsink_probe_needs_probe (GstPropertyProbe *probe, guint prop_id, const GParamSpec *pspec)
{
  GstEvasPixmapSink *evaspixmapsink = GST_EVASPIXMAPSINK (probe);
  gboolean ret = FALSE;

  switch (prop_id) {
    case PROP_DEVICE:
    case PROP_AUTOPAINT_COLORKEY:
    case PROP_DOUBLE_BUFFER:
    case PROP_COLORKEY:
      if (evaspixmapsink->xcontext != NULL) {
        ret = FALSE;
      } else {
        ret = TRUE;
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (probe, prop_id, pspec);
      break;
  }

  return ret;
}

static GValueArray *
gst_evaspixmapsink_probe_get_values (GstPropertyProbe *probe, guint prop_id, const GParamSpec *pspec)
{
  GstEvasPixmapSink *evaspixmapsink = GST_EVASPIXMAPSINK (probe);
  GValueArray *array = NULL;

  if (G_UNLIKELY (!evaspixmapsink->xcontext)) {
    GST_WARNING_OBJECT (evaspixmapsink,"we don't have any xcontext, can't "
        "get values");
    goto beach;
  }

  switch (prop_id) {
    case PROP_DEVICE:
    {
      guint i;
      GValue value = { 0 };

      array = g_value_array_new (evaspixmapsink->xcontext->nb_adaptors);
      g_value_init (&value, G_TYPE_STRING);

      for (i = 0; i < evaspixmapsink->xcontext->nb_adaptors; i++) {
        gchar *adaptor_id_s = g_strdup_printf ("%u", i);

        g_value_set_string (&value, adaptor_id_s);
        g_value_array_append (array, &value);
        g_free (adaptor_id_s);
      }
      g_value_unset (&value);
      break;
    }
    case PROP_AUTOPAINT_COLORKEY:
      if (evaspixmapsink->have_autopaint_colorkey) {
        GValue value = { 0 };

        array = g_value_array_new (2);
        g_value_init (&value, G_TYPE_BOOLEAN);
        g_value_set_boolean (&value, FALSE);
        g_value_array_append (array, &value);
        g_value_set_boolean (&value, TRUE);
        g_value_array_append (array, &value);
        g_value_unset (&value);
      }
      break;
    case PROP_DOUBLE_BUFFER:
      if (evaspixmapsink->have_double_buffer) {
        GValue value = { 0 };

        array = g_value_array_new (2);
        g_value_init (&value, G_TYPE_BOOLEAN);
        g_value_set_boolean (&value, FALSE);
        g_value_array_append (array, &value);
        g_value_set_boolean (&value, TRUE);
        g_value_array_append (array, &value);
        g_value_unset (&value);
      }
      break;
    case PROP_COLORKEY:
      if (evaspixmapsink->have_colorkey) {
        GValue value = { 0 };

        array = g_value_array_new (1);
        g_value_init (&value, GST_TYPE_INT_RANGE);
        gst_value_set_int_range (&value, 0, 0xffffff);
        g_value_array_append (array, &value);
        g_value_unset (&value);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (probe, prop_id, pspec);
      break;
  }

beach:
  return array;
}

static void
gst_evaspixmapsink_property_probe_interface_init (GstPropertyProbeInterface *iface)
{
	iface->get_properties = gst_evaspixmapsink_probe_get_properties;
	iface->probe_property = gst_evaspixmapsink_probe_probe_property;
	iface->needs_probe = gst_evaspixmapsink_probe_needs_probe;
	iface->get_values = gst_evaspixmapsink_probe_get_values;
}

static gboolean
gst_evaspixmapsink_xpixmap_link (GstEvasPixmapSink *evaspixmapsink)
{
	Display *dpy;
	Pixmap *pixmap_id;
	int evas_object_width = 0;
	int evas_object_height = 0;
	int pixmap_width = 0;
	int pixmap_height = 0;
	int xw = 0;
	int xh = 0;

	if (!evaspixmapsink) {
		GST_ERROR_OBJECT (evaspixmapsink,"could not get evaspixmapsink..");
		return FALSE;
	}
	g_mutex_lock (evaspixmapsink->flow_lock);

	/* Set xcontext and display */
	if (!evaspixmapsink->xcontext) {
		GST_WARNING_OBJECT (evaspixmapsink,"there's no xcontext, try to get one..");
		evaspixmapsink->xcontext = gst_evaspixmapsink_xcontext_get (evaspixmapsink);
		if (!evaspixmapsink->xcontext) {
			GST_ERROR_OBJECT (evaspixmapsink,"could not get xcontext..");
			return FALSE;
		}
	}

	dpy = evaspixmapsink->xcontext->disp;

	/* Set evas image object size */
	evas_object_geometry_get(evaspixmapsink->eo, NULL, NULL, &evas_object_width, &evas_object_height);
	if (evaspixmapsink->use_origin_size || !evas_object_width || !evas_object_height) {
		pixmap_width = evaspixmapsink->video_width;
		pixmap_height = evaspixmapsink->video_height;
		GST_INFO_OBJECT (evaspixmapsink,"set size to media src size(%dx%d)", pixmap_width, pixmap_height);
	}

	g_mutex_lock (evaspixmapsink->x_lock);
	if (evaspixmapsink->use_origin_size || !evas_object_width || !evas_object_height) {
		XvQueryBestSize(dpy, evaspixmapsink->xcontext->xv_port_id,0,0,0, pixmap_width, pixmap_height, &xw, &xh);
		if (!evas_object_width || !evas_object_height) {
			evaspixmapsink->w = xw;
			evaspixmapsink->h = xh;
		} else {
			evaspixmapsink->w = evas_object_width;
			evaspixmapsink->h = evas_object_height;
		}
		GST_DEBUG_OBJECT (evaspixmapsink,"XvQueryBestSize : xv_port_id(%d), w(%d),h(%d) => xw(%d),xh(%d)", evaspixmapsink->xcontext->xv_port_id, pixmap_width, pixmap_height, xw, xh);
	} else {
		XvQueryBestSize(dpy, evaspixmapsink->xcontext->xv_port_id,0,0,0, evas_object_width, evas_object_height, &xw, &xh);
		GST_DEBUG_OBJECT (evaspixmapsink,"XvQueryBestSize : xv_port_id(%d), w(%d),h(%d) => xw(%d),xh(%d)", evaspixmapsink->xcontext->xv_port_id, evas_object_width, evas_object_height, xw, xh);
		evaspixmapsink->w = xw;
		evaspixmapsink->h = xh;
	}
	evas_object_image_size_set(evaspixmapsink->eo, evaspixmapsink->w, evaspixmapsink->h);

	/* create xpixmap structure */
	if (!evaspixmapsink->xpixmap) {
		/* xpixmap can be created in this function only */
		evaspixmapsink->xpixmap = g_new0 (GstXPixmap, 1);
		if(!evaspixmapsink->xpixmap) {
			GST_ERROR_OBJECT (evaspixmapsink,"xpixmap is not valid..");
			goto GO_OUT_OF_FUNC;
		}
	}

	/* create pixmap */
	if (!xw || !xh) {
		GST_WARNING_OBJECT (evaspixmapsink,"skip creating pixmap..xw(%d),xh(%d)",xw,xh);
		goto GO_OUT_OF_FUNC;
	} else {
		pixmap_id = XCreatePixmap(dpy, DefaultRootWindow(dpy), xw, xh, DefaultDepth(dpy, DefaultScreen(dpy)));
		if ( (int)pixmap_id == BadAlloc || (int)pixmap_id == BadDrawable || (int)pixmap_id == BadValue ) {
			GST_ERROR_OBJECT (evaspixmapsink,"pixmap allocation error..");
			goto GO_OUT_OF_FUNC;
		}
		GST_DEBUG_OBJECT (evaspixmapsink,"evas_object_width(%d),evas_object_height(%d),pixmap:%d,depth:%d",
			evas_object_width,evas_object_height,pixmap_id,DefaultDepth(dpy, DefaultScreen(dpy)));
	}

	if (evaspixmapsink->xpixmap->pixmap && pixmap_id != evaspixmapsink->xpixmap->pixmap) {
		/* If we reset another pixmap, do below */
		GST_DEBUG_OBJECT (evaspixmapsink,"destroy former pixmap(%d)",evaspixmapsink->xpixmap->pixmap);
		if (evaspixmapsink->eo) {
			evas_object_image_native_surface_set(evaspixmapsink->eo, NULL);
		}
		g_mutex_unlock (evaspixmapsink->x_lock);
		gst_evaspixmapsink_xpixmap_clear (evaspixmapsink, evaspixmapsink->xpixmap);
		g_mutex_lock (evaspixmapsink->x_lock);
		if(evaspixmapsink->xpixmap->pixmap) {
			XFreePixmap(evaspixmapsink->xcontext->disp, evaspixmapsink->xpixmap->pixmap);
			evaspixmapsink->xpixmap->pixmap = NULL;
			GST_DEBUG_OBJECT (evaspixmapsink,"Free pixmap");
		}
		XFreeGC (evaspixmapsink->xcontext->disp, evaspixmapsink->xpixmap->gc);
		XSync (evaspixmapsink->xcontext->disp, FALSE);
	}

	/* Set pixmap id and create GC */
	evaspixmapsink->xpixmap->pixmap = pixmap_id;
	evaspixmapsink->xpixmap->gc = XCreateGC(dpy, evaspixmapsink->xpixmap->pixmap, 0,0);
	XSetForeground(dpy, evaspixmapsink->xpixmap->gc,evaspixmapsink->xcontext->black);
	XFillRectangle(dpy, evaspixmapsink->xpixmap->pixmap, evaspixmapsink->xpixmap->gc, 0, 0, xw, xh);
	XSync(dpy, FALSE);

	/* Create XDamage */
	if (evaspixmapsink->damage) {
		GST_DEBUG_OBJECT (evaspixmapsink,"destroy former damage(%d)",evaspixmapsink->damage);
		XDamageDestroy(evaspixmapsink->xcontext->disp, evaspixmapsink->damage);
		evaspixmapsink->damage = NULL;
	}
	evaspixmapsink->damage = XDamageCreate (dpy, evaspixmapsink->xpixmap->pixmap, XDamageReportRawRectangles);

	/* Set flag for mapping evas object with xpixmap */
	evaspixmapsink->do_link = TRUE;
	ecore_pipe_write(evaspixmapsink->epipe, evaspixmapsink, sizeof(GstEvasPixmapSink));

	gst_evaspixmapsink_update_colorbalance (evaspixmapsink);

	g_mutex_unlock (evaspixmapsink->x_lock);
	g_mutex_unlock (evaspixmapsink->flow_lock);

	return TRUE;

GO_OUT_OF_FUNC:
	g_mutex_unlock (evaspixmapsink->x_lock);
	g_mutex_unlock (evaspixmapsink->flow_lock);
	return FALSE;
}

static void
gst_evaspixmapsink_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GstEvasPixmapSink *evaspixmapsink;
	g_return_if_fail (GST_IS_EVASPIXMAPSINK (object));
	evaspixmapsink = GST_EVASPIXMAPSINK (object);

	Evas_Object *eo;

	switch (prop_id) {
	case PROP_HUE:
		evaspixmapsink->hue = g_value_get_int (value);
		evaspixmapsink->cb_changed = TRUE;
		gst_evaspixmapsink_update_colorbalance (evaspixmapsink);
		break;
	case PROP_CONTRAST:
		evaspixmapsink->contrast = g_value_get_int (value);
		evaspixmapsink->cb_changed = TRUE;
		gst_evaspixmapsink_update_colorbalance (evaspixmapsink);
		break;
	case PROP_BRIGHTNESS:
		evaspixmapsink->brightness = g_value_get_int (value);
		evaspixmapsink->cb_changed = TRUE;
		gst_evaspixmapsink_update_colorbalance (evaspixmapsink);
		break;
	case PROP_SATURATION:
		evaspixmapsink->saturation = g_value_get_int (value);
		evaspixmapsink->cb_changed = TRUE;
		gst_evaspixmapsink_update_colorbalance (evaspixmapsink);
		break;
	case PROP_DISPLAY:
		evaspixmapsink->display_name = g_strdup (g_value_get_string (value));
		break;
	case PROP_SYNCHRONOUS:
		evaspixmapsink->synchronous = g_value_get_boolean (value);
		if (evaspixmapsink->xcontext) {
			XSynchronize (evaspixmapsink->xcontext->disp, evaspixmapsink->synchronous);
			GST_DEBUG_OBJECT (evaspixmapsink,"XSynchronize called with %s", evaspixmapsink->synchronous ? "TRUE" : "FALSE");
		}
		break;
	case PROP_PIXEL_ASPECT_RATIO:
		g_free (evaspixmapsink->par);
		evaspixmapsink->par = g_new0 (GValue, 1);
		g_value_init (evaspixmapsink->par, GST_TYPE_FRACTION);
		if (!g_value_transform (value, evaspixmapsink->par)) {
			g_warning ("Could not transform string to aspect ratio");
			gst_value_set_fraction (evaspixmapsink->par, 1, 1);
		}
		GST_DEBUG_OBJECT (evaspixmapsink,"set PAR to %d/%d", gst_value_get_fraction_numerator (evaspixmapsink->par), gst_value_get_fraction_denominator (evaspixmapsink->par));
		break;
	case PROP_FORCE_ASPECT_RATIO:
		evaspixmapsink->keep_aspect = g_value_get_boolean (value);
		break;
	case PROP_DEVICE:
		evaspixmapsink->adaptor_no = atoi (g_value_get_string (value));
		break;
	case PROP_DOUBLE_BUFFER:
		evaspixmapsink->double_buffer = g_value_get_boolean (value);
		break;
	case PROP_AUTOPAINT_COLORKEY:
		evaspixmapsink->autopaint_colorkey = g_value_get_boolean (value);
		break;
	case PROP_COLORKEY:
		evaspixmapsink->colorkey = g_value_get_int (value);
		break;
	case PROP_PIXMAP_WIDTH:
		if (evaspixmapsink->xpixmap) {
			evaspixmapsink->xpixmap->width = g_value_get_uint64 (value);
			/* To do : code related to pixmap re-link */
		}
		break;
	case PROP_PIXMAP_HEIGHT:
		if (evaspixmapsink->xpixmap) {
			evaspixmapsink->xpixmap->height = g_value_get_uint64 (value);
			/* To do : code related to pixmap re-link */
		}
		break;
#ifdef GST_EXT_XV_ENHANCEMENT
	case PROP_DISPLAY_GEOMETRY_METHOD:
		evaspixmapsink->display_geometry_method = g_value_get_enum (value);
		GST_INFO_OBJECT (evaspixmapsink,"Overlay geometry method update, display_geometry_method(%d)",evaspixmapsink->display_geometry_method);
		if( evaspixmapsink->display_geometry_method != DISP_GEO_METHOD_FULL_SCREEN &&
			  evaspixmapsink->display_geometry_method != DISP_GEO_METHOD_CROPPED_FULL_SCREEN ) {
			if( evaspixmapsink->xcontext && evaspixmapsink->xpixmap ) {
				g_mutex_lock( evaspixmapsink->flow_lock );
				gst_evaspixmapsink_xpixmap_clear (evaspixmapsink, evaspixmapsink->xpixmap);
				g_mutex_unlock( evaspixmapsink->flow_lock );
			}
		}
		if (evaspixmapsink->xcontext) {
			gst_evaspixmap_buffer_put (evaspixmapsink, evaspixmapsink->evas_pixmap_buf);
		}
		break;
	case PROP_DST_ROI_X:
		evaspixmapsink->dst_roi.x = g_value_get_int (value);
		GST_INFO_OBJECT (evaspixmapsink, "ROI_X(%d)",evaspixmapsink->dst_roi.x );
		break;
	case PROP_DST_ROI_Y:
		evaspixmapsink->dst_roi.y = g_value_get_int (value);
		GST_INFO_OBJECT (evaspixmapsink, "ROI_Y(%d)",evaspixmapsink->dst_roi.y );
		break;
	case PROP_DST_ROI_W:
		evaspixmapsink->dst_roi.w = g_value_get_int (value);
		GST_INFO_OBJECT (evaspixmapsink, "ROI_W(%d)",evaspixmapsink->dst_roi.w );
		break;
	case PROP_DST_ROI_H:
		evaspixmapsink->dst_roi.h = g_value_get_int (value);
		GST_INFO_OBJECT (evaspixmapsink, "ROI_H(%d)",evaspixmapsink->dst_roi.h );
		break;
	case PROP_STOP_VIDEO:
		evaspixmapsink->stop_video = g_value_get_int (value);
		g_mutex_lock( evaspixmapsink->flow_lock );
		if( evaspixmapsink->stop_video ) {
			GST_INFO_OBJECT (evaspixmapsink, "XPixmap CLEAR when set video-stop property" );
			gst_evaspixmapsink_xpixmap_clear (evaspixmapsink, evaspixmapsink->xpixmap);
		}
		g_mutex_unlock( evaspixmapsink->flow_lock );
		break;
#endif
	case PROP_EVAS_OBJECT:
		eo = g_value_get_pointer (value);
		if ( is_evas_image_object (eo)) {
			if (!evaspixmapsink->epipe) {
				evaspixmapsink->epipe = ecore_pipe_add (ecore_pipe_callback_handler, evaspixmapsink);
				if (!evaspixmapsink->epipe) {
					GST_ERROR_OBJECT (evaspixmapsink,"Cannot set evas-object property: ecore_pipe_add() failed");
					break;
				}
			}
			if (eo != evaspixmapsink->eo) {
				/* delete evas object callbacks registrated on a former evas image object */
				evas_object_event_callback_del (evaspixmapsink->eo, EVAS_CALLBACK_DEL, evas_callback_del_event);
				evas_object_event_callback_del (evaspixmapsink->eo, EVAS_CALLBACK_RESIZE, evas_callback_resize_event);
				if (evaspixmapsink->eo) {
					if (!gst_evaspixmapsink_xpixmap_link(evaspixmapsink)) {
						GST_WARNING_OBJECT (evaspixmapsink,"link evas image object with pixmap failed...");
						return;
					}
				}
				evaspixmapsink->eo = eo;
				/* add evas object callbacks on a new evas image object */
				evas_object_event_callback_add (evaspixmapsink->eo, EVAS_CALLBACK_DEL, evas_callback_del_event, evaspixmapsink);
				evas_object_event_callback_add (evaspixmapsink->eo, EVAS_CALLBACK_RESIZE, evas_callback_resize_event, evaspixmapsink);
				GST_INFO_OBJECT (evaspixmapsink,"Evas Image Object(%x) is set", evaspixmapsink->eo);
			}
		} else {
			GST_ERROR_OBJECT (evaspixmapsink,"Cannot set evas-object property: value is not an evas image object");
		}
		  break;
	case PROP_VISIBLE:
		evaspixmapsink->visible = g_value_get_boolean (value);
		if (evaspixmapsink->eo) {
			if (!evaspixmapsink->visible) {
				if ( evaspixmapsink->xcontext && evaspixmapsink->xpixmap ) {
					g_mutex_lock( evaspixmapsink->flow_lock );
					gst_evaspixmapsink_xpixmap_clear (evaspixmapsink, evaspixmapsink->xpixmap);
					g_mutex_unlock( evaspixmapsink->flow_lock );
				}
				evas_object_hide(evaspixmapsink->eo);
				GST_INFO_OBJECT (evaspixmapsink,"object hide..");
			} else {
				evas_object_show(evaspixmapsink->eo);
				GST_INFO_OBJECT (evaspixmapsink,"object show..");
			}
		} else {
			GST_WARNING_OBJECT (evaspixmapsink,"evas image object was not set");
		}
		break;
	case PROP_ORIGIN_SIZE:
		evaspixmapsink->use_origin_size = g_value_get_boolean (value);
		GST_INFO_OBJECT (evaspixmapsink,"set origin-size (%d)",evaspixmapsink->use_origin_size);
		if (evaspixmapsink->former_origin_size != evaspixmapsink->use_origin_size) {
			if (!gst_evaspixmapsink_xpixmap_link(evaspixmapsink)) {
				GST_WARNING_OBJECT (evaspixmapsink,"link evas image object with pixmap failed...");
			}
			evaspixmapsink->former_origin_size = evaspixmapsink->use_origin_size;
		}
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gst_evaspixmapsink_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GstEvasPixmapSink *evaspixmapsink;

	g_return_if_fail (GST_IS_EVASPIXMAPSINK (object));

	evaspixmapsink = GST_EVASPIXMAPSINK (object);

	switch (prop_id) {
	case PROP_HUE:
		g_value_set_int (value, evaspixmapsink->hue);
		break;
	case PROP_CONTRAST:
		g_value_set_int (value, evaspixmapsink->contrast);
		break;
	case PROP_BRIGHTNESS:
		g_value_set_int (value, evaspixmapsink->brightness);
		break;
	case PROP_SATURATION:
		g_value_set_int (value, evaspixmapsink->saturation);
		break;
	case PROP_DISPLAY:
		g_value_set_string (value, evaspixmapsink->display_name);
		break;
	case PROP_SYNCHRONOUS:
		g_value_set_boolean (value, evaspixmapsink->synchronous);
		break;
	case PROP_PIXEL_ASPECT_RATIO:
		if (evaspixmapsink->par) {
			if (!g_value_transform (evaspixmapsink->par, value)) {
				g_warning ("g_value_transform() failure");
			}
		}
		break;
	case PROP_FORCE_ASPECT_RATIO:
		g_value_set_boolean (value, evaspixmapsink->keep_aspect);
		break;
	case PROP_DEVICE:
	{
		char *adaptor_no_s = g_strdup_printf ("%u", evaspixmapsink->adaptor_no);
		g_value_set_string (value, adaptor_no_s);
		g_free (adaptor_no_s);
		break;
	}
	case PROP_DEVICE_NAME:
		if (evaspixmapsink->xcontext && evaspixmapsink->xcontext->adaptors) {
			g_value_set_string (value,
			evaspixmapsink->xcontext->adaptors[evaspixmapsink->adaptor_no]);
		} else {
			g_value_set_string (value, NULL);
		}
		break;
	case PROP_DOUBLE_BUFFER:
		g_value_set_boolean (value, evaspixmapsink->double_buffer);
		break;
	case PROP_AUTOPAINT_COLORKEY:
		g_value_set_boolean (value, evaspixmapsink->autopaint_colorkey);
		break;
	case PROP_COLORKEY:
		g_value_set_int (value, evaspixmapsink->colorkey);
		break;
	case PROP_PIXMAP_WIDTH:
		if (evaspixmapsink->xpixmap) {
			g_value_set_uint64 (value, evaspixmapsink->xpixmap->width);
		} else {
			g_value_set_uint64 (value, 0);
		}
		break;
	case PROP_PIXMAP_HEIGHT:
		if (evaspixmapsink->xpixmap) {
			g_value_set_uint64 (value, evaspixmapsink->xpixmap->height);
		} else {
			g_value_set_uint64 (value, 0);
		}
		break;
#ifdef GST_EXT_XV_ENHANCEMENT
	case PROP_DISPLAY_GEOMETRY_METHOD:
		g_value_set_enum (value, evaspixmapsink->display_geometry_method);
		break;
	case PROP_DST_ROI_X:
		g_value_set_int (value, evaspixmapsink->dst_roi.x);
		break;
	case PROP_DST_ROI_Y:
		g_value_set_int (value, evaspixmapsink->dst_roi.y);
		break;
	case PROP_DST_ROI_W:
		g_value_set_int (value, evaspixmapsink->dst_roi.w);
		break;
	case PROP_DST_ROI_H:
		g_value_set_int (value, evaspixmapsink->dst_roi.h);
		break;
	case PROP_STOP_VIDEO:
		g_value_set_int (value, evaspixmapsink->stop_video);
		break;
#endif
	case PROP_EVAS_OBJECT:
		g_value_set_pointer (value, evaspixmapsink->eo);
		break;
	case PROP_VISIBLE:
		g_value_set_boolean (value, evaspixmapsink->visible);
		break;
	case PROP_ORIGIN_SIZE:
		g_value_set_boolean (value, evaspixmapsink->use_origin_size);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
  }
}

static void
gst_evaspixmapsink_reset (GstEvasPixmapSink *evaspixmapsink)
{
	GST_DEBUG_OBJECT (evaspixmapsink,"[START]");

	GThread *thread;
	GST_OBJECT_LOCK (evaspixmapsink);
	evaspixmapsink->running = FALSE;

	/* grab thread and mark it as NULL */
	thread = evaspixmapsink->event_thread;
	evaspixmapsink->event_thread = NULL;
	GST_OBJECT_UNLOCK (evaspixmapsink);

	/* Wait for our event thread to finish before we clean up our stuff. */
	if (thread) {
		g_thread_join (thread);
	}

	if(evaspixmapsink->damage) {
		XDamageDestroy(evaspixmapsink->xcontext->disp, evaspixmapsink->damage);
		evaspixmapsink->damage = NULL;
	}
	if(evaspixmapsink->handler) {
		ecore_event_handler_del (evaspixmapsink->handler);
		evaspixmapsink->handler = NULL;
	}

	evas_object_event_callback_del (evaspixmapsink->eo, EVAS_CALLBACK_RESIZE, evas_callback_resize_event);
	evas_object_event_callback_del (evaspixmapsink->eo, EVAS_CALLBACK_DEL, evas_callback_del_event);

	if (evaspixmapsink->evas_pixmap_buf) {
		g_boxed_free(GST_TYPE_EVASPIXMAP_BUFFER, evaspixmapsink->evas_pixmap_buf);
		evaspixmapsink->evas_pixmap_buf = NULL;
	}
	if (evaspixmapsink->xpixmap) {
		gst_evaspixmapsink_xpixmap_clear (evaspixmapsink, evaspixmapsink->xpixmap);
		gst_evaspixmapsink_xpixmap_destroy (evaspixmapsink, evaspixmapsink->xpixmap);
		evaspixmapsink->xpixmap = NULL;
		if (evaspixmapsink->eo) {
			evas_object_image_native_surface_set(evaspixmapsink->eo, NULL);
			evaspixmapsink->eo = NULL;
		}
	}
	evaspixmapsink->render_rect.x = evaspixmapsink->render_rect.y =
	evaspixmapsink->render_rect.w = evaspixmapsink->render_rect.h = 0;
	evaspixmapsink->have_render_rect = FALSE;

	gst_evaspixmapsink_xcontext_clear (evaspixmapsink);

	GST_DEBUG_OBJECT (evaspixmapsink,"[END]");
}

/* Finalize is called only once, dispose can be called multiple times.
 * We use mutexes and don't reset stuff to NULL here so let's register
 * as a finalize. */
static void
gst_evaspixmapsink_finalize (GObject *object)
{
	GstEvasPixmapSink *evaspixmapsink;
	evaspixmapsink = GST_EVASPIXMAPSINK (object);
	GST_DEBUG_OBJECT (evaspixmapsink,"[START]");

	if (evaspixmapsink->display_name) {
		g_free (evaspixmapsink->display_name);
		evaspixmapsink->display_name = NULL;
	}
	if (evaspixmapsink->par) {
		g_free (evaspixmapsink->par);
		evaspixmapsink->par = NULL;
	}
	if (evaspixmapsink->x_lock) {
		g_mutex_free (evaspixmapsink->x_lock);
		evaspixmapsink->x_lock = NULL;
	}
	if (evaspixmapsink->flow_lock) {
		g_mutex_free (evaspixmapsink->flow_lock);
		evaspixmapsink->flow_lock = NULL;
	}
	if (evaspixmapsink->epipe) {
		ecore_pipe_del (evaspixmapsink->epipe);
		evaspixmapsink->epipe = NULL;
	}

	GST_DEBUG_OBJECT (evaspixmapsink,"[END]");

	G_OBJECT_CLASS (parent_class)->finalize (object);

}

static void
gst_evaspixmapsink_init (GstEvasPixmapSink *evaspixmapsink)
{
	evaspixmapsink->display_name = NULL;
	evaspixmapsink->adaptor_no = 0;
	evaspixmapsink->xcontext = NULL;
	evaspixmapsink->xpixmap = NULL;
	evaspixmapsink->evas_pixmap_buf = NULL;

	evaspixmapsink->hue = evaspixmapsink->saturation = 0;
	evaspixmapsink->contrast = evaspixmapsink->brightness = 0;
	evaspixmapsink->cb_changed = FALSE;

	evaspixmapsink->fps_n = 0;
	evaspixmapsink->fps_d = 0;
	evaspixmapsink->video_width = 0;
	evaspixmapsink->video_height = 0;

	evaspixmapsink->x_lock = g_mutex_new ();
	evaspixmapsink->flow_lock = g_mutex_new ();

	evaspixmapsink->synchronous = FALSE;
	evaspixmapsink->double_buffer = TRUE;
	evaspixmapsink->keep_aspect = FALSE;
	evaspixmapsink->par = NULL;
	evaspixmapsink->autopaint_colorkey = TRUE;
	evaspixmapsink->running = FALSE;

	/* on 16bit displays this becomes r,g,b = 1,2,3
	* on 24bit displays this becomes r,g,b = 8,8,16
	* as a port atom value
	*/
	evaspixmapsink->colorkey = (8 << 16) | (8 << 8) | 16;

#ifdef GST_EXT_XV_ENHANCEMENT
	evaspixmapsink->display_geometry_method = DEF_DISPLAY_GEOMETRY_METHOD;
	evaspixmapsink->dst_roi.x = 0;
	evaspixmapsink->dst_roi.y = 0;
	evaspixmapsink->dst_roi.w = 0;
	evaspixmapsink->dst_roi.h = 0;
	evaspixmapsink->scr_w = 0;
	evaspixmapsink->scr_h = 0;
	evaspixmapsink->aligned_width = 0;
	evaspixmapsink->aligned_height = 0;
#endif
	evaspixmapsink->stop_video = FALSE;
	evaspixmapsink->eo = NULL;
	evaspixmapsink->epipe = NULL;
	evaspixmapsink->do_link = FALSE;
	evaspixmapsink->visible = TRUE;
	evaspixmapsink->use_origin_size = FALSE;
	evaspixmapsink->former_origin_size = FALSE;

 }

static void
gst_evaspixmapsink_base_init (gpointer g_class)
{
}

static void
gst_evaspixmapsink_class_init (GstEvasPixmapSinkClass *klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSinkClass *gstbasesink_class;
  GstVideoSinkClass *videosink_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbasesink_class = (GstBaseSinkClass *) klass;
  videosink_class = (GstVideoSinkClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->set_property = gst_evaspixmapsink_set_property;
  gobject_class->get_property = gst_evaspixmapsink_get_property;

  g_object_class_install_property (gobject_class, PROP_CONTRAST,
      g_param_spec_int ("contrast", "Contrast", "The contrast of the video",
          -1000, 1000, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_BRIGHTNESS,
      g_param_spec_int ("brightness", "Brightness",
          "The brightness of the video", -1000, 1000, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_HUE,
      g_param_spec_int ("hue", "Hue", "The hue of the video", -1000, 1000, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_SATURATION,
      g_param_spec_int ("saturation", "Saturation",
          "The saturation of the video", -1000, 1000, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_DISPLAY,
      g_param_spec_string ("display", "Display", "X Display name", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_SYNCHRONOUS,
      g_param_spec_boolean ("synchronous", "Synchronous",
          "When enabled, runs "
          "the X display in synchronous mode. (used only for debugging)", FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PIXEL_ASPECT_RATIO,
      g_param_spec_string ("pixel-aspect-ratio", "Pixel Aspect Ratio",
          "The pixel aspect ratio of the device", "1/1",
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_FORCE_ASPECT_RATIO,
      g_param_spec_boolean ("force-aspect-ratio", "Force aspect ratio",
          "When enabled, scaling will respect original aspect ratio", FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_DEVICE,
      g_param_spec_string ("device", "Adaptor number",
          "The number of the video adaptor", "0",
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_DEVICE_NAME,
      g_param_spec_string ("device-name", "Adaptor name",
          "The name of the video adaptor", NULL,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * GstEvasPixmapSink:double-buffer
   *
   * Whether to double-buffer the output.
   *
   * Since: 0.10.14
   */
  g_object_class_install_property (gobject_class, PROP_DOUBLE_BUFFER,
      g_param_spec_boolean ("double-buffer", "Double-buffer",
          "Whether to double-buffer the output", TRUE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  /**
   * GstEvasPixmapSink:autopaint-colorkey
   *
   * Whether to autofill overlay with colorkey
   *
   * Since: 0.10.21
   */
  g_object_class_install_property (gobject_class, PROP_AUTOPAINT_COLORKEY,
      g_param_spec_boolean ("autopaint-colorkey", "Autofill with colorkey",
          "Whether to autofill overlay with colorkey", TRUE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  /**
   * GstEvasPixmapSink:colorkey
   *
   * Color to use for the overlay mask.
   *
   * Since: 0.10.21
   */
  g_object_class_install_property (gobject_class, PROP_COLORKEY,
      g_param_spec_int ("colorkey", "Colorkey",
          "Color to use for the overlay mask", G_MININT, G_MAXINT, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstEvasPixmapSink:pixmap-width
   *
   * Actual width of the pixmap.
   */
  g_object_class_install_property (gobject_class, PROP_PIXMAP_WIDTH,
      g_param_spec_uint64 ("pixmap-width", "pixmap-width", "Width of the pixmap", 0, G_MAXUINT64, 0, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * GstEvasPixmapSink:pixmap-height
   *
   * Actual height of the pixmap.
   */
  g_object_class_install_property (gobject_class, PROP_PIXMAP_HEIGHT,
      g_param_spec_uint64 ("pixmap-height", "pixmap-height", "Height of the pixmap", 0, G_MAXUINT64, 0, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

#ifdef GST_EXT_XV_ENHANCEMENT
  /**
   * GstEvasPixmapSink:display-geometry-method
   *
   * Display geometrical method setting
   */
  g_object_class_install_property(gobject_class, PROP_DISPLAY_GEOMETRY_METHOD,
    g_param_spec_enum("display-geometry-method", "Display geometry method",
      "Geometrical method for display",
      GST_TYPE_EVASPIXMAPSINK_DISPLAY_GEOMETRY_METHOD, DEF_DISPLAY_GEOMETRY_METHOD,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstEvasPixmapSink:dst-roi-x
   *
   * X value of Destination ROI
   */
  g_object_class_install_property (gobject_class, PROP_DST_ROI_X,
      g_param_spec_int ("dst-roi-x", "Dst-ROI-X",
          "X value of Destination ROI(only effective \"CUSTOM_ROI\")", 0, XV_SCREEN_SIZE_WIDTH, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstEvasPixmapSink:dst-roi-y
   *
   * Y value of Destination ROI
   */
  g_object_class_install_property (gobject_class, PROP_DST_ROI_Y,
      g_param_spec_int ("dst-roi-y", "Dst-ROI-Y",
          "Y value of Destination ROI(only effective \"CUSTOM_ROI\")", 0, XV_SCREEN_SIZE_HEIGHT, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstEvasPixmapSink:dst-roi-w
   *
   * W value of Destination ROI
   */
  g_object_class_install_property (gobject_class, PROP_DST_ROI_W,
      g_param_spec_int ("dst-roi-w", "Dst-ROI-W",
          "W value of Destination ROI(only effective \"CUSTOM_ROI\")", 0, XV_SCREEN_SIZE_WIDTH, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstEvasPixmapSink:dst-roi-h
   *
   * H value of Destination ROI
   */
  g_object_class_install_property (gobject_class, PROP_DST_ROI_H,
      g_param_spec_int ("dst-roi-h", "Dst-ROI-H",
          "H value of Destination ROI(only effective \"CUSTOM_ROI\")", 0, XV_SCREEN_SIZE_HEIGHT, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstEvasPixmapSink:stop-video
   *
   * Stop video for releasing video source buffer
   */
  g_object_class_install_property (gobject_class, PROP_STOP_VIDEO,
      g_param_spec_int ("stop-video", "Stop-Video", "Stop video for releasing video source buffer", 0, 1, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
#endif

  /**
   * GstEvasPixmapSink:evas-object
   *
   * Evas image object for rendering
   */
  g_object_class_install_property (gobject_class, PROP_EVAS_OBJECT,
	g_param_spec_pointer ("evas-object", "Destination Evas Object",	"Destination evas image object", G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstEvasPixmapSink:visible
   *
   * visible setting for a evas image object
   */
  g_object_class_install_property (gobject_class, PROP_VISIBLE,
	g_param_spec_boolean ("visible", "Visible", "When setting it false, evas image object does not show", TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

   /**
   * GstEvasPixmapSink:origin-size
   *
   * Set pixmap size with media source's width and height
   */
  g_object_class_install_property (gobject_class, PROP_ORIGIN_SIZE,
	g_param_spec_boolean ("origin-size", "Origin-Size", "When setting it true, pixmap will be created with media source's width and height", FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gobject_class->finalize = gst_evaspixmapsink_finalize;

  gstelement_class->change_state = GST_DEBUG_FUNCPTR (gst_evaspixmapsink_change_state);
  gst_element_class_set_details_simple (gstelement_class,
      "EvasPixmapSink", "Sink/Video",
      "evas image object videosink based on Xv extension", "Sangchul Lee <sc11.lee@samsung.com>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_evaspixmapsink_sink_template_factory));
  gstbasesink_class->get_caps = GST_DEBUG_FUNCPTR (gst_evaspixmapsink_getcaps);
  gstbasesink_class->set_caps = GST_DEBUG_FUNCPTR (gst_evaspixmapsink_setcaps);
  gstbasesink_class->get_times = GST_DEBUG_FUNCPTR (gst_evaspixmapsink_get_times);
  gstbasesink_class->event = GST_DEBUG_FUNCPTR (gst_evaspixmapsink_event);
  videosink_class->show_frame = GST_DEBUG_FUNCPTR (gst_evaspixmapsink_show_frame);
}

/* Object typing & Creation */

static gboolean
plugin_init (GstPlugin *plugin)
{
	if (!gst_element_register (plugin, "evaspixmapsink", GST_RANK_NONE, GST_TYPE_EVASPIXMAPSINK)) {
		return FALSE;
	}
	GST_DEBUG_CATEGORY_INIT (gst_debug_evaspixmapsink, "evaspixmapsink", 0, "evaspixmapsink element");
	GST_DEBUG_CATEGORY_GET (GST_CAT_PERFORMANCE, "GST_PERFORMANCE");

	return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR, GST_VERSION_MINOR,
	evaspixmapsink,"Evas image object render plugin using Xv extension", plugin_init,
	VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
