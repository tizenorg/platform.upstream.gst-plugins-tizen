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

#ifndef __GST_EVASPIXMAPSINK_H__
#define __GST_EVASPIXMAPSINK_H__

#include <gst/video/gstvideosink.h>

#ifdef HAVE_XSHM
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#endif /* HAVE_XSHM */

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#ifdef HAVE_XSHM
#include <X11/extensions/XShm.h>
#endif /* HAVE_XSHM */

#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/damagewire.h>
#ifdef GST_EXT_XV_ENHANCEMENT
#include <X11/Xatom.h>
#include <stdio.h>
#endif

#include <Evas.h>
#include <Ecore.h>
#include <Ecore_X.h>

#include <string.h>
#include <math.h>
#include <stdlib.h>

#define MAX_PLANE_NUM		3
#define MAX_BUFFER_NUM		20
#define MAX_GEM_BUFFER_NUM	(MAX_PLANE_NUM * MAX_BUFFER_NUM)
typedef struct _gem_info_t {
	int dmabuf_fd;
	unsigned int gem_handle;
	unsigned int gem_name;
} gem_info_t;


G_BEGIN_DECLS

#define GST_TYPE_EVASPIXMAPSINK \
  (gst_evaspixmapsink_get_type())
#define GST_EVASPIXMAPSINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_EVASPIXMAPSINK, GstEvasPixmapSink))
#define GST_EVASPIXMAPSINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_EVASPIXMAPSINK, GstEvasPixmapSinkClass))
#define GST_IS_EVASPIXMAPSINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_EVASPIXMAPSINK))
#define GST_IS_EVASPIXMAPSINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_EVASPIXMAPSINK))

#ifdef GST_EXT_XV_ENHANCEMENT
#define XV_SCREEN_SIZE_WIDTH 4096
#define XV_SCREEN_SIZE_HEIGHT 4096
#endif /* GST_EXT_XV_ENHANCEMENT */
#define MARGIN_OF_ERROR 0.005

typedef struct _GstXContext GstXContext;
typedef struct _GstXPixmap GstXPixmap;
typedef struct _GstEvasPixmapFormat GstEvasPixmapFormat;
typedef struct _GstEvasPixmapBuffer GstEvasPixmapBuffer;
typedef struct _GstEvasPixmapBufferClass GstEvasPixmapBufferClass;
typedef struct _GstEvasPixmapSink GstEvasPixmapSink;
typedef struct _GstEvasPixmapSinkClass GstEvasPixmapSinkClass;

/*
 * GstXContext:
 * @disp: the X11 Display of this context
 * @screen: the default Screen of Display @disp
 * @screen_num: the Screen number of @screen
 * @visual: the default Visual of Screen @screen
 * @root: the root Window of Display @disp
 * @white: the value of a white pixel on Screen @screen
 * @black: the value of a black pixel on Screen @screen
 * @depth: the color depth of Display @disp
 * @bpp: the number of bits per pixel on Display @disp
 * @endianness: the endianness of image bytes on Display @disp
 * @width: the width in pixels of Display @disp
 * @height: the height in pixels of Display @disp
 * @widthmm: the width in millimeters of Display @disp
 * @heightmm: the height in millimeters of Display @disp
 * @par: the pixel aspect ratio calculated from @width, @widthmm and @height,
 * @heightmm ratio
 * @use_xshm: used to known wether of not XShm extension is usable or not even
 * if the Extension is present
 * @xv_port_id: the XVideo port ID
 * @im_format: used to store at least a valid format for XShm calls checks
 * @formats_list: list of supported image formats on @xv_port_id
 * @channels_list: list of #GstColorBalanceChannels
 * @caps: the #GstCaps that Display @disp can accept
 *
 * Structure used to store various informations collected/calculated for a
 * Display.
 */
struct _GstXContext {
	Display *disp;

	Screen *screen;
	gint screen_num;

	Visual *visual;

	Window root;

	gulong white, black;

	gint depth;
	gint bpp;
	gint endianness;

	gint width, height;
	gint widthmm, heightmm;
	GValue *par;                  /* calculated pixel aspect ratio */

	gboolean use_xshm;

	XvPortID xv_port_id;
	guint nb_adaptors;
	gchar ** adaptors;
	guint32 im_format;

	GList *formats_list;
	GList *channels_list;

	GstCaps *caps;
};

/*
 * GstXPixmap:
 * @pixmap: the pixmap ID of this X11 pixmap
 * @width: the width in pixels of Pixmap @pixmap
 * @height: the height in pixels of Pixmap @pixmap
 * @gc: the Graphical Context of Pixmap @pixmap
 *
 * Structure used to store informations about a Pixmap.
 */
struct _GstXPixmap {
	Pixmap pixmap;
#ifdef GST_EXT_XV_ENHANCEMENT
	gint x, y;
#endif
	gint width, height;
	GC gc;
};

/**
 * GstEvasPixmapFormat:
 * @format: the image format
 * @caps: generated #GstCaps for this image format
 *
 * Structure storing image format to #GstCaps association.
 */
struct _GstEvasPixmapFormat {
	guint32 format;
	GstCaps *caps;
};

/**
 * GstEvasPixmapBuffer:
 * @evaspixmapsink: a reference to our #GstEvasPixmapSink
 * @xvimage: the XvImage of this buffer
 * @width: the width in pixels of XvImage @xvimage
 * @height: the height in pixels of XvImage @xvimage
 * @im_format: the image format of XvImage @xvimage
 * @size: the size in bytes of XvImage @xvimage
 *
 * Subclass of #GstBuffer containing additional information about an XvImage.
 */
struct _GstEvasPixmapBuffer {
	GstBuffer* buffer;

	/* Reference to the evaspixmapsink we belong to */
	GstEvasPixmapSink *evaspixmapsink;
	XvImage *xvimage;

#ifdef HAVE_XSHM
	XShmSegmentInfo SHMInfo;
#endif /* HAVE_XSHM */

	gint width, height;
	guint32 im_format;
	size_t size;
};

/**
 * GstEvasPixmapSink:
 * @display_name: the name of the Display we want to render to
 * @xcontext: our instance's #GstXContext
 * @xpixmap: the #GstXPixmap we are rendering to
 * @fps_n: the framerate fraction numerator
 * @fps_d: the framerate fraction denominator
 * @x_lock: used to protect X calls as we are not using the XLib in threaded
 * mode
 * @flow_lock: used to protect data flow routines from external calls such as
 * methods from the #GstXOverlay interface
 * @par: used to override calculated pixel aspect ratio from @xcontext
 * @synchronous: used to store if XSynchronous should be used or not (for
 * debugging purpose only)
 * @keep_aspect: used to remember if reverse negotiation scaling should respect
 * aspect ratio
 * @brightness: used to store the user settings for color balance brightness
 * @contrast: used to store the user settings for color balance contrast
 * @hue: used to store the user settings for color balance hue
 * @saturation: used to store the user settings for color balance saturation
 * @cb_changed: used to store if the color balance settings where changed
 * @video_width: the width of incoming video frames in pixels
 * @video_height: the height of incoming video frames in pixels
 *
 * The #GstEvasPixmapSink data structure.
 */
struct _GstEvasPixmapSink {
	/* Our element stuff */
	GstVideoSink videosink;

	char *display_name;
	guint adaptor_no;

	GstXContext *xcontext;
	GstXPixmap *xpixmap;
	GstEvasPixmapBuffer *evas_pixmap_buf;

	GThread *event_thread;
	gboolean running;

	gint fps_n;
	gint fps_d;

	GMutex *x_lock;
	GMutex *flow_lock;

	/* object-set pixel aspect ratio */
	GValue *par;

	gboolean synchronous;
	gboolean double_buffer;
	gboolean keep_aspect;

	gint brightness;
	gint contrast;
	gint hue;
	gint saturation;
	gboolean cb_changed;

	/* size of incoming video, used as the size for XvImage */
	guint video_width, video_height;

	/* display sizes, used for clipping the image */
	gint disp_x, disp_y;
	gint disp_width, disp_height;

	/* port attributes */
	gboolean autopaint_colorkey;
	gint colorkey;

	/* port features */
	gboolean have_autopaint_colorkey;
	gboolean have_colorkey;
	gboolean have_double_buffer;

	/* target video rectagle */
	GstVideoRectangle render_rect;
	gboolean have_render_rect;

#ifdef GST_EXT_XV_ENHANCEMENT
	/* display */
	guint display_geometry_method;
	GstVideoRectangle dst_roi;
	guint scr_w, scr_h;
	/* needed if fourcc is one if S series */
	guint aligned_width;
	guint aligned_height;
	GstCaps *negotiated_caps;
#endif
	gboolean stop_video;

	/* evas object */
	Evas_Object *eo;
	Evas_Coord w;
	Evas_Coord h;
	gboolean visible;

	/* pixmap */
	gboolean do_link;
	gboolean use_origin_size;
	gboolean former_origin_size;

	/* damage event */
	Damage damage;
	int damage_case;
	Ecore_Event_Handler *handler;
	Ecore_Pipe *epipe;

	gint drm_fd;
	gem_info_t gem_info[MAX_GEM_BUFFER_NUM];
};

#ifdef GST_EXT_XV_ENHANCEMENT
/* max plane count *********************************************************/
#define MPLANE_IMGB_MAX_COUNT         (4)

/* image buffer definition ***************************************************

    +------------------------------------------+ ---
    |                                          |  ^
    |     uaddr[], index[]                     |  |
    |     +---------------------------+ ---    |  |
    |     |                           |  ^     |  |
    |     |<-------- width[] -------->|  |     |  |
    |     |                           |  |     |  |
    |     |                           |        |
    |     |                           |height[]|elevation[]
    |     |                           |        |
    |     |                           |  |     |  |
    |     |                           |  |     |  |
    |     |                           |  v     |  |
    |     +---------------------------+ ---    |  |
    |                                          |  v
    +------------------------------------------+ ---

    |<----------------- stride[] ------------------>|
*/
typedef struct _GstMultiPlaneImageBuffer GstMultiPlaneImageBuffer;
struct _GstMultiPlaneImageBuffer
{
    GstBuffer buffer;

    /* width of each image plane */
    gint      width[MPLANE_IMGB_MAX_COUNT];
    /* height of each image plane */
    gint      height[MPLANE_IMGB_MAX_COUNT];
    /* stride of each image plane */
    gint      stride[MPLANE_IMGB_MAX_COUNT];
    /* elevation of each image plane */
    gint      elevation[MPLANE_IMGB_MAX_COUNT];
    /* user space address of each image plane */
    gpointer uaddr[MPLANE_IMGB_MAX_COUNT];
    /* Index of real address of each image plane, if needs */
    gpointer index[MPLANE_IMGB_MAX_COUNT];
    /* left postion, if needs */
    gint      x;
    /* top position, if needs */
    gint      y;
    /* to align memory */
    gint      __dummy2;
    /* arbitrary data */
    gint      data[16];
};
#endif /* GST_EXT_XV_ENHANCEMENT */

struct _GstEvasPixmapSinkClass {
  GstVideoSinkClass parent_class;
};

GType gst_evaspixmapsink_get_type(void);

G_END_DECLS

#endif /* __GST_EVASPIXMAPSINK_H__ */
