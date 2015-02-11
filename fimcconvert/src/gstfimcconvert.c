/*
 * FimcConvert
 *
 * Copyright (c) 2000 - 2013 Samsung Electronics Co., Ltd.
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

/*! Revision History:
 *! ------------------------------------------------------------------------------------
 *!     DATE    |          AUTHOR          |              COMMENTS
 *! ------------------------------------------------------------------------------------
 *! 22-Sep-2010   naveen.ch@samsung.com
 *! 02-Aug-2012   sc11.lee@samsung.com       expand input/output colorspace formats
 *! 15-Oct-2012   sc11.lee@samsung.com       apply DRM-IPP interface
 *! 24-Oct-2012   sc11.lee@samsung.com       support DMA-BUF
 *! 16-Mar-2013   sc11.lee@samsung.com       support TBM
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstfimcconvert.h"

#define DEBUG_FOR_DV

#ifdef DEBUG_FOR_DV
	int total_converted_frame_count = 0;
	int total_buffer_unref_count = 0;
#endif

/* debug variable definition */
GST_DEBUG_CATEGORY(fimcconvert_debug);
#define GST_CAT_DEFAULT fimcconvert_debug

enum
{
	PROP_0,
	PROP_ROT_ANG,
	PROP_DST_WIDTH,
	PROP_DST_HEIGHT,
	PROP_FLIP,
	PROP_SRC_RANDOM_IDX,
	PROP_DST_BUFFER_NUM,
};

typedef enum {
	FIMCCONVERT_ROTATE_NONE = 0,
	FIMCCONVERT_ROTATE_90 = 90,
	FIMCCONVERT_ROTATE_180 = 180,
	FIMCCONVERT_ROTATE_270 = 270
} FIMCConvertRotateType;

typedef enum {
	FIMCCONVERT_FLIP_NONE = 0,
	FIMCCONVERT_FLIP_VERTICAL,
	FIMCCONVERT_FLIP_HORIZONTAL
} FIMCConvertFlipType;

typedef enum {
	FIMCCONVERT_GEM_FOR_SRC = 0,
	FIMCCONVERT_GEM_FOR_DST = 1,
} FimcConvertGemCreateType;

typedef enum {
	FIMCCONVERT_IPP_CTRL_STOP = 0,
	FIMCCONVERT_IPP_CTRL_START = 1,
} FimcConvertIppCtrl;

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE("sink",
								GST_PAD_SINK,
								GST_PAD_ALWAYS,
                                GST_STATIC_CAPS("video/x-raw, " \
                                        "framerate = (fraction) [ 0, MAX ], " \
                                        "width = (int) [ 1, MAX ], " \
                                        "height = (int) [ 1, MAX ]; " \
                                        GST_VIDEO_CAPS_MAKE ("ST12") ";" \
                                        GST_VIDEO_CAPS_MAKE ("SN12") ";" \
                                        GST_VIDEO_CAPS_MAKE ("NV12") ";" \
                                        GST_VIDEO_CAPS_MAKE ("S420") ";" \
                                        GST_VIDEO_CAPS_MAKE ("I420") ";" \
                                        GST_VIDEO_CAPS_MAKE ("SYVY") ";" \
                                        GST_VIDEO_CAPS_MAKE ("UYVY") ";" \
                                        GST_VIDEO_CAPS_MAKE ("SUYV") ";" \
                                        GST_VIDEO_CAPS_MAKE ("YUYV") ";" \
                                        GST_VIDEO_CAPS_MAKE ("SUY2") ";" \
                                        GST_VIDEO_CAPS_MAKE ("YUY2") ";" \
                                        GST_VIDEO_CAPS_MAKE ("ITLV"))
								);

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE("src",
								GST_PAD_SRC,
								GST_PAD_ALWAYS,
                                GST_STATIC_CAPS(GST_VIDEO_CAPS_MAKE("BGRx") ";" \
                                        GST_VIDEO_CAPS_MAKE ("ST12") ";" \
                                        GST_VIDEO_CAPS_MAKE ("SN12") ";" \
                                        GST_VIDEO_CAPS_MAKE ("NV12") ";" \
                                        GST_VIDEO_CAPS_MAKE ("S420") ";" \
                                        GST_VIDEO_CAPS_MAKE ("I420") ";" \
                                        GST_VIDEO_CAPS_MAKE ("SYVY") ";" \
                                        GST_VIDEO_CAPS_MAKE ("UYVY") ";" \
                                        GST_VIDEO_CAPS_MAKE ("SUYV") ";" \
                                        GST_VIDEO_CAPS_MAKE ("YUYV") ";" \
                                        GST_VIDEO_CAPS_MAKE ("SUY2") ";" \
                                        GST_VIDEO_CAPS_MAKE ("YUY2"))
								);

static void gst_fimcconvert_base_init(gpointer g_class);
static void gst_fimcconvert_class_init(GstFimcConvertClass * klass);
static void gst_fimcconvert_init(GstFimcConvert * fimcconvert);
static void gst_fimcconvert_finalize(GstFimcConvert * fimcconvert);
static GstStateChangeReturn gst_fimcconvert_change_state (GstElement *element, GstStateChange transition);
static GstCaps * gst_fimcconvert_transform_caps(GstBaseTransform *trans, GstPadDirection direction, GstCaps *caps, GstCaps *filter);
static GstCaps* gst_fimcconvert_fixate_caps(GstBaseTransform *trans, GstPadDirection direction, GstCaps *caps, GstCaps *othercaps);
static gboolean gst_fimcconvert_set_caps(GstBaseTransform *trans, GstCaps *in, GstCaps *out);
static gboolean gst_fimcconvert_start(GstBaseTransform *trans);
static gboolean gst_fimcconvert_stop(GstBaseTransform *trans);
static gboolean gst_fimcconvert_get_unit_size(GstBaseTransform *trans, GstCaps *caps, guint *size);
static GstFlowReturn gst_fimcconvert_transform(GstBaseTransform *trans, GstBuffer *in, GstBuffer *out);
static void gst_fimcconvert_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void gst_fimcconvert_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static GstFlowReturn gst_fimcconvert_prepare_output_buffer(GstBaseTransform *trans, GstBuffer *input, GstBuffer **buf);
static gboolean gst_fimcconvert_priv_init (GstFimcConvert *fimcconvert);
static void gst_fimcconvert_reset (GstFimcConvert *fimcconvert);
static gboolean gst_fimcconvert_tbm_create_and_map_bo(GstFimcConvert *fimcconvert, FimcConvertGemCreateType type);
static void gst_fimcconvert_tbm_unmap_and_destroy_bo(GstFimcConvert *fimcconvert, FimcConvertGemCreateType type, int index);
static void gst_fimcconvert_buffer_finalize(GstFimcConvertBuffer *buffer);
static gboolean gst_fimcconvert_drm_ipp_init(GstFimcConvert *fimcconvert);
static gboolean gst_fimcconvert_drm_ipp_ctrl(GstFimcConvert *fimcconvert, FimcConvertIppCtrl cmd);
static void gst_fimcconvert_drm_ipp_fini(GstFimcConvert *fimcconvert);

static GstElementClass *parent_class = NULL;

/* for fimc multi instance */
GMutex * instance_lock = NULL;
guint instance_lock_count;

#define MAX_IPP_EVENT_BUFFER_SIZE    1024
#define FIMCCONVERT_TYPE_ROTATE (fimcconvert_rotate_get_type())
#define FIMCCONVERT_TYPE_FLIP (fimcconvert_flip_get_type())

static GType
fimcconvert_rotate_get_type (void)
{
	static GType fimcconvert_rotate_type = 0;
	GST_LOG ("[%d : %s] ENTERING",__LINE__,__func__);

	if (!fimcconvert_rotate_type) {
		static GEnumValue fimcconvert_rotate_types[] = {
			{FIMCCONVERT_ROTATE_NONE, "0",    "No Rotation"},
			{FIMCCONVERT_ROTATE_90,   "90",   "Rotate 90 degrees"},
			{FIMCCONVERT_ROTATE_180,  "180",  "Rotate 180 degrees"},
			{FIMCCONVERT_ROTATE_270,  "270",  "Rotate 270 degrees"},
			{0, NULL, NULL},
		};
		fimcconvert_rotate_type = g_enum_register_static ("FIMCConvertRotateType", fimcconvert_rotate_types);
	}

	GST_LOG ("[%d : %s] LEAVING",__LINE__,__func__);
	return  fimcconvert_rotate_type;
}

static GType
fimcconvert_flip_get_type (void)
{
	static GType fimcconvert_flip_type = 0;
	GST_LOG ("[%d : %s] ENTERING",__LINE__,__func__);

	if (!fimcconvert_flip_type) {
		static GEnumValue fimcconvert_flip_types[] = {
			{FIMCCONVERT_FLIP_NONE,"NONE","No Flip"},
			{FIMCCONVERT_FLIP_VERTICAL,"VERTICAL","Vertical Flip"},
			{FIMCCONVERT_FLIP_HORIZONTAL,"HORIZONTAL","Horizontal Flip"},
			{0, NULL, NULL},
		};
		fimcconvert_flip_type = g_enum_register_static ("FIMCConvertFlipType", fimcconvert_flip_types);
	}

	GST_LOG ("[%d : %s] LEAVING",__LINE__,__func__);
	return  fimcconvert_flip_type;
}

GType
gst_fimcconvert_get_type(void)
{
	static GType fimcconvert_type = 0;

	if (!fimcconvert_type) {
		static const GTypeInfo fimcconvert_info = {
			sizeof(GstFimcConvertClass),
			gst_fimcconvert_base_init,
			NULL,
			(GClassInitFunc) gst_fimcconvert_class_init,
			NULL,
			NULL,
			sizeof(GstFimcConvert),
			0,
			(GInstanceInitFunc) gst_fimcconvert_init,
		};

		fimcconvert_type = g_type_register_static(GST_TYPE_BASE_TRANSFORM, "GstFimcConvert", &fimcconvert_info, 0);
	}
	return fimcconvert_type;
}

static GstFimcConvertBuffer *
gst_fimcconvert_buffer_new(GstFimcConvert *fimcconvert, guint buf_size)
{
	GstFimcConvertBuffer *buffer = NULL;

	buffer = (GstFimcConvertBuffer *)malloc(sizeof(*buffer));

	GST_LOG_OBJECT(fimcconvert, "creating fimcconvert buffer(%p)", buffer);

	buffer->actual_size = buf_size;
	buffer->buffer = gst_buffer_new();
	buffer->fimcconvert = gst_object_ref(GST_OBJECT(fimcconvert));

	return buffer;
}

static void
gst_fimcconvert_buffer_finalize(GstFimcConvertBuffer *buffer)
{
	GstFimcConvert *fimcconvert = NULL;
	SCMN_IMGB *imgb = NULL;
#ifdef USE_DETECT_JPEG
	GstMemory *imgb_memory = NULL;
	GstMapInfo imgb_info = GST_MAP_INFO_INIT;
#endif

	fimcconvert = buffer->fimcconvert;

	GST_DEBUG_OBJECT (fimcconvert, "[START]");

#ifdef DEBUG_FOR_DV
	if (total_converted_frame_count > 0) {
		if (++total_buffer_unref_count <= 10) {
			GST_WARNING_OBJECT (fimcconvert, "%2dth of buffer(%p) unrefering", total_buffer_unref_count, buffer);
		}
	}
#endif

	g_mutex_lock (fimcconvert->buf_idx_lock);
	if (fimcconvert->dst_buf_num > 0) {
		fimcconvert->dst_buf_num--;
		GST_LOG_OBJECT(fimcconvert, "unrefering a buffer(%p), now number of pending buffers(%d)", buffer, fimcconvert->dst_buf_num);
	} else {
		if (fimcconvert->is_drm_inited) {
			GST_WARNING_OBJECT(fimcconvert, "something wrong..trying to unrefer a buffer(%p)", buffer);
		}
	}

#ifdef USE_DETECT_JPEG
	/* free memory for JPEG data */
	if (gst_buffer_n_memory(buffer->buffer) > 2) {
	    imgb_memory = gst_buffer_peek_memory(buffer->buffer, 1);
	    gst_memory_map(imgb_memory, &imgb_info, GST_MAP_READ);
	    imgb = (SCMN_IMGB *)imgb_info.data;
	    gst_memory_unmap(imgb_memory, &imgb_info);
	    if (imgb->jpeg_data) {
	        GST_INFO_OBJECT(fimcconvert, "JPEG data(%p) is detected, free it", imgb->jpeg_data);
	        free(imgb->jpeg_data);
	    }
	}
#endif

	gst_object_unref(fimcconvert);
	free(buffer);

	g_mutex_unlock (fimcconvert->buf_idx_lock);

	GST_DEBUG_OBJECT (fimcconvert, "[END]");
}

static gboolean
gst_fimcconvert_is_buffer_available (GstFimcConvert *fimcconvert)
{
	g_return_val_if_fail (GST_IS_FIMCCONVERT (fimcconvert), FALSE);

	g_mutex_lock (fimcconvert->buf_idx_lock);
	if (fimcconvert->dst_buf_num < fimcconvert->num_of_dst_buffers) {
		fimcconvert->dst_buf_num++;
	} else {
		int timeout_count = 50;
		g_mutex_unlock (fimcconvert->buf_idx_lock);
		/* waiting for unref buffer */
		while (fimcconvert->dst_buf_num == fimcconvert->num_of_dst_buffers) {
			if (timeout_count == 0) {
				GST_ERROR_OBJECT (fimcconvert, "timeout..");
				return FALSE;
			}
			GST_WARNING_OBJECT (fimcconvert, "wait until downstream plugin unrefers a buffer");
			g_usleep (G_USEC_PER_SEC / 10);
			timeout_count--;
		}
		g_mutex_lock (fimcconvert->buf_idx_lock);
		fimcconvert->dst_buf_num++;
	}
	GST_LOG_OBJECT (fimcconvert, "Number of dst buffers(%d), refered number of buffers(%d)", fimcconvert->num_of_dst_buffers, fimcconvert->dst_buf_num);
	g_mutex_unlock (fimcconvert->buf_idx_lock);

	return TRUE;
}

static gboolean
gst_fimcconvert_drm_ipp_init(GstFimcConvert *fimcconvert)
{
	Display *dpy = NULL;
	int i = 0;
	int j = 0;
	int ret = 0;
	int eventBase = 0;
	int errorBase = 0;
	int dri2Major = 0;
	int dri2Minor = 0;
	char *driverName = NULL;
	char *deviceName = NULL;
	struct drm_auth auth_arg = {0};
	struct drm_exynos_ipp_property property;
	struct drm_exynos_sz src_sz;
	struct drm_exynos_sz dst_sz;
	enum drm_exynos_degree dst_degree = EXYNOS_DRM_DEGREE_0;
	enum drm_exynos_flip dst_flip = EXYNOS_DRM_FLIP_NONE;

	if (fimcconvert->drm_fimc_fd != -1) {
		/* maybe skipping convert case */
		GST_WARNING_OBJECT (fimcconvert, "already initialized drm_fd (%d)", fimcconvert->drm_fimc_fd);
		return TRUE;
	}

	g_return_val_if_fail (GST_IS_FIMCCONVERT (fimcconvert), FALSE);

	if (fimcconvert->drm_tbm_fd != -1) {
		fimcconvert->drm_fimc_fd = fimcconvert->drm_tbm_fd;
		GST_WARNING_OBJECT (fimcconvert, "use drm fd(%d) for IPP", fimcconvert->drm_fimc_fd);
	} else {

		dpy = XOpenDisplay(0);
		if (!dpy) {
			GST_ERROR_OBJECT(fimcconvert, "failed to XOpenDisplay()");
			goto ERROR_CASE;
		}

		/* DRI2 */
		if (!DRI2QueryExtension(dpy, &eventBase, &errorBase)) {
			GST_ERROR_OBJECT(fimcconvert, "failed to DRI2QueryExtension()");
			goto ERROR_CASE;
		}

		if (!DRI2QueryVersion(dpy, &dri2Major, &dri2Minor)) {
			GST_ERROR_OBJECT(fimcconvert, "failed to DRI2QueryVersion");
			goto ERROR_CASE;
		}

		if (!DRI2Connect(dpy, RootWindow(dpy, DefaultScreen(dpy)), &driverName, &deviceName)) {
			GST_ERROR_OBJECT(fimcconvert,"failed to DRI2Connect");
			goto ERROR_CASE;
		}

		/* get the drm_fd though opening the deviceName */
		fimcconvert->drm_fimc_fd = open(deviceName, O_RDWR);
		if (fimcconvert->drm_fimc_fd < 0) {
			GST_ERROR_OBJECT(fimcconvert,"cannot open drm device (%s)", deviceName);
			goto ERROR_CASE;
		}
		GST_INFO("Open drm device : %s, fd(%d)", deviceName, fimcconvert->drm_fimc_fd);

		/* get magic from drm to authentication */
		if (ioctl(fimcconvert->drm_fimc_fd, DRM_IOCTL_GET_MAGIC, &auth_arg)) {
			GST_ERROR_OBJECT(fimcconvert,"cannot get drm auth magic");
			close(fimcconvert->drm_fimc_fd);
			fimcconvert->drm_fimc_fd = -1;
			goto ERROR_CASE;
		}

		if (!DRI2Authenticate(dpy, RootWindow(dpy, DefaultScreen(dpy)), auth_arg.magic)) {
			GST_ERROR_OBJECT(fimcconvert,"cannot get drm authentication from X");
			close(fimcconvert->drm_fimc_fd);
			fimcconvert->drm_fimc_fd = -1;
			goto ERROR_CASE;
		}

		fimcconvert->drm_tbm_fd = fimcconvert->drm_fimc_fd;
	}

	fimcconvert->tbm = tbm_bufmgr_init (fimcconvert->drm_tbm_fd);
	if (!fimcconvert->tbm) {
		GST_ERROR_OBJECT (fimcconvert, "failed to initialize tizen buffer manager ");
		goto ERROR_CASE;
	} else {
		GST_INFO_OBJECT (fimcconvert, "tbm bufmgr(%x) initialized : using drm fd(%d)", fimcconvert->tbm, fimcconvert->drm_tbm_fd);
	}

	for (i=0; i<MAX_SRC_BUF_NUM; i++) {
		for (j=0; j<SCMN_IMGB_MAX_PLANE; j++) {
			memset(&fimcconvert->gem_info_src[i][j], 0x0, sizeof(GemInfoSrc));
		}
	}
	for (i=0; i<MAX_SRC_BUF_NUM; i++) {
		memset(&fimcconvert->gem_info_src_normal_fmt[i], 0x0, sizeof(GemInfoDst));
	}
	for (i=0; i<MAX_DST_BUF_NUM; i++) {
		for (j=0; j<SCMN_IMGB_MAX_PLANE; j++) {
			memset(&fimcconvert->gem_info_dst[i][j], 0x0, sizeof(GemInfoDst));
		}
	}
	fimcconvert->ipp_prop_id = 0;

	/* Update src/dst size */
	src_sz.hsize = fimcconvert->src_caps_width;
	src_sz.vsize = fimcconvert->src_caps_height;
	dst_sz.hsize = fimcconvert->dst_caps_width;
	dst_sz.vsize = fimcconvert->dst_caps_height;

	/* Update rotation information */
	switch (fimcconvert->rotate_ang) {
	case 90:
		dst_degree = EXYNOS_DRM_DEGREE_90;
		break;
	case 270:
		dst_degree = EXYNOS_DRM_DEGREE_270;
		break;
	case 0:
		dst_degree = EXYNOS_DRM_DEGREE_0;
		break;
	case 180:
		dst_degree = EXYNOS_DRM_DEGREE_180;
		break;
	default:
		break;
	}
	GST_INFO_OBJECT (fimcconvert, "set rotate angle to %d", dst_degree);

	/* Update flip information */
	switch (fimcconvert->dst_flip) {
	case 0:
		dst_flip = EXYNOS_DRM_FLIP_NONE;
		break;
	case 1:
		dst_flip = EXYNOS_DRM_FLIP_VERTICAL;
		break;
	case 2:
		dst_flip = EXYNOS_DRM_FLIP_HORIZONTAL;
		break;
	default:
		break;
	}
	GST_INFO_OBJECT (fimcconvert, "set flip to %d", dst_flip);

	/* Set DRM property, IPP_CMD_M2M */
	ret = exynos_drm_ipp_set_property(fimcconvert->drm_fimc_fd, &property, &src_sz, &dst_sz, fimcconvert->src_format_drm, fimcconvert->dst_format_drm, IPP_CMD_M2M, dst_degree, dst_flip);
	if (ret) {
		GST_ERROR_OBJECT (fimcconvert, "failed to set ipp property");
		gst_fimcconvert_drm_ipp_fini (fimcconvert);
		goto ERROR_CASE;
	} else {
		fimcconvert->ipp_prop_id = property.prop_id;
		GST_INFO_OBJECT (fimcconvert, "set IPP_CMD_M2M, ipp_prop_id(%d)", fimcconvert->ipp_prop_id);
	}
#if 0
	/* Start */
	if (!gst_fimcconvert_drm_ipp_ctrl(fimcconvert, FIMCCONVERT_IPP_CTRL_START)) {
		GST_ERROR_OBJECT (fimcconvert, "failed to gst_fimcconvert_drm_ipp_ctrl(START)");
	}
#endif
	fimcconvert->is_drm_inited = FALSE;

	if (dpy) {
		XCloseDisplay(dpy);
	}
	if (driverName) {
		free(driverName);
	}
	if (deviceName) {
		free(deviceName);
	}

	return TRUE;

ERROR_CASE:
	if (dpy) {
		XCloseDisplay(dpy);
	}
	if (driverName) {
		free(driverName);
	}
	if (deviceName) {
		free(deviceName);
	}

	return FALSE;
}

static gboolean
gst_fimcconvert_drm_ipp_ctrl(GstFimcConvert *fimcconvert, FimcConvertIppCtrl cmd)
{
	int ret = 0;
	enum drm_exynos_ipp_ctrl ctrl_cmd;

	g_return_val_if_fail (GST_IS_FIMCCONVERT (fimcconvert), FALSE);

	if (fimcconvert->drm_fimc_fd < 0) {
		GST_ERROR_OBJECT(fimcconvert, "already closed drm_fimc_fd(%d)", fimcconvert->drm_fimc_fd);
		return FALSE;
	}
	if (fimcconvert->ipp_prop_id == 0) {
		GST_ERROR_OBJECT(fimcconvert, "ipp_prop_id is 0..");
		return FALSE;
	}

	switch(cmd) {
	case FIMCCONVERT_IPP_CTRL_STOP:
		ctrl_cmd = IPP_CTRL_STOP;
		break;
	case FIMCCONVERT_IPP_CTRL_START:
		ctrl_cmd = IPP_CTRL_PLAY;
		break;
	default:
		GST_ERROR_OBJECT(fimcconvert, "cmd is not valid(%d)", cmd);
		return FALSE;
	}

	ret = exynos_drm_ipp_cmd_ctrl(fimcconvert->drm_fimc_fd, fimcconvert->ipp_prop_id, ctrl_cmd);
	if (ret) {
		GST_ERROR_OBJECT (fimcconvert, "failed to stop ipp ctrl IPP_CMD_M2M");
		return FALSE;
	}

	return TRUE;
}

static void
gst_fimcconvert_drm_ipp_fini(GstFimcConvert *fimcconvert)
{
	int i = 0;
	int ret = 0;

	g_return_if_fail (GST_IS_FIMCCONVERT (fimcconvert));

	if (fimcconvert->drm_fimc_fd < 0) {
		GST_WARNING_OBJECT(fimcconvert, "already closed drm_fimc_fd(%d)", fimcconvert->drm_fimc_fd);
		return;

	} else if (fimcconvert->drm_fimc_fd >= 0) {
		if (fimcconvert->is_drm_inited) {
			/* For source buffer dequeue to IPP */
			for (i = 0; i < MAX_SRC_BUF_NUM; i++) {
				if (fimcconvert->gem_info_src[i][SCMN_Y_PLANE].gem_handle) {
					ret = exynos_drm_ipp_queue_buf(fimcconvert->drm_fimc_fd, EXYNOS_DRM_OPS_SRC, IPP_BUF_DEQUEUE, fimcconvert->ipp_prop_id, i,
						fimcconvert->gem_info_src[i][SCMN_Y_PLANE].gem_handle, fimcconvert->gem_info_src[i][SCMN_CB_PLANE].gem_handle, fimcconvert->gem_info_src[i][SCMN_CR_PLANE].gem_handle);
					if (ret < 0) {
						GST_ERROR_OBJECT (fimcconvert, "src buffer map to IPP failed, src_buf_idx(%d)", i);
					}
				}
			}
			/* For destination buffer dequeue to IPP */
			for (i = 0; i < fimcconvert->num_of_dst_buffers; i++) {
				if (fimcconvert->gem_info_dst[i][SCMN_Y_PLANE].gem_handle) {
					ret = exynos_drm_ipp_queue_buf(fimcconvert->drm_fimc_fd, EXYNOS_DRM_OPS_DST, IPP_BUF_DEQUEUE, fimcconvert->ipp_prop_id, i,
						fimcconvert->gem_info_dst[i][SCMN_Y_PLANE].gem_handle, fimcconvert->gem_info_dst[i][SCMN_CB_PLANE].gem_handle, fimcconvert->gem_info_dst[i][SCMN_CR_PLANE].gem_handle);
					if (ret < 0) {
						GST_ERROR_OBJECT (fimcconvert, "dst buffer map to IPP failed, dst_buf_idx(%d)", i);
					}
				}
			}
			/* Set IPP to stop */
			if (!gst_fimcconvert_drm_ipp_ctrl(fimcconvert, FIMCCONVERT_IPP_CTRL_STOP)) {
				GST_ERROR_OBJECT (fimcconvert, "failed to gst_fimcconvert_drm_ipp_ctrl(STOP)");
			}
			/* Memory unmap & close gem */
			/* dst buffer */
			for (i = 0; i < fimcconvert->num_of_dst_buffers; i++) {
				gst_fimcconvert_tbm_unmap_and_destroy_bo (fimcconvert, FIMCCONVERT_GEM_FOR_DST, i);
			}
			/* src buffer */
			for (i = 0; i < MAX_SRC_BUF_NUM; i++) {
				gst_fimcconvert_tbm_unmap_and_destroy_bo (fimcconvert, FIMCCONVERT_GEM_FOR_SRC, i);
			}
		}

		/* Deinitialize TBM */
		if (fimcconvert->tbm) {
			tbm_bufmgr_deinit(fimcconvert->tbm);
		}

		/* Close DRM */
		if (fimcconvert->drm_tbm_fd != fimcconvert->drm_fimc_fd) {
			GST_INFO_OBJECT (fimcconvert, "close drm_fimc_fd(%d)", fimcconvert->drm_fimc_fd);
			close(fimcconvert->drm_fimc_fd);
		}

		fimcconvert->drm_fimc_fd = -1;
		fimcconvert->ipp_prop_id = 0;
		fimcconvert->tbm = 0;
		fimcconvert->drm_tbm_fd = -1;
		fimcconvert->is_drm_inited = FALSE;

		return;
	}
}

static void
gst_fimcconvert_base_init(gpointer g_class)
{
}

static void
gst_fimcconvert_class_init(GstFimcConvertClass *klass)
{
	GObjectClass *gobject_class;
	GstBaseTransformClass *trans_class;
	GstElementClass *gstelement_class = GST_ELEMENT_CLASS(klass);

	gobject_class =(GObjectClass *) klass;
	trans_class =(GstBaseTransformClass *) klass;

	gobject_class->finalize =(GObjectFinalizeFunc) gst_fimcconvert_finalize;
	gobject_class->set_property = gst_fimcconvert_set_property;
	gobject_class->get_property = gst_fimcconvert_get_property;

	gstelement_class->change_state = GST_DEBUG_FUNCPTR(gst_fimcconvert_change_state);
    gst_element_class_set_static_metadata(gstelement_class,
                                          "Video Scale, Rotate, Colorspace Convert Plug-in based on FIMC",
                                          "Filter/Effect/Video",
                                          "Video scale, rotate and colorspace convert using FIMC",
                                          "Samsung Electronics Co");
    gst_element_class_add_pad_template(gstelement_class, gst_static_pad_template_get(&src_template));
    gst_element_class_add_pad_template(gstelement_class, gst_static_pad_template_get(&sink_template));

	trans_class->transform_caps = GST_DEBUG_FUNCPTR(gst_fimcconvert_transform_caps);
	trans_class->fixate_caps = GST_DEBUG_FUNCPTR(gst_fimcconvert_fixate_caps);
	trans_class->set_caps = GST_DEBUG_FUNCPTR(gst_fimcconvert_set_caps);
	trans_class->start = GST_DEBUG_FUNCPTR(gst_fimcconvert_start);
	trans_class->stop = GST_DEBUG_FUNCPTR(gst_fimcconvert_stop);
	trans_class->get_unit_size = GST_DEBUG_FUNCPTR(gst_fimcconvert_get_unit_size);

	trans_class->transform = GST_DEBUG_FUNCPTR(gst_fimcconvert_transform);
	trans_class->prepare_output_buffer = GST_DEBUG_FUNCPTR(gst_fimcconvert_prepare_output_buffer);

	trans_class->passthrough_on_same_caps = TRUE;

	parent_class = g_type_class_peek_parent(klass);

	GST_DEBUG_CATEGORY_INIT(fimcconvert_debug, "fimcconvert", 0, "fimcconvert element");

	g_object_class_install_property (gobject_class, PROP_ROT_ANG,
			g_param_spec_enum ("rotate", "Rotate", "Rotate can be 0, 90, 180 or 270",
					FIMCCONVERT_TYPE_ROTATE, FIMCCONVERT_ROTATE_NONE, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_DST_WIDTH,
			g_param_spec_int ("dst-width", "Output video width", "Output video width. If setting 0, output video width will be media src's width",
					FIMCCONVERT_DST_WIDTH_MIN, FIMCCONVERT_DST_WIDTH_MAX, FIMCCONVERT_DST_WIDTH_DEFAULT, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_DST_HEIGHT,
			g_param_spec_int ("dst-height", "Output video height", "Output video height. If setting 0, output video height will be media src's height",
					FIMCCONVERT_DST_HEIGHT_MIN, FIMCCONVERT_DST_HEIGHT_MAX, FIMCCONVERT_DST_HEIGHT_DEFAULT, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_FLIP,
			 g_param_spec_enum ("flip", "Flip", "Flip can be 0, 1, 2",
					FIMCCONVERT_TYPE_FLIP, FIMCCONVERT_FLIP_NONE, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_SRC_RANDOM_IDX,
			g_param_spec_boolean ("src-rand-idx", "Random src buffer index", "Random src buffer index. If setting TRUE, fimcconvert does not manage index of src buffer from upstream src-plugin",
					TRUE,	G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_DST_BUFFER_NUM,
			g_param_spec_int ("dst-buffer-num", "Number of dst buffers", "Number of destination buffers",
					FIMCCONVERT_DST_BUFFER_MIN, FIMCCONVERT_DST_BUFFER_MAX, FIMCCONVERT_DST_BUFFER_DEFAULT, G_PARAM_READWRITE));
}

static void
gst_fimcconvert_init(GstFimcConvert * fimcconvert)
{
	GST_DEBUG_OBJECT (fimcconvert, "[START]");
	gst_base_transform_set_qos_enabled(GST_BASE_TRANSFORM(fimcconvert), TRUE);

	/* pad setting */
	fimcconvert->src_pad = GST_BASE_TRANSFORM_SRC_PAD(fimcconvert);

	/* private values init */
	gst_fimcconvert_priv_init (fimcconvert);

	/* property values init */
	fimcconvert->is_random_src_idx = TRUE;
	fimcconvert->num_of_dst_buffers = FIMCCONVERT_DST_BUFFER_DEFAULT;
	fimcconvert->rotate_ang = FIMCCONVERT_ROTATE_NONE;
	fimcconvert->dst_flip = FIMCCONVERT_FLIP_NONE;

	if(!instance_lock) {
		instance_lock = g_mutex_new();
	}
	g_mutex_lock (instance_lock);
	instance_lock_count++;
	g_mutex_unlock (instance_lock);

	fimcconvert->fd_lock = g_mutex_new ();
	if (!fimcconvert->fd_lock) {
		GST_ERROR ("Failed to created mutex...");
	}
	fimcconvert->buf_idx_lock = g_mutex_new ();
	if (!fimcconvert->buf_idx_lock) {
		GST_ERROR ("Failed to created mutex...");
	}


#ifdef DEBUG_FOR_DV
	GST_WARNING_OBJECT (fimcconvert, "[END]");
#else
	GST_DEBUG_OBJECT (fimcconvert, "[END]");
#endif
}

static gboolean
gst_fimcconvert_priv_init (GstFimcConvert *fimcconvert)
{
	g_return_val_if_fail (GST_IS_FIMCCONVERT (fimcconvert), FALSE);

	fimcconvert->drm_fimc_fd = -1;
	fimcconvert->drm_tbm_fd = -1;

	fimcconvert->src_buf_idx = 0;
	fimcconvert->dst_buf_idx = 0;

	fimcconvert->is_first_set_for_src = TRUE;
	fimcconvert->is_first_set_for_dst = TRUE;

	fimcconvert->buf_share_method_type = 0;

	fimcconvert->is_src_format_std = FALSE;
	fimcconvert->is_same_caps = FALSE;
	fimcconvert->do_skip = FALSE;

	fimcconvert->jpeg_data = NULL;
	fimcconvert->jpeg_data_size = 0;

	return TRUE;
}

static void
gst_fimcconvert_finalize(GstFimcConvert *fimcconvert)
{
	g_return_if_fail (GST_IS_FIMCCONVERT (fimcconvert));
	GST_DEBUG_OBJECT (fimcconvert, "[START]");

	if (fimcconvert->is_drm_inited) {
		gst_fimcconvert_reset(fimcconvert);
	}

	if (fimcconvert->fd_lock) {
		g_mutex_free (fimcconvert->fd_lock);
		fimcconvert->fd_lock = NULL;
	}
	if (fimcconvert->buf_idx_lock) {
		g_mutex_free (fimcconvert->buf_idx_lock);
		fimcconvert->buf_idx_lock = NULL;
	}

	g_mutex_lock (instance_lock);
	instance_lock_count--;
	g_mutex_unlock (instance_lock);
	if (instance_lock_count == 0) {
		g_mutex_free (instance_lock);
		instance_lock = NULL;
	}

#ifdef DEBUG_FOR_DV
	GST_WARNING_OBJECT (fimcconvert, "[END] total number of converted frames : %d",total_converted_frame_count);
	GST_WARNING_OBJECT (fimcconvert, "[END] total number of unrefered buffers : %d",total_buffer_unref_count);
	total_converted_frame_count = 0;
	total_buffer_unref_count = 0;
#else
	GST_DEBUG_OBJECT (fimcconvert, "[END]");
#endif

	G_OBJECT_CLASS(parent_class)->finalize(G_OBJECT(fimcconvert));

}

static void
gst_fimcconvert_reset (GstFimcConvert *fimcconvert)
{
	g_return_if_fail (GST_IS_FIMCCONVERT (fimcconvert));

	GST_DEBUG_OBJECT (fimcconvert, "[START]");

	g_mutex_lock (fimcconvert->fd_lock);
	g_mutex_lock (instance_lock);

	if (fimcconvert->drm_fimc_fd >= 0) {
		FIMCCONVERT_SEND_CUSTOM_EVENT_TO_SINK_WITH_DATA( 0 );
		gst_fimcconvert_drm_ipp_fini(fimcconvert);
		gst_fimcconvert_priv_init (fimcconvert);
	}

	g_mutex_unlock (instance_lock);
	g_mutex_unlock (fimcconvert->fd_lock);

	GST_DEBUG_OBJECT (fimcconvert, "[END]");

	return;
}

static GstStateChangeReturn
gst_fimcconvert_change_state (GstElement *element, GstStateChange transition)
{
	GstFimcConvert *fimcconvert = NULL;
	GstStateChangeReturn stateret = GST_STATE_CHANGE_SUCCESS;

	fimcconvert = GST_FIMCCONVERT(element);

	switch (transition) {
	case GST_STATE_CHANGE_NULL_TO_READY:
		GST_INFO_OBJECT( fimcconvert, "GST_STATE_CHANGE_NULL_TO_READY");
		break;
	case GST_STATE_CHANGE_READY_TO_PAUSED:
		GST_INFO_OBJECT( fimcconvert, "GST_STATE_CHANGE_READY_TO_PAUSED");
		break;
	case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
		GST_INFO_OBJECT( fimcconvert, "GST_STATE_CHANGE_PAUSED_TO_PLAYING");
		break;
	default:
		break;
	}
	stateret = parent_class->change_state (element, transition);
	if ( stateret != GST_STATE_CHANGE_SUCCESS ) {
		GST_ERROR_OBJECT( fimcconvert, "chane state error in parent class");
		return GST_STATE_CHANGE_FAILURE;
	}

	switch (transition) {
	case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
		GST_INFO_OBJECT( fimcconvert, "GST_STATE_CHANGE_PLAYING_TO_PAUSED");
		break;
	case GST_STATE_CHANGE_PAUSED_TO_READY:
		GST_INFO_OBJECT( fimcconvert, "GST_STATE_CHANGE_PAUSED_TO_READY");
		break;
	case GST_STATE_CHANGE_READY_TO_NULL:
		GST_INFO_OBJECT( fimcconvert, "GST_STATE_CHANGE_READY_TO_NULL");
		gst_fimcconvert_reset(fimcconvert);
		break;
	default:
		break;
	}

	return stateret;
}

static void
gst_fimcconvert_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
 	GstFimcConvert *fimcconvert = GST_FIMCCONVERT (object);
	guint rotate_ang;
	gint width;
	gint height;
	gint flip;
	gboolean is_rand_src_idx;
	guint dst_buffer_num;

	switch (prop_id) {
	case PROP_ROT_ANG:
		rotate_ang = g_value_get_enum(value);
		if(rotate_ang != fimcconvert->rotate_ang) {
			fimcconvert->rotate_ang = rotate_ang;
			GST_INFO_OBJECT (fimcconvert, "Rotate angle value = %d", rotate_ang);
			gst_fimcconvert_reset(fimcconvert);
		}
		break;

	case PROP_DST_WIDTH:
		width = g_value_get_int(value);
		if (width != fimcconvert->dst_width) {
			if (!width) {
				fimcconvert->dst_width = fimcconvert->src_caps_width;
			} else {
				fimcconvert->dst_width = width;
			}
			GST_INFO_OBJECT (fimcconvert, "Going to set output width = %d", fimcconvert->dst_width);
			fimcconvert->dst_caps_width = fimcconvert->dst_width;
			gst_fimcconvert_reset(fimcconvert);
		} else {
			GST_WARNING_OBJECT (fimcconvert, "skip set width.. width(%d),fimcconvert->dst_width(%d)",width,fimcconvert->dst_width);
		}
		break;

	case PROP_DST_HEIGHT:
		height = g_value_get_int(value);
		if (height != fimcconvert->dst_height) {
			if (!height) {
				fimcconvert->dst_height = fimcconvert->src_caps_height;
			} else {
				fimcconvert->dst_height = height;
			}
			GST_INFO_OBJECT (fimcconvert, "Going to set output height = %d", fimcconvert->dst_height);
			fimcconvert->dst_caps_height = fimcconvert->dst_height;
			gst_fimcconvert_reset(fimcconvert);
		} else {
			GST_WARNING_OBJECT (fimcconvert, "skip set height.. height(%d),fimcconvert->dst_height(%d)",height,fimcconvert->dst_height);
		}
		break;

	case PROP_FLIP:
		flip = g_value_get_enum(value);
		if (flip != fimcconvert->dst_flip) {
			fimcconvert->dst_flip = flip;
			GST_INFO_OBJECT (fimcconvert, "Going to set flip = %d", fimcconvert->dst_flip);
			gst_fimcconvert_reset(fimcconvert);
		}
		break;

	case PROP_SRC_RANDOM_IDX:
		is_rand_src_idx = g_value_get_boolean(value);
		if (fimcconvert->is_random_src_idx != is_rand_src_idx) {
			fimcconvert->is_random_src_idx = is_rand_src_idx;
			GST_INFO_OBJECT (fimcconvert, "src index ordering from upstream src plugin is random? %d", fimcconvert->is_random_src_idx);
			gst_fimcconvert_reset(fimcconvert);
		}
		break;

	case PROP_DST_BUFFER_NUM:
		dst_buffer_num = (guint)g_value_get_int(value);
		if (fimcconvert->num_of_dst_buffers != dst_buffer_num) {
			fimcconvert->num_of_dst_buffers = dst_buffer_num;
			GST_INFO_OBJECT (fimcconvert, "Number of dst buffers = %d", dst_buffer_num);
			gst_fimcconvert_reset(fimcconvert);
		}
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gst_fimcconvert_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GstFimcConvert *fimcconvert = GST_FIMCCONVERT (object);

	switch (prop_id) {
	case PROP_ROT_ANG:
		g_value_set_enum(value, fimcconvert->rotate_ang);
		break;
	case PROP_DST_WIDTH:
		g_value_set_int(value, fimcconvert->dst_width);
		break;
	case PROP_DST_HEIGHT:
		g_value_set_int(value, fimcconvert->dst_height);
		break;
	case PROP_FLIP:
		g_value_set_enum(value, fimcconvert->dst_flip);
		break;
	case PROP_SRC_RANDOM_IDX:
		g_value_set_boolean(value, fimcconvert->is_random_src_idx);
		break;
	case PROP_DST_BUFFER_NUM:
		g_value_set_int(value, (int)fimcconvert->num_of_dst_buffers);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static gboolean
gst_fimcconvert_start(GstBaseTransform *trans)
{
	/* Not doing anything here */
	return TRUE;
}

static gboolean
gst_fimcconvert_stop(GstBaseTransform *trans)
{
	/* Not doing anything here */
	return TRUE;
}

static gboolean
gst_fimcconvert_set_imgb_info_for_dst(GstFimcConvert *fimcconvert, GstBuffer *buf)
{
	SCMN_IMGB *imgb = NULL;
	guint idx = 0;

	g_return_val_if_fail (GST_IS_FIMCCONVERT (fimcconvert), FALSE);
	g_return_val_if_fail (buf, FALSE);

	idx = fimcconvert->dst_buf_idx;

	g_return_val_if_fail (fimcconvert->gem_info_dst[idx][SCMN_Y_PLANE].gem_handle, FALSE);
	g_return_val_if_fail (fimcconvert->gem_info_dst[idx][SCMN_Y_PLANE].usr_addr, FALSE);

	imgb = (SCMN_IMGB *)malloc(sizeof(SCMN_IMGB));
	if (imgb == NULL) {
		GST_ERROR_OBJECT (fimcconvert, "SCMN_IMGB memory allocation failed");
		return FALSE;
	}
	memset(imgb, 0x0, sizeof(SCMN_IMGB));

	GST_INFO_OBJECT (fimcconvert, "OutBuf :: buf_share_method_type (%d)", fimcconvert->buf_share_method_type);

	if (fimcconvert->buf_share_method_type == BUF_SHARE_METHOD_PADDR) {

		if (!fimcconvert->gem_info_dst[idx][SCMN_Y_PLANE].phy_addr) {
			free(imgb);
			GST_ERROR_OBJECT (fimcconvert, "OutBuf :: phy_addr is null");
			return FALSE;
		}

		imgb->p[SCMN_Y_PLANE] = (void*)(unsigned long)fimcconvert->gem_info_dst[idx][SCMN_Y_PLANE].phy_addr;
		imgb->a[SCMN_Y_PLANE] = (void*)(unsigned long)fimcconvert->gem_info_dst[idx][SCMN_Y_PLANE].usr_addr;
		imgb->w[SCMN_Y_PLANE] = fimcconvert->dst_caps_width;
		imgb->h[SCMN_Y_PLANE] = fimcconvert->dst_caps_height;
		imgb->s[SCMN_Y_PLANE] = imgb->w[SCMN_Y_PLANE];
		imgb->e[SCMN_Y_PLANE] = imgb->h[SCMN_Y_PLANE];
		imgb->buf_share_method = BUF_SHARE_METHOD_PADDR;

		if (fimcconvert->dst_format_gst == GST_MAKE_FOURCC('S','T','1','2')
			|| fimcconvert->dst_format_gst == GST_MAKE_FOURCC('S','N','1','2')) {
			if (fimcconvert->dst_format_gst == GST_MAKE_FOURCC('S','N','1','2')) {
				GST_INFO_OBJECT (fimcconvert, "OutBuf :: It's SN12");
				imgb->cs = SCMN_CS_NV12;
			} else {
				GST_INFO_OBJECT (fimcconvert, "OutBuf :: It's ST12");
				imgb->cs = SCMN_CS_NV12_T64X32;
			}
			imgb->p[SCMN_CB_PLANE] = (void*)(unsigned long)fimcconvert->gem_info_dst[idx][SCMN_CB_PLANE].phy_addr;
			imgb->a[SCMN_CB_PLANE] = (void*)(unsigned long)fimcconvert->gem_info_dst[idx][SCMN_CB_PLANE].usr_addr;
			imgb->w[SCMN_CB_PLANE] = fimcconvert->dst_caps_width;
			imgb->h[SCMN_CB_PLANE] = fimcconvert->dst_caps_height >> 1;
			imgb->s[SCMN_CB_PLANE] = imgb->w[SCMN_CB_PLANE];
			imgb->e[SCMN_CB_PLANE] = imgb->h[SCMN_CB_PLANE];
		} else if (fimcconvert->dst_format_gst == GST_MAKE_FOURCC('S','4','2','0')) {
			GST_INFO_OBJECT (fimcconvert, "OutBuf :: It's S420");
			imgb->cs = SCMN_CS_I420;
			imgb->p[SCMN_CB_PLANE] = (void*)(unsigned long)fimcconvert->gem_info_dst[idx][SCMN_CB_PLANE].phy_addr;
			imgb->a[SCMN_CB_PLANE] = (void*)(unsigned long)fimcconvert->gem_info_dst[idx][SCMN_CB_PLANE].usr_addr;
			imgb->p[SCMN_CR_PLANE] = (void*)(unsigned long)fimcconvert->gem_info_dst[idx][SCMN_CR_PLANE].phy_addr;
			imgb->a[SCMN_CR_PLANE] = (void*)(unsigned long)fimcconvert->gem_info_dst[idx][SCMN_CR_PLANE].usr_addr;
			imgb->w[SCMN_CB_PLANE] = imgb->w[SCMN_CR_PLANE] = fimcconvert->dst_caps_width;
			imgb->h[SCMN_CB_PLANE] = imgb->h[SCMN_CR_PLANE] = fimcconvert->dst_caps_height >> 2;
			imgb->s[SCMN_CB_PLANE] = imgb->w[SCMN_CB_PLANE];
			imgb->e[SCMN_CB_PLANE] = imgb->h[SCMN_CB_PLANE];
			imgb->s[SCMN_CR_PLANE] = imgb->w[SCMN_CR_PLANE];
			imgb->e[SCMN_CR_PLANE] = imgb->h[SCMN_CR_PLANE];
		} else if (fimcconvert->dst_format_gst == GST_MAKE_FOURCC('S','U','Y','V')
			|| fimcconvert->dst_format_gst == GST_MAKE_FOURCC('S','U','Y','2') ) {
			GST_INFO_OBJECT (fimcconvert, "OutBuf :: It's SUYV, SUY2");
			imgb->cs = SCMN_CS_YUYV;
		} else if (fimcconvert->dst_format_gst == GST_MAKE_FOURCC('S','Y','V','Y')) {
			GST_INFO_OBJECT (fimcconvert, "OutBuf :: It's SYVY");
			imgb->cs = SCMN_CS_UYVY;
		} else {
			free(imgb);
			GST_ERROR_OBJECT (fimcconvert, "OutBuf :: can not support this format (%d)",fimcconvert->dst_format_gst);
			return FALSE;
		}
		GST_DEBUG_OBJECT (fimcconvert, "OutBuf :: y_phy_addr(0x%x), cb_phy_addr(0x%x), cr_phy_addr(0x%x)",imgb->p[0],imgb->p[1],imgb->p[2]);

	} else if (fimcconvert->buf_share_method_type == BUF_SHARE_METHOD_FD ||
				fimcconvert->buf_share_method_type == BUF_SHARE_METHOD_TIZEN_BUFFER) {

		if (fimcconvert->buf_share_method_type == BUF_SHARE_METHOD_FD) {
			imgb->dma_buf_fd[SCMN_Y_PLANE] = fimcconvert->gem_info_dst[idx][SCMN_Y_PLANE].dma_buf_fd;
		} else {
			imgb->bo[SCMN_Y_PLANE] = fimcconvert->gem_info_dst[idx][SCMN_Y_PLANE].bo;
		}
		imgb->a[SCMN_Y_PLANE] = (void*)(unsigned long)fimcconvert->gem_info_dst[idx][SCMN_Y_PLANE].usr_addr;
		imgb->w[SCMN_Y_PLANE] = fimcconvert->dst_caps_width;
		imgb->h[SCMN_Y_PLANE] = fimcconvert->dst_caps_height;
		imgb->s[SCMN_Y_PLANE] = imgb->w[SCMN_Y_PLANE];
		imgb->e[SCMN_Y_PLANE] = imgb->h[SCMN_Y_PLANE];
		imgb->buf_share_method = fimcconvert->buf_share_method_type;

		if (fimcconvert->dst_format_gst == GST_MAKE_FOURCC('S','T','1','2')
		|| fimcconvert->dst_format_gst == GST_MAKE_FOURCC('S','N','1','2')) {
			if (fimcconvert->buf_share_method_type == BUF_SHARE_METHOD_FD) {
				imgb->dma_buf_fd[SCMN_CB_PLANE] = fimcconvert->gem_info_dst[idx][SCMN_CB_PLANE].dma_buf_fd;
			} else {
				imgb->bo[SCMN_CB_PLANE] = fimcconvert->gem_info_dst[idx][SCMN_CB_PLANE].bo;
			}
			if (fimcconvert->dst_format_gst == GST_MAKE_FOURCC('S','N','1','2')) {
				GST_INFO_OBJECT (fimcconvert, "OutBuf :: It's SN12");
				imgb->cs = SCMN_CS_NV12;
			} else {
				GST_INFO_OBJECT (fimcconvert, "OutBuf :: It's ST12");
				imgb->cs = SCMN_CS_NV12_T64X32;
			}
			imgb->a[SCMN_CB_PLANE] = (void*)(unsigned long)fimcconvert->gem_info_dst[idx][SCMN_CB_PLANE].usr_addr;
			imgb->w[SCMN_CB_PLANE] = fimcconvert->dst_caps_width;
			imgb->h[SCMN_CB_PLANE] = fimcconvert->dst_caps_height >> 1;
			imgb->s[SCMN_CB_PLANE] = imgb->w[SCMN_CB_PLANE];
			imgb->e[SCMN_CB_PLANE] = imgb->h[SCMN_CB_PLANE];
		} else if (fimcconvert->dst_format_gst == GST_MAKE_FOURCC('S','4','2','0')) {
			GST_INFO_OBJECT (fimcconvert, "OutBuf :: It's S420");
			if (fimcconvert->buf_share_method_type == BUF_SHARE_METHOD_FD) {
				imgb->dma_buf_fd[SCMN_CB_PLANE] = fimcconvert->gem_info_dst[idx][SCMN_CB_PLANE].dma_buf_fd;
				imgb->dma_buf_fd[SCMN_CR_PLANE] = fimcconvert->gem_info_dst[idx][SCMN_CR_PLANE].dma_buf_fd;
			} else {
				imgb->bo[SCMN_CB_PLANE] = fimcconvert->gem_info_dst[idx][SCMN_CB_PLANE].bo;
				imgb->bo[SCMN_CR_PLANE] = fimcconvert->gem_info_dst[idx][SCMN_CR_PLANE].bo;
			}
			imgb->cs = SCMN_CS_I420;
			imgb->a[SCMN_CB_PLANE] = (void*)(unsigned long)fimcconvert->gem_info_dst[idx][SCMN_CB_PLANE].usr_addr;
			imgb->a[SCMN_CR_PLANE] = (void*)(unsigned long)fimcconvert->gem_info_dst[idx][SCMN_CR_PLANE].usr_addr;
			imgb->w[SCMN_CB_PLANE] = imgb->w[SCMN_CR_PLANE] = fimcconvert->dst_caps_width;
			imgb->h[SCMN_CB_PLANE] = imgb->h[SCMN_CR_PLANE] = fimcconvert->dst_caps_height >> 2;
			imgb->s[SCMN_CB_PLANE] = imgb->w[SCMN_CB_PLANE];
			imgb->e[SCMN_CB_PLANE] = imgb->h[SCMN_CB_PLANE];
			imgb->s[SCMN_CR_PLANE] = imgb->w[SCMN_CR_PLANE];
			imgb->e[SCMN_CR_PLANE] = imgb->h[SCMN_CR_PLANE];
		} else if (fimcconvert->dst_format_gst == GST_MAKE_FOURCC('S','U','Y','V')
			|| fimcconvert->dst_format_gst == GST_MAKE_FOURCC('S','U','Y','2') ) {
			GST_INFO_OBJECT (fimcconvert, "OutBuf :: It's SUYV, SUY2");
			imgb->cs = SCMN_CS_YUYV;
		} else if (fimcconvert->dst_format_gst == GST_MAKE_FOURCC('S','Y','V','Y')) {
			GST_INFO_OBJECT (fimcconvert, "OutBuf :: It's SYVY");
			imgb->cs = SCMN_CS_UYVY;
		} else if (fimcconvert->dst_format_gst == GST_MAKE_FOURCC('S','R','3','2')) {
			GST_INFO_OBJECT (fimcconvert, "OutBuf :: It's SR32");
			imgb->cs = SCMN_CS_ARGB8888;
		} else {
			free(imgb);
			GST_ERROR_OBJECT (fimcconvert, "OutBuf :: can not support this format (%d)",fimcconvert->dst_format_gst);
			return FALSE;
		}

		if (fimcconvert->buf_share_method_type == BUF_SHARE_METHOD_FD) {
			GST_DEBUG_OBJECT (fimcconvert, "OutBuf :: y_fd(%d), cb_fd(%d), cr_fd(%d)",imgb->dma_buf_fd[SCMN_Y_PLANE],imgb->dma_buf_fd[SCMN_CB_PLANE],imgb->dma_buf_fd[SCMN_CR_PLANE]);
		} else {
			GST_DEBUG_OBJECT (fimcconvert, "OutBuf :: y_bo(%p), cb_bo(%p), cr_bo(%p)",imgb->bo[SCMN_Y_PLANE],imgb->bo[SCMN_CB_PLANE],imgb->bo[SCMN_CR_PLANE]);
		}

#ifdef USE_DETECT_JPEG
		/* allocation memory and setting for JPEG */
		if (fimcconvert->jpeg_data && fimcconvert->jpeg_data_size != 0) {
			imgb->jpeg_data = malloc((size_t)fimcconvert->jpeg_data_size);
			if (!imgb->jpeg_data) {
				GST_ERROR_OBJECT (fimcconvert, "OutBuf :: failed to malloc() for JPEG data, size(%d)", fimcconvert->jpeg_data_size);
			} else {
				GST_INFO_OBJECT (fimcconvert, "OutBuf :: JPEG data is detected, data(%p), size(%d), DO MEMCPY", imgb->jpeg_data, fimcconvert->jpeg_data_size);
				imgb->jpeg_size = fimcconvert->jpeg_data_size;
				memcpy ((void*)(unsigned int)imgb->jpeg_data, fimcconvert->jpeg_data, fimcconvert->jpeg_data_size);
			}
		}
#endif

	} else {
		free(imgb);
		GST_ERROR_OBJECT (fimcconvert, "OutBuf :: invalid buffer sharing method type (%d)", fimcconvert->buf_share_method_type);
		return FALSE;
	}

	/* Set SCMN_IMGB pointer */
	gst_buffer_prepend_memory(buf, gst_memory_new_wrapped(GST_MEMORY_FLAG_READONLY,
	                                                        imgb,
	                                                        sizeof(*imgb),
	                                                        0,
	                                                        sizeof(*imgb),
	                                                        imgb,
	                                                        free));

	return TRUE;
}

static guint32
gst_fimcconvert_get_drm_format(GstCaps *caps)
{
	GstStructure *structure = NULL;
    GstVideoInfo video_info;

	structure = gst_caps_get_structure(caps, 0);
	if (gst_structure_has_name (structure, "video/x-raw")) {
	    gst_video_info_from_caps(&video_info, caps);
	    if (GST_VIDEO_INFO_IS_RGB(&video_info)) {
            gint bpp = 0;

            if (!gst_structure_get_int (structure, "bpp", &bpp) || (bpp & 0x07) != 0)
                return 0;
            if (bpp == 16) {
                GST_INFO ("It's RGB565, bpp(16)");
                return DRM_FORMAT_RGB565;
            } else if (bpp == 24) {
                GST_INFO ("It's RGB888, bpp(24)");
                return DRM_FORMAT_RGB888;
            } else if (bpp == 32) {
    #if 1
                GST_INFO ("It's XRGB8888, bpp(32)");
                return DRM_FORMAT_XRGB8888;
    #else
                /* NOTE : BGRA8888 test required if kernel support it */
                GST_INFO ("It's BGRA8888, bpp(32)");
                return DRM_FORMAT_BGRA8888;
    #endif
            } else {
                GST_ERROR ("bpp is not valid. bpp(%d)", bpp);
                return 0;
            }
	    } else if (GST_VIDEO_INFO_IS_YUV(&video_info)) {
	        const gchar *format = GST_VIDEO_INFO_NAME(&video_info);
            if (!format) {
                GST_ERROR ("can not get format in gst structure");
                return 0;
            }

            GST_INFO ("format - %s", format);
            if (strcmp(format, "ST12") == 0)
                return DRM_FORMAT_NV12MT;
            else if (strcmp(format, "SN12") == 0
                    || strcmp(format, "NV12") == 0)
                return DRM_FORMAT_NV12;
            else if (strcmp(format, "SUYV") == 0
                    || strcmp(format, "YUYV") == 0
                    || strcmp(format, "SUY2") == 0
                    || strcmp(format, "YUY2") == 0)
                return DRM_FORMAT_YUYV;
            else if (strcmp(format, "I420") == 0
                    || strcmp(format, "S420") == 0)
                return DRM_FORMAT_YUV420;
            else if (strcmp(format, "ITLV") == 0
                    || strcmp(format, "SYVY") == 0
                    || strcmp(format, "UYVY") == 0)
                return DRM_FORMAT_UYVY;
            else {
                GST_WARNING ("not supported format(%s)", format);
                return 0;
            }
	    }
	}
	return 0;
}

static GstCaps *
gst_fimcconvert_transform_caps(GstBaseTransform *trans, GstPadDirection direction, GstCaps *caps, GstCaps *filter)
{
 	GstCaps *template;
	GstCaps *ret_caps;
	GstPad *otherpad;

	if (direction == GST_PAD_SRC) {
		otherpad = trans->sinkpad;
	} else if (direction == GST_PAD_SINK) {
		otherpad = trans->srcpad;
	} else {
		return NULL;
	}

	template = gst_caps_copy(gst_pad_get_pad_template_caps(otherpad));
	ret_caps = gst_caps_intersect(caps, template);
	ret_caps = gst_caps_make_writable(ret_caps);
	gst_caps_append(ret_caps, template);

	return filter ? gst_caps_intersect(ret_caps, filter) : ret_caps;
}

// fixate_caps function is from videoscale plugin
static GstCaps*
gst_fimcconvert_fixate_caps(GstBaseTransform *base, GstPadDirection direction, GstCaps *caps, GstCaps *othercaps)
{
	GstStructure *ins, *outs;
	const GValue *from_par, *to_par;
	GstFimcConvert *fimcconvert;

	g_return_val_if_fail(gst_caps_is_fixed(caps), NULL);

	fimcconvert = GST_FIMCCONVERT(base);

	GST_DEBUG_OBJECT(base, "trying to fixate othercaps %" GST_PTR_FORMAT
			" based on caps %" GST_PTR_FORMAT, othercaps, caps);

	ins = gst_caps_get_structure(caps, 0);
	outs = gst_caps_get_structure(othercaps, 0);

	from_par = gst_structure_get_value(ins, "pixel-aspect-ratio");
	to_par = gst_structure_get_value(outs, "pixel-aspect-ratio");

	/* we have both PAR but they might not be fixated */
	if (from_par && to_par) {
		gint from_w, from_h, from_par_n, from_par_d, to_par_n, to_par_d;
		gint count = 0, w = 0, h = 0;
		guint num, den;

		/* from_par should be fixed */
		g_return_val_if_fail(gst_value_is_fixed(from_par), NULL);

		from_par_n = gst_value_get_fraction_numerator(from_par);
		from_par_d = gst_value_get_fraction_denominator(from_par);

		/* fixate the out PAR */
		if (!gst_value_is_fixed(to_par)) {
			GST_DEBUG_OBJECT(base, "fixating to_par to %dx%d", from_par_n, from_par_d);
			gst_structure_fixate_field_nearest_fraction(outs, "pixel-aspect-ratio",	from_par_n, from_par_d);
		}

		to_par_n = gst_value_get_fraction_numerator(to_par);
		to_par_d = gst_value_get_fraction_denominator(to_par);

		/* if both width and height are already fixed, we can't do anything
		 * about it anymore */
		if (gst_structure_get_int(outs, "width", &w))
			++count;
		if (gst_structure_get_int(outs, "height", &h))
			++count;
		if (count == 2) {
			GST_DEBUG_OBJECT(base, "dimensions already set to %dx%d, not fixating", w, h);
			return othercaps;
		}

		gst_structure_get_int(ins, "width", &from_w);
		gst_structure_get_int(ins, "height", &from_h);

		if (!gst_video_calculate_display_ratio(&num, &den, from_w, from_h, from_par_n, from_par_d, to_par_n, to_par_d)) {
			GST_ELEMENT_ERROR(base, CORE, NEGOTIATION,(NULL), ("Error calculating the output scaled size - integer overflow"));
			return NULL;
		}

		GST_DEBUG_OBJECT(base, "scaling input with %dx%d and PAR %d/%d to output PAR %d/%d", from_w, from_h, from_par_n, from_par_d, to_par_n, to_par_d);
		GST_DEBUG_OBJECT(base, "resulting output should respect ratio of %d/%d", num, den);

		/* now find a width x height that respects this display ratio.
		 * prefer those that have one of w/h the same as the incoming video
		 * using wd / hd = num / den */

		/* if one of the output width or height is fixed, we work from there */
		if (h) {
			GST_DEBUG_OBJECT(base, "height is fixed,scaling width");
			w =(guint) gst_util_uint64_scale_int(h, num, den);
		} else if (w) {
			GST_DEBUG_OBJECT(base, "width is fixed, scaling height");
			h =(guint) gst_util_uint64_scale_int(w, den, num);
		} else {
			/* none of width or height is fixed, figure out both of them based only on
			 * the input width and height */
			/* check hd / den is an integer scale factor, and scale wd with the PAR */
			if (from_h % den == 0) {
				GST_DEBUG_OBJECT(base, "keeping video height");
				h = from_h;
				w =(guint) gst_util_uint64_scale_int(h, num, den);
			} else if (from_w % num == 0) {
				GST_DEBUG_OBJECT(base, "keeping video width");
				w = from_w;
				h =(guint) gst_util_uint64_scale_int(w, den, num);
			} else {
				GST_DEBUG_OBJECT(base, "approximating but keeping video height");
				h = from_h;
				w =(guint) gst_util_uint64_scale_int(h, num, den);
			}
		}
		GST_DEBUG_OBJECT(base, "scaling to %dx%d", w, h);

		/* now fixate */
		gst_structure_fixate_field_nearest_int(outs, "width", w);
		gst_structure_fixate_field_nearest_int(outs, "height", h);
	} else {
		gint width = 0;
		gint height = 0;
		gint rotate = 0;
		gint flip = 0;
		GValue *framerate = NULL;

		if (gst_structure_get_int(outs, "rotate", &rotate)) {
			switch (rotate) {
			case 0:
			case 90:
			case 180:
			case 270:
				fimcconvert->rotate_ang = rotate;
				GST_INFO_OBJECT(fimcconvert, "set rotate(%d) from outcaps", rotate);
				break;
			default:
				break;
			}
		}
		if (gst_structure_get_int(outs, "flip", &flip)) {
			switch (flip) {
			case 0:
			case 1:
			case 2:
				fimcconvert->dst_flip = flip;
				GST_INFO_OBJECT(fimcconvert, "set flip(%d) from outcaps", flip);
				break;
			default:
				break;
			}
		}
		framerate = gst_structure_get_value(ins, "framerate");
		if (framerate) {
			if (gst_structure_has_field(outs, "framerate")) {
				gst_structure_set_value (outs, "framerate", framerate);
			}
		}

		if (gst_structure_has_field(outs, "width") && gst_structure_has_field(outs, "height")) {
			if (fimcconvert->dst_width && fimcconvert->dst_height) {
				if (gst_structure_fixate_field_nearest_int(outs, "width", fimcconvert->dst_width)) {
					GST_INFO_OBJECT(fimcconvert, "set width size from property value(%d), rotate(%d)", fimcconvert->dst_width, fimcconvert->rotate_ang);
				} else {
					GST_WARNING_OBJECT(fimcconvert, "fixate_field_nearest_int(width) failed");
				}
				if (gst_structure_fixate_field_nearest_int(outs, "height", fimcconvert->dst_height)) {
					GST_INFO_OBJECT(fimcconvert, "set height size from property value(%d), rotate(%d)", fimcconvert->dst_height, fimcconvert->rotate_ang);
				} else {
					GST_WARNING_OBJECT(fimcconvert, "fixate_field_nearest_int(height) failed");
				}
			} else {
				if ((fimcconvert->rotate_ang == 90) || (fimcconvert->rotate_ang == 270)) {
					if (gst_structure_get_int(outs, "width", &width)
							&& gst_structure_get_int(outs, "height", &height)) {	/* outcaps already have width/height */
						/* do nothing */
					} else {
						if (gst_structure_get_int(ins, "width", &width)
							 && gst_structure_get_int(ins, "height", &height)) {	/* outcaps does not have width/height, use incaps */
							if (gst_structure_fixate_field_nearest_int(outs, "width", height)) {
								GST_INFO_OBJECT(fimcconvert, "set width size from caps value(%d), rotate(%d)", height, fimcconvert->rotate_ang);
							} else {
								GST_WARNING_OBJECT(fimcconvert, "fixate_field_nearest_int(width) failed");
							}
							if (gst_structure_fixate_field_nearest_int(outs, "height", width)) {
								GST_INFO_OBJECT(fimcconvert, "set height size from caps value(%d), rotate(%d)", width, fimcconvert->rotate_ang);
							} else {
								GST_WARNING_OBJECT(fimcconvert, "fixate_field_nearest_int(height) failed");
							}
						} else {
							GST_ERROR_OBJECT(fimcconvert, "can not get width/height from incaps");
							return NULL;
						}
					}
				} else {
					if (gst_structure_get_int(outs, "width", &width)
							&& gst_structure_get_int(outs, "height", &height)) {	/* outcaps already have width/height */
						/* do nothing */
					} else {
						if (gst_structure_get_int(ins, "width", &width)
							 && gst_structure_get_int(ins, "height", &height)) {	/* outcaps does not have width/height, use incaps */
							if (gst_structure_fixate_field_nearest_int(outs, "width", width)) {
								GST_INFO_OBJECT(fimcconvert, "set width size from caps value(%d)", width);
							} else {
								GST_WARNING_OBJECT(fimcconvert, "fixate_field_nearest_int(width) failed");
							}
							if (gst_structure_fixate_field_nearest_int(outs, "height", height)) {
								GST_INFO_OBJECT(fimcconvert, "set height size from caps value(%d)", height);
							} else {
								GST_WARNING_OBJECT(fimcconvert, "fixate_field_nearest_int(height) failed");
							}
						} else {
							GST_ERROR_OBJECT(fimcconvert, "can not get width/height from incaps");
							return NULL;
						}
					}
				}
			}
		} else {
			GST_ERROR_OBJECT(fimcconvert, "outcaps does not have width/height fields");
			return NULL;
		}
	}

	GST_INFO_OBJECT(base, "fixated othercaps to %" GST_PTR_FORMAT, othercaps);
	return othercaps;
}

static gboolean
gst_fimcconvert_set_caps(GstBaseTransform *trans, GstCaps *in, GstCaps *out)
{
	GstFimcConvert *fimcconvert = NULL;
	GstStructure *structure = NULL;
	const gchar *format_name = NULL;
	gchar *str_in = NULL;
	gchar *str_out = NULL;
	const gchar *src_format_gst = NULL;
	gint src_caps_width = 0;
	gint src_caps_height = 0;

	fimcconvert = GST_FIMCCONVERT(trans);
	str_in = gst_caps_to_string(in);
	if(str_in == NULL) {
		GST_ERROR_OBJECT(fimcconvert, "gst_caps_to_string() returns NULL...");
		return FALSE;
	}
	GST_WARNING_OBJECT(fimcconvert, "[incaps] %s", str_in);

	str_out = gst_caps_to_string(out);
	if(str_out == NULL) {
		GST_ERROR_OBJECT(fimcconvert, "gst_caps_to_string() returns NULL...");
		g_free (str_in);
		return FALSE;
	}
	GST_WARNING_OBJECT(fimcconvert, "[outcaps] %s", str_out);

	/* get src width/height/format of video frame */
	structure = gst_caps_get_structure(in, 0);
	if (!gst_structure_get_int (structure, "width", &src_caps_width) || !gst_structure_get_int (structure, "height", &src_caps_height)) {
		GST_ERROR_OBJECT (fimcconvert, "input frame width or height is not set...");
		return FALSE;
	}
	if (!(src_format_gst = gst_structure_get_string (structure, "format"))) {
		GST_DEBUG_OBJECT (fimcconvert, "format(src) is not set...it may be rgb series");
	}

	if ( strcmp(src_format_gst, "SN12") != 0 &&
	        strcmp(src_format_gst, "ST12") != 0 &&
	        strcmp(src_format_gst, "S420") != 0 &&
	        strcmp(src_format_gst, "SUYV") != 0 &&
	        strcmp(src_format_gst, "SUY2") != 0 &&
	        strcmp(src_format_gst, "SYVY") != 0 &&
	        strcmp(src_format_gst, "ITLV") != 0 &&
	        strcmp(src_format_gst, "SR32") != 0 	) {
		fimcconvert->is_src_format_std = TRUE;
	}

	/* determine whether if incaps is same as outcaps */
	if ( !strncmp(str_in, str_out, strlen(str_in))
		&& ( (!fimcconvert->dst_width && !fimcconvert->dst_height) || ((fimcconvert->dst_width == src_caps_width) && (fimcconvert->dst_height == src_caps_height)) )
		&& !fimcconvert->dst_flip && !fimcconvert->rotate_ang ) {
		GST_LOG_OBJECT(fimcconvert, "incaps is same as outcaps");
		fimcconvert->is_same_caps = TRUE;
		g_free (str_in);
		g_free (str_out);
		return TRUE;
	} else {
		fimcconvert->is_same_caps = FALSE;
		g_free (str_in);
		g_free (str_out);
	}

	/* set src width/height of video frame */
	fimcconvert->src_caps_width = src_caps_width;
	fimcconvert->src_caps_height = src_caps_height;
	fimcconvert->src_format_drm = gst_fimcconvert_get_drm_format(in);
	GST_INFO_OBJECT(fimcconvert, "[set src_caps] width(%d) height(%d)", fimcconvert->src_caps_width, fimcconvert->src_caps_height);

	/* set dst width/height of video frame */
	if (!fimcconvert->dst_width || !fimcconvert->dst_height) {
		structure = gst_caps_get_structure(out, 0);
		if (!gst_structure_get_int (structure, "width", &fimcconvert->dst_caps_width) || !gst_structure_get_int (structure, "height", &fimcconvert->dst_caps_height) ) {
			GST_ERROR_OBJECT (fimcconvert, "output frame width or height is not set...");
			return FALSE;
		}
	}
	fimcconvert->dst_format_drm = gst_fimcconvert_get_drm_format(out);
	format_name = gst_structure_get_string (structure, "format");
	fimcconvert->dst_format_gst = gst_video_format_to_fourcc(gst_video_format_from_string(format_name));
	if (!fimcconvert->dst_format_gst) {
		GST_DEBUG_OBJECT (fimcconvert, "format(dst) is not set...it may be rgb series");
	}

	GST_INFO_OBJECT(fimcconvert, "[set dst_caps] width(%d) height(%d)", fimcconvert->dst_caps_width, fimcconvert->dst_caps_height);
	if (fimcconvert->dst_format_gst) {
		GST_INFO_OBJECT(fimcconvert, "[set dst_caps] format(%c%c%c%c)",	fimcconvert->dst_format_gst, fimcconvert->dst_format_gst>>8,
				fimcconvert->dst_format_gst>>16, fimcconvert->dst_format_gst>>24);
	}

	gst_fimcconvert_reset(fimcconvert);

	return TRUE;
}

static gboolean
gst_fimcconvert_get_unit_size(GstBaseTransform *trans, GstCaps *caps, guint *size)
{
	GstStructure *structure;
	gint width, height;
	gint bytes_per_pixel;
	gint stride;
	GstFimcConvert *fimcconvert;
	
	fimcconvert = GST_FIMCCONVERT(trans);
	
	structure = gst_caps_get_structure (caps, 0);
	if (!gst_structure_get_int (structure, "width", &width) ||!gst_structure_get_int (structure, "height", &height)) {
		return FALSE;
	}

	if (gst_structure_has_name (structure, "video/x-raw")) {
	    GstVideoInfo video_info;
	    gst_video_info_from_caps(&video_info, caps);
	    if (GST_VIDEO_INFO_IS_RGB(&video_info)) {
            gint bpp = 0;
            GST_DEBUG_OBJECT (fimcconvert, "Output format is in RGB...");
            if (!gst_structure_get_int (structure, "bpp", &bpp) || (bpp & 0x07) != 0) {
                return FALSE;
            }
            bytes_per_pixel = bpp / 8;
            stride = ALIGN_TO_4B(width) * bytes_per_pixel;
            *size = stride * ALIGN_TO_4B(height);
	    } else if (GST_VIDEO_INFO_IS_YUV(&video_info)) {
	        gint32 width = 0;
	        gint32 height = 0;

	        GST_DEBUG_OBJECT (fimcconvert,"format is in YUV...");
	        if (!gst_structure_get_int (structure, "width", &width)) {
	            return FALSE;
	        }
	        if (!gst_structure_get_int (structure, "height", &height)) {
	            return FALSE;
	        }

	        switch (gst_video_format_to_fourcc(GST_VIDEO_INFO_FORMAT(&video_info))) {
	        case GST_MAKE_FOURCC('S', 'T', '1', '2'):
	        case GST_MAKE_FOURCC('S', 'N', '1', '2'):
	        case GST_MAKE_FOURCC('N', 'V', '1', '2'):
	        case GST_MAKE_FOURCC('S', '4', '2', '0'):
	        case GST_MAKE_FOURCC('I', '4', '2', '0'):
	            *size = (width * height * 3) >> 1;
	            break;
	        case GST_MAKE_FOURCC('I', 'T', 'L', 'V'):
	        case GST_MAKE_FOURCC('S', 'U', 'Y', '2'):
	        case GST_MAKE_FOURCC('Y', 'U', 'Y', '2'):
	        case GST_MAKE_FOURCC('S', 'U', 'Y', 'V'):
	        case GST_MAKE_FOURCC('Y', 'U', 'Y', 'V'):
	        case GST_MAKE_FOURCC('S', 'Y', 'V', 'Y'):
	        case GST_MAKE_FOURCC('U', 'Y', 'V', 'Y'):
	            *size = (width * height * 2);
	            break;
	        default:
	            GST_DEBUG_OBJECT (fimcconvert, "Not handling selected YUV format...");
	            return FALSE;
	        }
	    }
	}
	GST_DEBUG_OBJECT (fimcconvert, "Buffer size is %d", *size);

	return TRUE;
}

static GstFlowReturn 
gst_fimcconvert_prepare_output_buffer(GstBaseTransform *trans, GstBuffer *in_buf, GstBuffer **out_buf)
{
	gboolean ret = TRUE;
	guint out_size = 0;
	guint in_size = 0;
	GstFimcConvert *fimcconvert = GST_FIMCCONVERT(trans);
	GstCaps *out_caps;
	GstCaps *in_caps;
	GstFimcConvertBuffer *fimcconvert_buffer = NULL;

	GST_DEBUG ("[START]");

	if (fimcconvert->is_same_caps) {
		GST_INFO ("in/out caps are same. skipping it");
		GST_DEBUG ("[END]");
		return GST_FLOW_OK;
	}

	g_mutex_lock (fimcconvert->fd_lock);

	in_caps = gst_pad_get_current_caps (trans->sinkpad);
	out_caps = gst_pad_get_current_caps (trans->srcpad);

	ret = gst_fimcconvert_get_unit_size(trans, in_caps, &in_size);
	if (ret == FALSE) {
		GST_ERROR ("ERROR in _get_unit_size from in_caps...");
		return GST_FLOW_ERROR;
	}
	ret = gst_fimcconvert_get_unit_size(trans, out_caps, &out_size);
	if (ret == FALSE) {
		GST_ERROR ("ERROR in _get_unit_size from out_caps...");
		return GST_FLOW_ERROR;
	}

	GST_INFO ("input caps :: %s", gst_caps_to_string(in_caps));
	GST_INFO ("output caps :: %s", gst_caps_to_string(out_caps));
	GST_DEBUG ("In buffer size = %d", in_size);
	GST_DEBUG ("Out buffer size = %d", out_size);

	fimcconvert->dst_buf_size = out_size;


	fimcconvert_buffer = gst_fimcconvert_buffer_new(fimcconvert, out_size);
	*out_buf = fimcconvert_buffer->buffer;
	if (*out_buf == NULL) {
		GST_ERROR ("ERROR in creating outbuf...");
		return GST_FLOW_ERROR;
	}

	gst_buffer_copy_into(*out_buf, in_buf, GST_BUFFER_COPY_METADATA, 0, -1);
	GST_LOG ("out ts : %"GST_TIME_FORMAT" and out dur : %"GST_TIME_FORMAT,
			GST_TIME_ARGS(GST_BUFFER_TIMESTAMP(*out_buf)), GST_TIME_ARGS(GST_BUFFER_DURATION(*out_buf)));
	GST_DEBUG ("output buffer ready");
	gst_caps_unref(out_caps);
	gst_buffer_append_memory(*out_buf, gst_memory_new_wrapped(GST_MEMORY_FLAG_READONLY,
	                                                          fimcconvert_buffer,
	                                                          sizeof(*fimcconvert_buffer),
	                                                          0,
	                                                          sizeof(*fimcconvert_buffer),
	                                                          fimcconvert_buffer,
	                                                          (GDestroyNotify)gst_fimcconvert_buffer_finalize));
	g_mutex_unlock (fimcconvert->fd_lock);

	GST_DEBUG ("[END]");

	return GST_FLOW_OK;
}

static gboolean
gst_fimcconvert_check_new_src(GstFimcConvert *fimcconvert, GstBuffer *inbuf)
{
	SCMN_IMGB *imgb = NULL;
	GstMemory *imgb_memory = NULL;
	GstMapInfo imgb_info = GST_MAP_INFO_INIT;
	int i = 0;
	guint phy_addr = 0;
	gint fd = 0;
	tbm_bo bo = 0;

	g_return_val_if_fail (GST_IS_FIMCCONVERT (fimcconvert), FALSE);

	if (!fimcconvert->is_src_format_std) {
	    if (gst_buffer_n_memory(inbuf) > 2) {
	        imgb_memory = gst_buffer_peek_memory(inbuf, 1);
	        gst_memory_map(imgb_memory, &imgb_info, GST_MAP_READ);
	        imgb = (SCMN_IMGB *)imgb_info.data;
	        gst_memory_unmap(imgb_memory, &imgb_info);
            if (imgb->buf_share_method == BUF_SHARE_METHOD_PADDR) {
                GST_LOG_OBJECT (fimcconvert, "Samsung colorspace format, PADDR type");
                phy_addr = (guint)imgb->p[SCMN_Y_PLANE];
                if (phy_addr) {
                    for (i = 0; i < MAX_SRC_BUF_NUM; i++) {
                        if (fimcconvert->gem_info_src[i][SCMN_Y_PLANE].gem_handle) {
                            if (phy_addr == fimcconvert->gem_info_src[i][SCMN_Y_PLANE].phy_addr) {
                                return FALSE;
                            }
                        } else {
                            return TRUE;
                        }
                    }
                } else {
                    GST_WARNING_OBJECT (fimcconvert, "invalid value : imgb->p[SCMN_Y_PLANE] = 0");
                    fimcconvert->do_skip = TRUE;
                    return FALSE;
                }
            } else if (imgb->buf_share_method == BUF_SHARE_METHOD_FD) {
                GST_LOG_OBJECT (fimcconvert, "Samsung colorspace format, FD type");
                fd = imgb->dma_buf_fd[SCMN_Y_PLANE];
                if (fd) {
                    for (i = 0; i < MAX_SRC_BUF_NUM; i++) {
                        if (fimcconvert->gem_info_src[i][SCMN_Y_PLANE].dma_buf_fd) {
                            if (fd == fimcconvert->gem_info_src[i][SCMN_Y_PLANE].dma_buf_fd) {
                                return FALSE;
                            }
                        } else {
                            return TRUE;
                        }
                    }
                } else {
                    GST_WARNING_OBJECT (fimcconvert, "invalid value : imgb->dma_buf_fd[SCMN_Y_PLANE] = 0");
                    fimcconvert->do_skip = TRUE;
                    return FALSE;
                }
            } else if (imgb->buf_share_method == BUF_SHARE_METHOD_TIZEN_BUFFER) {
                GST_LOG_OBJECT (fimcconvert, "Samsung colorspace format, TBM type");
                bo = imgb->bo[SCMN_Y_PLANE];
                if (bo) {
                    for (i = 0; i < MAX_SRC_BUF_NUM; i++) {
                        if (fimcconvert->gem_info_src[i][SCMN_Y_PLANE].bo) {
                            if (bo == fimcconvert->gem_info_src[i][SCMN_Y_PLANE].bo) {
                                return FALSE;
                            }
                        } else {
                            return TRUE;
                        }
                    }
                } else {
                    GST_WARNING_OBJECT (fimcconvert, "invalid value : imgb->bo[SCMN_Y_PLANE] = 0");
                    fimcconvert->do_skip = TRUE;
                    return FALSE;
                }
            } else {
                GST_ERROR_OBJECT (fimcconvert, "Samsung colorspace format, could not find BUF_SHARE_METHOD type");
                return FALSE;
            }
		} else {
			GST_ERROR_OBJECT (fimcconvert, "Samsung colorspace format, could not get IMGB");
			return FALSE;
		}
	} else {
		/* if a buffer passed from src plugin has a standard colorspace format, return TRUE */
		if (fimcconvert->src_buf_idx == SRC_BUF_NUM_FOR_STD_FMT) {
			return FALSE;
		} else {
			return TRUE;
		}
	}

	GST_ERROR_OBJECT (fimcconvert, "should not reach here");
	return TRUE;
}

static gboolean
gst_fimcconvert_drm_prepare_src_buf(GstFimcConvert *fimcconvert, GstBuffer *inbuf)
{
	SCMN_IMGB *imgb = NULL;
    GstMemory *imgb_memory = NULL;
    GstMapInfo imgb_info = GST_MAP_INFO_INIT;
	guint src_idx = 0;
	guint dst_idx = 0;

	struct drm_exynos_gem_phy_imp gem_src_phy_imp_y;
	struct drm_exynos_gem_phy_imp gem_src_phy_imp_cb;
	struct drm_exynos_gem_phy_imp gem_src_phy_imp_cr;

	g_return_val_if_fail (GST_IS_FIMCCONVERT (fimcconvert), FALSE);

	GST_DEBUG_OBJECT (fimcconvert, "[START]");
#ifdef DEBUG_FOR_DV
	if (total_converted_frame_count <= 10) {
		GST_WARNING_OBJECT (fimcconvert, "[START]");
	}
#endif

	memset(&gem_src_phy_imp_y, 0x0, sizeof(struct drm_exynos_gem_phy_imp));
	memset(&gem_src_phy_imp_cb, 0x0, sizeof(struct drm_exynos_gem_phy_imp));
	memset(&gem_src_phy_imp_cr, 0x0, sizeof(struct drm_exynos_gem_phy_imp));

	src_idx = fimcconvert->src_buf_idx;
	dst_idx = fimcconvert->dst_buf_idx;

	GST_INFO_OBJECT (fimcconvert, "InBuf :: input buffer(%p)", inbuf);

	if (!fimcconvert->is_src_format_std) {
        if (gst_buffer_n_memory(inbuf) > 2) {
            imgb_memory = gst_buffer_peek_memory(inbuf, 1);
            gst_memory_map(imgb_memory, &imgb_info, GST_MAP_READ);
            imgb = (SCMN_IMGB *)imgb_info.data;
            gst_memory_unmap(imgb_memory, &imgb_info);
			GST_LOG_OBJECT (fimcconvert, "InBuf :: buf_share_method_type=%d (0:PADDR/1:FD/2:TBM)", imgb->buf_share_method);

			if (imgb->buf_share_method == BUF_SHARE_METHOD_PADDR) {
				fimcconvert->buf_share_method_type = BUF_SHARE_METHOD_PADDR;

				switch (fimcconvert->src_format_drm) {
				case DRM_FORMAT_NV12MT:
					gem_src_phy_imp_y.phy_addr = (guint)imgb->p[0];
					gem_src_phy_imp_cb.phy_addr = (guint)imgb->p[1];
					gem_src_phy_imp_y.size = ALIGN_TO_8KB( ALIGN_TO_128B(fimcconvert->src_caps_width) * ALIGN_TO_32B(fimcconvert->src_caps_height));
					gem_src_phy_imp_cb.size = ALIGN_TO_8KB( ALIGN_TO_128B(fimcconvert->src_caps_width) * ALIGN_TO_32B(fimcconvert->src_caps_height>>1) );
					break;
				case DRM_FORMAT_NV12:
					gem_src_phy_imp_y.phy_addr = (guint)imgb->p[0];
					gem_src_phy_imp_cb.phy_addr = (guint)imgb->p[1];
					gem_src_phy_imp_y.size = imgb->w[0] * imgb->h[0];
					gem_src_phy_imp_cb.size = imgb->w[0] * imgb->h[0] >> 1;
					break;
				case DRM_FORMAT_UYVY:
				case DRM_FORMAT_YUYV:
					gem_src_phy_imp_y.phy_addr = (guint)imgb->p[0];
					gem_src_phy_imp_y.size = (imgb->w[0] * imgb->h[0]) << 1;
					break;
				case DRM_FORMAT_YUV420:
					gem_src_phy_imp_y.phy_addr = (guint)imgb->p[0];
					gem_src_phy_imp_cb.phy_addr = (guint)imgb->p[1];
					gem_src_phy_imp_cr.phy_addr = (guint)imgb->p[2];
					gem_src_phy_imp_y.size = imgb->w[0] * imgb->h[0];
					gem_src_phy_imp_cb.size = imgb->w[0] * imgb->h[0] >> 2;
					gem_src_phy_imp_cr.size = gem_src_phy_imp_cb.size;
					break;
				default:
					GST_ERROR_OBJECT (fimcconvert, "Un-Supported Input color format...");
					return FALSE;
				}
				GST_INFO_OBJECT (fimcconvert, "InBuf :: It's a samsung extension format(%d)",fimcconvert->src_format_drm);

				if (!gem_src_phy_imp_y.phy_addr) {
					GST_WARNING_OBJECT (fimcconvert, "gem_src_phy_imp_y.phy_addr is null..");
					fimcconvert->do_skip = TRUE;
					return TRUE;
				}

				/* Do DRM_IOCTL_EXYNOS_GEM_GET_PHY */
				if (fimcconvert->drm_fimc_fd) {
					if (fimcconvert_drm_gem_import_phy_addr (fimcconvert->drm_fimc_fd, &gem_src_phy_imp_y)) {
						return FALSE;
					}
					fimcconvert->gem_info_src[src_idx][SCMN_Y_PLANE].gem_handle = gem_src_phy_imp_y.gem_handle;
					fimcconvert->gem_info_src[src_idx][SCMN_Y_PLANE].phy_addr = gem_src_phy_imp_y.phy_addr;
					GST_DEBUG_OBJECT (fimcconvert, "InBuf :: gem_src_phy_imp_y[%d] : gem_handle=%d, y_phy_addr=0x%x",
						src_idx, gem_src_phy_imp_y.gem_handle,(void*)(unsigned long)gem_src_phy_imp_y.phy_addr);

					switch (fimcconvert->src_format_drm) {
						case DRM_FORMAT_NV12MT:
						case DRM_FORMAT_NV12:
							if (fimcconvert_drm_gem_import_phy_addr (fimcconvert->drm_fimc_fd, &gem_src_phy_imp_cb)) {
								return FALSE;
							}
							fimcconvert->gem_info_src[src_idx][SCMN_CB_PLANE].gem_handle = gem_src_phy_imp_cb.gem_handle;
							GST_DEBUG_OBJECT (fimcconvert, "InBuf :: gem_src_phy_imp_cb[%d] : gem_handle=%d, cb_phy_addr=0x%x",
								src_idx, gem_src_phy_imp_cb.gem_handle,(void*)(unsigned long)gem_src_phy_imp_cb.phy_addr);
							break;

						case DRM_FORMAT_YUV420:
							if (fimcconvert_drm_gem_import_phy_addr (fimcconvert->drm_fimc_fd, &gem_src_phy_imp_cb)) {
								return FALSE;
							}
							fimcconvert->gem_info_src[src_idx][SCMN_CB_PLANE].gem_handle = gem_src_phy_imp_cb.gem_handle;
							GST_DEBUG_OBJECT (fimcconvert, "InBuf :: gem_src_phy_imp_cb[%d] : gem_handle=%d, cb_phy_addr=0x%x",
								src_idx, gem_src_phy_imp_cb.gem_handle,(void*)(unsigned long)gem_src_phy_imp_cb.phy_addr);
							if (fimcconvert_drm_gem_import_phy_addr (fimcconvert->drm_fimc_fd, &gem_src_phy_imp_cr)) {
								return FALSE;
							}
							fimcconvert->gem_info_src[src_idx][SCMN_CR_PLANE].gem_handle = gem_src_phy_imp_cr.gem_handle;
							GST_DEBUG_OBJECT (fimcconvert, "InBuf :: gem_src_phy_imp_cr[%d] : gem_handle=%d, cr_phy_addr=0x%x",
								src_idx, gem_src_phy_imp_cr.gem_handle,(void*)(unsigned long)gem_src_phy_imp_cr.phy_addr);
							break;
					}
				} else {
					GST_ERROR_OBJECT (fimcconvert, "drm_fimc_fd is null..");
					return FALSE;
				}

			} else if (imgb->buf_share_method == BUF_SHARE_METHOD_FD) {
				int ret = 0;
				int i = 0;
				fimcconvert->buf_share_method_type = BUF_SHARE_METHOD_FD;

				/* convert dma-buf fd into drm gem handle */
				for (i = 0; i < SCMN_IMGB_MAX_PLANE-1; i++) {
					fimcconvert->gem_info_src[src_idx][i].dma_buf_fd = imgb->dma_buf_fd[i];
					ret = fimcconvert_drm_convert_fd_to_gemhandle(fimcconvert->drm_fimc_fd, (int)imgb->dma_buf_fd[i], &fimcconvert->gem_info_src[src_idx][i].gem_handle);
					if (ret) {
						return FALSE;
					}
				}
				GST_DEBUG_OBJECT (fimcconvert, "InBuf :: gem_info_src[%d] : gem_handle_y=%u, gem_handle_cb=%u, gem_handle_cr=%u", src_idx,
						fimcconvert->gem_info_src[src_idx][SCMN_Y_PLANE].gem_handle, fimcconvert->gem_info_src[src_idx][SCMN_CB_PLANE].gem_handle,
						fimcconvert->gem_info_src[src_idx][SCMN_CR_PLANE].gem_handle);

			} else if (imgb->buf_share_method == BUF_SHARE_METHOD_TIZEN_BUFFER) {
				int i = 0;
				tbm_bo_handle bo_handle_gem;

				fimcconvert->buf_share_method_type = BUF_SHARE_METHOD_TIZEN_BUFFER;

				/* convert bo into drm gem handle */
				for (i = 0; i < SCMN_IMGB_MAX_PLANE-1; i++) {
					if (imgb->bo[i] > 0) {
						GST_LOG_OBJECT(fimcconvert,"imgb->bo[%d] = %p", i, imgb->bo[i]);
						fimcconvert->gem_info_src[src_idx][i].bo = imgb->bo[i];
						FIMCCONVERT_TBM_BO_GET_GEM_HANDLE( imgb->bo[i], bo_handle_gem );
						fimcconvert->gem_info_src[src_idx][i].gem_handle = bo_handle_gem.u32;
					}
				}
				GST_DEBUG_OBJECT (fimcconvert, "InBuf :: gem_info_src[%d] : gem_handle_y=%u, gem_handle_cb=%u, gem_handle_cr=%u", src_idx,
						fimcconvert->gem_info_src[src_idx][SCMN_Y_PLANE].gem_handle, fimcconvert->gem_info_src[src_idx][SCMN_CB_PLANE].gem_handle,
						fimcconvert->gem_info_src[src_idx][SCMN_CR_PLANE].gem_handle);
			} else {
				GST_ERROR_OBJECT (fimcconvert, "InBuf :: Unknown BUF_SHARE_METHOD");
				return FALSE;
			}
		} else {
			GST_WARNING_OBJECT (fimcconvert, "InBuf :: could not get imgb from inbuf.. let this frame skip..");
			fimcconvert->do_skip = TRUE;
			return FALSE;
		}
	} else {
		/* for a standard colorspace format */
		fimcconvert->src_buf_size = gst_buffer_get_size(inbuf);

		GST_INFO_OBJECT (fimcconvert, "InBuf :: It's a standard format(%d)", fimcconvert->src_format_drm);
		GST_DEBUG_OBJECT (fimcconvert, "InBuf :: Prepare for memory copy, src data size(%d)", fimcconvert->src_buf_size);

		/* Make gem handle and do map for src buffer */
		if (!gst_fimcconvert_tbm_create_and_map_bo(fimcconvert, FIMCCONVERT_GEM_FOR_SRC)) {
			return FALSE;
		}

		/* determine buf_share_method_type */
		if (!fimcconvert->is_drm_inited) {
			if (fimcconvert_drm_gem_get_phy_addr(fimcconvert->drm_fimc_fd, fimcconvert->gem_info_src_normal_fmt[src_idx].gem_handle, &fimcconvert->gem_info_src_normal_fmt[src_idx].phy_addr)) {
#ifdef USE_TBM
				fimcconvert->buf_share_method_type = BUF_SHARE_METHOD_TIZEN_BUFFER;
				GST_WARNING_OBJECT (fimcconvert, "InBuf :: buf_share_method_type = TIZEN_BUFFER");
#else
				fimcconvert->buf_share_method_type = BUF_SHARE_METHOD_FD;
				GST_WARNING_OBJECT (fimcconvert, "InBuf :: buf_share_method_type = FD");
#endif
			} else {
				fimcconvert->buf_share_method_type = BUF_SHARE_METHOD_PADDR;
				GST_WARNING_OBJECT (fimcconvert, "InBuf :: buf_share_method_type = PADDR");
			}
		}

		fimcconvert->gem_info_src[src_idx][SCMN_Y_PLANE].gem_handle = fimcconvert->gem_info_src_normal_fmt[src_idx].gem_handle;
		GST_DEBUG_OBJECT (fimcconvert, "InBuf :: gem_info_src[%d].gem_handle_y=%u", src_idx, fimcconvert->gem_info_src[src_idx][SCMN_Y_PLANE].gem_handle);
	}

	GST_DEBUG_OBJECT (fimcconvert, "[END]");
#ifdef DEBUG_FOR_DV
	if (total_converted_frame_count <= 10) {
		GST_WARNING_OBJECT (fimcconvert, "[END]");
	}
#endif

	return TRUE;
}

static gboolean
gst_fimcconvert_drm_prepare_dst_buf(GstFimcConvert *fimcconvert)
{
	guint dst_idx = 0;

	g_return_val_if_fail (GST_IS_FIMCCONVERT (fimcconvert), FALSE);

	GST_DEBUG_OBJECT (fimcconvert, "[START]");
#ifdef DEBUG_FOR_DV
	if (total_converted_frame_count <= 10) {
		GST_WARNING_OBJECT (fimcconvert, "[START]");
	}
#endif

	dst_idx = fimcconvert->dst_buf_idx;

	/* Make gem handle and do map for dst buffer */
	if (!gst_fimcconvert_tbm_create_and_map_bo(fimcconvert, FIMCCONVERT_GEM_FOR_DST)) {
		return FALSE;
	}

	if (fimcconvert->buf_share_method_type == BUF_SHARE_METHOD_PADDR) {
		GST_LOG_OBJECT (fimcconvert, "OutBuf :: buf_share_method_type == PADDR");

		/* Get phy address of each gem handle for destination */
		if (fimcconvert_drm_gem_get_phy_addr(fimcconvert->drm_fimc_fd,
			fimcconvert->gem_info_dst[dst_idx][SCMN_Y_PLANE].gem_handle, &fimcconvert->gem_info_dst[dst_idx][SCMN_Y_PLANE].phy_addr)) {
			return FALSE;
		}
		if (fimcconvert->dst_format_gst == GST_MAKE_FOURCC('S','N','1','2')
		|| fimcconvert->dst_format_gst == GST_MAKE_FOURCC('S','T','1','2')) {
			/* CBCR Plane */
			if (fimcconvert_drm_gem_get_phy_addr(fimcconvert->drm_fimc_fd,
				fimcconvert->gem_info_dst[dst_idx][SCMN_CB_PLANE].gem_handle, &fimcconvert->gem_info_dst[dst_idx][SCMN_CB_PLANE].phy_addr)) {
				return FALSE;
			}
			/* Memset according to colorspace format (NV12) */
			if (fimcconvert->dst_format_drm == DRM_FORMAT_NV12) {
				int y_size = fimcconvert->dst_caps_width * fimcconvert->dst_caps_height;
				GST_LOG_OBJECT (fimcconvert, "memset : y_addr(0x%x), size(%d)",
					(void*)(unsigned long)fimcconvert->gem_info_dst[dst_idx][SCMN_Y_PLANE].usr_addr, y_size);
				if (fimcconvert->gem_info_dst[dst_idx][SCMN_Y_PLANE].usr_addr) {
					memset ((void*)(unsigned long)fimcconvert->gem_info_dst[dst_idx][SCMN_Y_PLANE].usr_addr, 16, y_size);
				}
				if (fimcconvert->dst_format_drm == DRM_FORMAT_NV12) {
					GST_LOG_OBJECT (fimcconvert, "memset : cbcr_addr(0x%x), size(%d)",
						(void*)(unsigned long)fimcconvert->gem_info_dst[dst_idx][SCMN_CB_PLANE].usr_addr, fimcconvert->dst_buf_size - y_size);
					if (fimcconvert->gem_info_dst[dst_idx][SCMN_CB_PLANE].usr_addr) {
						memset ((void*)(unsigned long)fimcconvert->gem_info_dst[dst_idx][SCMN_CB_PLANE].usr_addr, 128, fimcconvert->dst_buf_size - y_size);
					}
				}
			}
		} else if (fimcconvert->dst_format_gst == GST_MAKE_FOURCC('S','4','2','0')) {
			/* CB Plane */
			if (fimcconvert_drm_gem_get_phy_addr(fimcconvert->drm_fimc_fd,
				fimcconvert->gem_info_dst[dst_idx][SCMN_CB_PLANE].gem_handle, &fimcconvert->gem_info_dst[dst_idx][SCMN_CB_PLANE].phy_addr)) {
				return FALSE;
			}
			/* CR Plane */
			if (fimcconvert_drm_gem_get_phy_addr(fimcconvert->drm_fimc_fd,
				fimcconvert->gem_info_dst[dst_idx][SCMN_CR_PLANE].gem_handle, &fimcconvert->gem_info_dst[dst_idx][SCMN_CR_PLANE].phy_addr)) {
				return FALSE;
			}
			/* Memset according to colorspace format (YUV420) */
			{
				int y_size = fimcconvert->dst_caps_width * fimcconvert->dst_caps_height;
				GST_LOG_OBJECT (fimcconvert, "memset : y_addr(0x%x), size(%d)",
					(void*)(unsigned long)fimcconvert->gem_info_dst[dst_idx][SCMN_Y_PLANE].usr_addr, y_size);
				if (fimcconvert->gem_info_dst[dst_idx][SCMN_Y_PLANE].usr_addr) {
					memset ((void*)(unsigned long)fimcconvert->gem_info_dst[dst_idx][SCMN_Y_PLANE].usr_addr, 16, y_size);
				}
				int cb_size = (fimcconvert->dst_caps_width * fimcconvert->dst_caps_height) >> 2;
				GST_LOG_OBJECT (fimcconvert, "memset : cb_addr(0x%x), size(%d)",
					(void*)(unsigned long)fimcconvert->gem_info_dst[dst_idx][SCMN_CB_PLANE].usr_addr, cb_size);
				if (fimcconvert->gem_info_dst[dst_idx][SCMN_CB_PLANE].usr_addr) {
					memset ((void*)(unsigned long)fimcconvert->gem_info_dst[dst_idx][SCMN_CB_PLANE].usr_addr, 128, cb_size);
				}
				GST_LOG_OBJECT (fimcconvert, "memset : cr_addr(0x%x), size(%d)",
					(void*)(unsigned long)fimcconvert->gem_info_dst[dst_idx][SCMN_CR_PLANE].usr_addr, cb_size);
				if (fimcconvert->gem_info_dst[dst_idx][SCMN_CR_PLANE].usr_addr) {
					memset ((void*)(unsigned long)fimcconvert->gem_info_dst[dst_idx][SCMN_CR_PLANE].usr_addr, 128, cb_size);
				}
			}
		}
	} else if (fimcconvert->buf_share_method_type == BUF_SHARE_METHOD_FD || fimcconvert->buf_share_method_type == BUF_SHARE_METHOD_TIZEN_BUFFER ) {
		GST_LOG_OBJECT (fimcconvert, "OutBuf :: buf_share_method_type (%d)", fimcconvert->buf_share_method_type);

	}

	GST_DEBUG_OBJECT (fimcconvert, "[END]");
#ifdef DEBUG_FOR_DV
	if (total_converted_frame_count <= 10) {
		GST_WARNING_OBJECT (fimcconvert, "[END]");
	}
#endif

	return TRUE;
}

static void
gst_fimcconvert_tbm_unmap_and_destroy_bo(GstFimcConvert *fimcconvert, FimcConvertGemCreateType type, int index)
{
	int i = 0;
	int tbm_ret = 0;
	if (type == FIMCCONVERT_GEM_FOR_SRC) {
		/* for standard colorspace format */
		if (fimcconvert->gem_info_src_normal_fmt[index].bo) {
			GST_LOG_OBJECT (fimcconvert, "SRC : (standard format) unmap src(%p)", (void*)(unsigned long)fimcconvert->gem_info_src_normal_fmt[index].usr_addr);
			tbm_ret = tbm_bo_unmap(fimcconvert->gem_info_src_normal_fmt[index].bo);
			if (tbm_ret == 0) {
				GST_ERROR_OBJECT (fimcconvert, "SRC : (standard format) failed to tbm_bo_unmap(bo:%p), index(%d)", fimcconvert->gem_info_src_normal_fmt[index].bo, index);
			}
			GST_LOG_OBJECT (fimcconvert, "SRC : (standard format) tbm_bo_unref(bo:%p), index(%d)", fimcconvert->gem_info_src_normal_fmt[index].bo, index);
			tbm_bo_unref (fimcconvert->gem_info_src_normal_fmt[index].bo);
			memset(&fimcconvert->gem_info_src_normal_fmt[index], 0x00, sizeof(GemInfoDst));
		}
		/* for samsung extension colorspace format */
		for (i = 0; i < SCMN_IMGB_MAX_PLANE-1; i++) {
			if (fimcconvert->gem_info_src[index][i].bo) {
				memset(&fimcconvert->gem_info_src[index], 0x00, sizeof(GemInfoSrc));
			}
			if (fimcconvert->gem_info_src[index][i].dma_buf_fd || fimcconvert->gem_info_src[index][i].phy_addr) {
				GST_LOG_OBJECT (fimcconvert, "SRC : close gem_handle(%d)", fimcconvert->gem_info_src[index][i].gem_handle);
				fimcconvert_drm_close_gem(fimcconvert->drm_fimc_fd, fimcconvert->gem_info_src[index][i].gem_handle);
				memset(&fimcconvert->gem_info_src[index], 0x00, sizeof(GemInfoSrc));
			}
		}
	} else if (type == FIMCCONVERT_GEM_FOR_DST) {
		for (i = 0; i < SCMN_IMGB_MAX_PLANE-1; i++) {
			if (fimcconvert->gem_info_dst[index][i].bo) {
				GST_LOG_OBJECT (fimcconvert, "DST : unref dst(%p)", (void*)(unsigned long)fimcconvert->gem_info_dst[index][i].usr_addr);
				tbm_bo_unref (fimcconvert->gem_info_dst[index][i].bo);
			}
			if (fimcconvert->gem_info_dst[index][i].dma_buf_fd) {
				GST_LOG_OBJECT (fimcconvert, "DST : close fd(%d)", fimcconvert->gem_info_dst[index][i].dma_buf_fd);
				close(fimcconvert->gem_info_dst[index][i].dma_buf_fd);
			}
			memset(&fimcconvert->gem_info_dst[index][i], 0x00, sizeof(GemInfoDst));
		}
	}

	return;
}

static gboolean
gst_fimcconvert_tbm_create_and_map_bo(GstFimcConvert *fimcconvert, FimcConvertGemCreateType type)
{
	tbm_bo bo;
	tbm_bo_handle bo_handle_vaddr;
	tbm_bo_handle bo_handle_gem;
	guint src_idx = 0;
	guint dst_idx = 0;
	gint mem_option = 0;

	g_return_val_if_fail (GST_IS_FIMCCONVERT (fimcconvert), FALSE);
	g_return_val_if_fail (fimcconvert->drm_fimc_fd, -1);
	g_return_val_if_fail (fimcconvert->dst_buf_size, -1);

	src_idx = fimcconvert->src_buf_idx;
	dst_idx = fimcconvert->dst_buf_idx;

	if (fimcconvert->buf_share_method_type == BUF_SHARE_METHOD_PADDR) {
		mem_option = MEM_OP_NONCACHE_CONTIG;
	} else {
		mem_option = MEM_OP_NONCACHE_NONCONTIG;
	}
	GST_WARNING_OBJECT (fimcconvert, "memory option=%d (2:NONCACHEABLE_NONCONTIG 3:NONCACHEABLE_CONTIG)", mem_option);

	if (type == FIMCCONVERT_GEM_FOR_SRC) {

		GST_DEBUG_OBJECT (fimcconvert, "create and map bo for SRC");

		g_return_val_if_fail (fimcconvert->src_buf_size, -1);

		/* allocate and map memory */
		FIMCCONVERT_TBM_BO_ALLOC_AND_MAP(bo, bo_handle_vaddr, fimcconvert->src_buf_size, mem_option);

		/* get gem handle */
		FIMCCONVERT_TBM_BO_GET_GEM_HANDLE(bo, bo_handle_gem);

		/* update information */
		fimcconvert->gem_info_src_normal_fmt[src_idx].gem_handle = bo_handle_gem.u32;
		fimcconvert->gem_info_src_normal_fmt[src_idx].usr_addr = bo_handle_vaddr.ptr;
		fimcconvert->gem_info_src_normal_fmt[src_idx].size = fimcconvert->src_buf_size;
		fimcconvert->gem_info_src_normal_fmt[src_idx].bo = bo;
		GST_DEBUG_OBJECT (fimcconvert, "gem_handle=%u, user addr=%p, size=%u", bo_handle_gem.u32, bo_handle_vaddr.ptr, fimcconvert->src_buf_size);

	} else if (type == FIMCCONVERT_GEM_FOR_DST) {

		GST_DEBUG_OBJECT (fimcconvert, "create and map bo for DST");

		g_return_val_if_fail (fimcconvert->dst_buf_size, -1);
		g_return_val_if_fail (fimcconvert->dst_caps_width, -1);
		g_return_val_if_fail (fimcconvert->dst_caps_height, -1);

		int y_size = 0;
		int cb_size = 0;
		int cr_size = 0;

	/* for Y plane */
		/* initialize structure for gem create */
		if (fimcconvert->dst_format_drm == DRM_FORMAT_NV12MT) {
			GST_DEBUG_OBJECT (fimcconvert, "It's ST12, set Y plane");
			y_size = ALIGN_TO_8KB( ALIGN_TO_128B(fimcconvert->dst_caps_width) * ALIGN_TO_32B(fimcconvert->dst_caps_height));
		} else if (fimcconvert->dst_format_drm == DRM_FORMAT_NV12 ||
				fimcconvert->dst_format_drm == DRM_FORMAT_YUV420) {
			if (fimcconvert->dst_format_gst == GST_MAKE_FOURCC('S','N','1','2')) {
				y_size = ALIGN_TO_16B(fimcconvert->dst_caps_width) * ALIGN_TO_16B(fimcconvert->dst_caps_height);
			} else if (fimcconvert->dst_format_gst == GST_MAKE_FOURCC('S','4','2','0')) {
				if (fimcconvert->buf_share_method_type == BUF_SHARE_METHOD_PADDR) {
					y_size = ALIGN_TO_16B(fimcconvert->dst_caps_width) * ALIGN_TO_2B(fimcconvert->dst_caps_height);
				} else {
					y_size = (ALIGN_TO_16B(fimcconvert->dst_caps_width) * ALIGN_TO_2B(fimcconvert->dst_caps_height) * 3) >> 1;
				}
			} else {
				y_size = fimcconvert->dst_buf_size;
			}
		} else {
			if (fimcconvert->dst_format_drm == DRM_FORMAT_YUYV ||
				fimcconvert->dst_format_drm == DRM_FORMAT_UYVY ) {
				y_size = (ALIGN_TO_16B(fimcconvert->dst_caps_width) * ALIGN_TO_2B(fimcconvert->dst_caps_height) * 2);
			} else {
				/* maybe, it's rgb case */
				y_size = fimcconvert->dst_buf_size;
			}
		}

		/* allocate and map memory */
		FIMCCONVERT_TBM_BO_ALLOC(bo, y_size, mem_option);
		FIMCCONVERT_TBM_BO_GET_VADDR(bo, bo_handle_vaddr, y_size);

		/* get gem handle */
		FIMCCONVERT_TBM_BO_GET_GEM_HANDLE(bo, bo_handle_gem);

		/* update information */
		fimcconvert->gem_info_dst[dst_idx][SCMN_Y_PLANE].gem_handle = bo_handle_gem.u32;
		fimcconvert->gem_info_dst[dst_idx][SCMN_Y_PLANE].usr_addr = bo_handle_vaddr.ptr;
		fimcconvert->gem_info_dst[dst_idx][SCMN_Y_PLANE].size = y_size;
		fimcconvert->gem_info_dst[dst_idx][SCMN_Y_PLANE].bo = bo;

		if (fimcconvert->buf_share_method_type == BUF_SHARE_METHOD_FD) {
			if (fimcconvert_drm_convert_gemhandle_to_fd(fimcconvert->drm_fimc_fd, fimcconvert->gem_info_dst[dst_idx][SCMN_Y_PLANE].gem_handle, &fimcconvert->gem_info_dst[dst_idx][SCMN_Y_PLANE].dma_buf_fd)) {
				return FALSE;
			}
		}
		GST_DEBUG_OBJECT (fimcconvert, "Y: gem_handle=%u, user addr=%p, size=%u", bo_handle_gem.u32, bo_handle_vaddr.ptr, y_size);

		if (fimcconvert->dst_format_gst == GST_MAKE_FOURCC('S','N','1','2')
			|| fimcconvert->dst_format_gst == GST_MAKE_FOURCC('S','T','1','2')
			|| ((fimcconvert->buf_share_method_type == BUF_SHARE_METHOD_PADDR) && fimcconvert->dst_format_gst == GST_MAKE_FOURCC('S','4','2','0'))) {

		/* for CB(CR) plane */
			/* initialize structure for gem create */
			if (fimcconvert->dst_format_drm == DRM_FORMAT_NV12MT) {
				/* for cbcr plane */
				GST_DEBUG_OBJECT (fimcconvert, "It's ST12, set CBCR plane");
				cb_size = ALIGN_TO_8KB( ALIGN_TO_128B(fimcconvert->dst_caps_width) * ALIGN_TO_32B(fimcconvert->dst_caps_height>>1) );
			} else if (fimcconvert->dst_format_drm == DRM_FORMAT_NV12) {
				/* for cbcr plane */
				cb_size = y_size >> 1;
			} else if (fimcconvert->dst_format_drm == DRM_FORMAT_YUV420) {
				cb_size = y_size >> 2;
			}

			/* allocate and map memory */
			FIMCCONVERT_TBM_BO_ALLOC(bo, cb_size, mem_option);
			FIMCCONVERT_TBM_BO_GET_VADDR(bo, bo_handle_vaddr, cb_size);

			/* get gem handle */
			FIMCCONVERT_TBM_BO_GET_GEM_HANDLE(bo, bo_handle_gem);

			/* update information */
			fimcconvert->gem_info_dst[dst_idx][SCMN_CB_PLANE].gem_handle = bo_handle_gem.u32;
			fimcconvert->gem_info_dst[dst_idx][SCMN_CB_PLANE].usr_addr = bo_handle_vaddr.ptr;
			fimcconvert->gem_info_dst[dst_idx][SCMN_CB_PLANE].size = cb_size;
			fimcconvert->gem_info_dst[dst_idx][SCMN_CB_PLANE].bo = bo;

			if (fimcconvert->buf_share_method_type == BUF_SHARE_METHOD_FD) {
				if (fimcconvert_drm_convert_gemhandle_to_fd(fimcconvert->drm_fimc_fd, fimcconvert->gem_info_dst[dst_idx][SCMN_CB_PLANE].gem_handle, &fimcconvert->gem_info_dst[dst_idx][SCMN_CB_PLANE].dma_buf_fd)) {
					return FALSE;
				}
			}
			GST_DEBUG_OBJECT (fimcconvert, "CB(CR): gem_handle=%u, user addr=%p, size=%u", bo_handle_gem.u32, bo_handle_vaddr.ptr, cb_size);

			/* for CR plane */
			if (fimcconvert->dst_format_drm == DRM_FORMAT_YUV420) {
				cr_size = cb_size;

				/* allocate and map memory */
				FIMCCONVERT_TBM_BO_ALLOC(bo, cr_size, mem_option);
				FIMCCONVERT_TBM_BO_GET_VADDR(bo, bo_handle_vaddr, cr_size);

				/* get gem handle */
				FIMCCONVERT_TBM_BO_GET_GEM_HANDLE(bo, bo_handle_gem);

				/* update information */
				fimcconvert->gem_info_dst[dst_idx][SCMN_CR_PLANE].gem_handle = bo_handle_gem.u32;
				fimcconvert->gem_info_dst[dst_idx][SCMN_CR_PLANE].usr_addr = bo_handle_vaddr.ptr;
				fimcconvert->gem_info_dst[dst_idx][SCMN_CR_PLANE].size = cr_size;
				fimcconvert->gem_info_dst[dst_idx][SCMN_CR_PLANE].bo = bo;

				if (fimcconvert->buf_share_method_type == BUF_SHARE_METHOD_FD) {
					if (fimcconvert_drm_convert_gemhandle_to_fd(fimcconvert->drm_fimc_fd, fimcconvert->gem_info_dst[dst_idx][SCMN_CR_PLANE].gem_handle, &fimcconvert->gem_info_dst[dst_idx][SCMN_CR_PLANE].dma_buf_fd)) {
						return FALSE;
					}
				}
				GST_DEBUG_OBJECT (fimcconvert, "CR: gem_handle=%u, user addr=%p, size=%u", bo_handle_gem.u32, bo_handle_vaddr.ptr, cr_size);
			}
		}
	} else {
		GST_ERROR ("invalid type(%d) for gem creation", type);
		return FALSE;
	}

	return TRUE;
}

int
util_write_rawdata(const char *file, const void *data, unsigned int size)
{
	FILE *fp;

	fp = fopen(file, "wb");
	if (fp == NULL)
		return -1;

	fwrite((char*)data, sizeof(char), size, fp);
	fclose(fp);

	return 0;
}

static GstFlowReturn
gst_fimcconvert_transform(GstBaseTransform *trans, GstBuffer *inbuf, GstBuffer *outbuf)
{
	int ret = 0;
	int gret = TRUE;

	GstFimcConvert *fimcconvert = NULL;
	SCMN_IMGB *imgb = NULL;
    GstMemory *imgb_memory = NULL;
    GstMapInfo imgb_info = GST_MAP_INFO_INIT;
	guint src_idx = 0;
	guint dst_idx = 0;

	struct timeval timeout = { .tv_sec = 3, .tv_usec = 0 };
	fd_set fds;

	fimcconvert = GST_FIMCCONVERT(trans);

	GST_DEBUG_OBJECT (fimcconvert, "[START]");

	/* check if all destination buffers are full */
	if (!gst_fimcconvert_is_buffer_available(fimcconvert)) {
		GST_DEBUG_OBJECT (fimcconvert, "[END]");
		return GST_FLOW_ERROR;
	}

	/* Set src/dst index */
	src_idx = fimcconvert->src_buf_idx;
	dst_idx = fimcconvert->dst_buf_idx;

	g_mutex_lock (fimcconvert->fd_lock);
	g_mutex_lock (instance_lock);

#ifdef DEBUG_FOR_DV
	if (total_converted_frame_count <= 10) {
		GST_WARNING_OBJECT (fimcconvert, "is_src_format_std(%d), inbuf(%p)",fimcconvert->is_src_format_std,inbuf);
	}
#endif
	if (!fimcconvert->is_src_format_std) {
		/* get an imgb of input buffer */
		if (gst_buffer_n_memory(inbuf) <= 2) {
			GST_WARNING_OBJECT (fimcconvert, "failed to get SCMN_IMGB");
			goto SKIP_CONVERTING;
		} else {
#ifdef USE_DETECT_JPEG
			/* detect JPEG data */
	        imgb_memory = gst_buffer_peek_memory(inbuf, 1);
	        gst_memory_map(imgb_memory, &imgb_info, GST_MAP_READ);
	        imgb = (SCMN_IMGB *)imgb_info.data;
	        gst_memory_unmap(imgb_memory, &imgb_info);
			if (imgb->jpeg_data && imgb->jpeg_size) {
				GST_LOG_OBJECT (fimcconvert, "got a JPEG data");
				fimcconvert->jpeg_data = imgb->jpeg_data;
				fimcconvert->jpeg_data_size = imgb->jpeg_size;
			} else {
				fimcconvert->jpeg_data = NULL;
				fimcconvert->jpeg_data_size = 0;
			}
#endif
		}
	}

	/* initializing DRM IPP */
	if (!fimcconvert->is_drm_inited) {
		if (!fimcconvert->is_src_format_std && imgb->buf_share_method == BUF_SHARE_METHOD_TIZEN_BUFFER && imgb->drm_fd) {
			fimcconvert->drm_tbm_fd = imgb->drm_fd;
			GST_WARNING_OBJECT (fimcconvert, "drm fd for tbm : %d", fimcconvert->drm_tbm_fd);
		}
		/* Open DRM */
		if (!gst_fimcconvert_drm_ipp_init(fimcconvert)) {
			GST_ERROR_OBJECT (fimcconvert, "failed to gst_fimcconvert_drm_ipp_init()");
			g_mutex_unlock (instance_lock);
			g_mutex_unlock (fimcconvert->fd_lock);
			return GST_FLOW_ERROR;
		}
	}

	/* Do below only when first round */
	if (fimcconvert->is_random_src_idx) {
		GST_DEBUG_OBJECT (fimcconvert, "===== Set for SRC ==================================================================");
		if (fimcconvert->is_src_format_std) {
			/* if src colorspace format is standard format, do below just once. */
			if (!fimcconvert->gem_info_src_normal_fmt[0].gem_handle) {
				gret = gst_fimcconvert_drm_prepare_src_buf(fimcconvert, inbuf);
				if (!gret) {
					if (!fimcconvert->do_skip) {
						goto ERROR;
					}
				}
			}
		} else {
			gret = gst_fimcconvert_drm_prepare_src_buf(fimcconvert, inbuf);
			if (!gret) {
				if (!fimcconvert->do_skip) {
					goto ERROR;
				}
			}
		}
		GST_DEBUG_OBJECT (fimcconvert, "===================================================================================");
	} else if (fimcconvert->is_first_set_for_src) {
		/* Check if src buffer from decoder has new phy address */
		if (gst_fimcconvert_check_new_src(fimcconvert, inbuf)) {
			GST_DEBUG_OBJECT (fimcconvert, "===== First Set for SRC : Round %d =================================================",src_idx+1);
			gret = gst_fimcconvert_drm_prepare_src_buf(fimcconvert, inbuf);
			if (!gret) {
				if (!fimcconvert->do_skip) {
					goto ERROR;
				}
			}
			GST_DEBUG_OBJECT (fimcconvert, "===================================================================================");
		} else {
			/* check if this buffer should be skipped */
			if (fimcconvert->do_skip) {
				GST_WARNING_OBJECT (fimcconvert, "something wrong, gem_handle_y is 0. maybe this src buffer has invalid paddr/fd");
				/* set is_random_src_idx to TRUE and reset all buffers */
				fimcconvert->is_random_src_idx = TRUE;
				g_mutex_unlock (instance_lock);
				g_mutex_unlock (fimcconvert->fd_lock);
				gst_fimcconvert_reset(fimcconvert);
				g_mutex_lock (fimcconvert->fd_lock);
				g_mutex_lock (instance_lock);
			} else {
				GST_INFO_OBJECT (fimcconvert, "found number of decoder output buffer(%d)",src_idx);
				fimcconvert->src_buf_num = src_idx;
				fimcconvert->is_first_set_for_src = FALSE;
				fimcconvert->src_buf_idx = src_idx = 0;
			}
		}
	}

	/* check if this buffer should be skipped */
	if (fimcconvert->do_skip) {
		goto SKIP_CONVERTING;
	}

	if (fimcconvert->is_first_set_for_dst) {
		GST_DEBUG_OBJECT (fimcconvert, "===== First Set for DST : Round %d =================================================",dst_idx+1);
		gret = gst_fimcconvert_drm_prepare_dst_buf(fimcconvert);
		if (!gret) {
			goto ERROR;
		}
		GST_DEBUG_OBJECT (fimcconvert, "===================================================================================");
	}

	/* Check if src data's colorspace is normal format */
	if (fimcconvert->is_src_format_std) {
		GST_INFO_OBJECT (fimcconvert, "InBuf :: It's a standard format(%d), Do memory copy",fimcconvert->src_format_drm);
		gst_buffer_extract(inbuf, 0, (void*)(unsigned int)fimcconvert->gem_info_src_normal_fmt[src_idx].usr_addr, gst_buffer_get_size(inbuf));
#ifdef USE_CACHABLE_GEM
		fimcconvert_gem_flush_cache(fimcconvert->drm_fimc_fd);
#endif
#ifdef DUMP_IMG
		int ret = 0;
		GST_INFO ("DUMP SRC IMG : inbuf size(%d)", GST_BUFFER_SIZE(inbuf));
		ret = util_write_rawdata("dump_raw_src_img", GST_BUFFER_DATA(inbuf), GST_BUFFER_SIZE(inbuf));
		if (ret) {
			GST_ERROR_OBJECT (fimcconvert, "util_write_rawdata() failed");
		}
#endif
	}

	/* Enqueue src buffer */
	GST_LOG_OBJECT (fimcconvert, "enqueue src buffer : src_idx(%d), gem_handle_y(%u), gem_handle_cb(%u), gem_handle_cr(%u)",
		src_idx,fimcconvert->gem_info_src[src_idx][SCMN_Y_PLANE].gem_handle,fimcconvert->gem_info_src[src_idx][SCMN_CB_PLANE].gem_handle,fimcconvert->gem_info_src[src_idx][SCMN_CR_PLANE].gem_handle);
	ret = exynos_drm_ipp_queue_buf(fimcconvert->drm_fimc_fd, EXYNOS_DRM_OPS_SRC, IPP_BUF_ENQUEUE, fimcconvert->ipp_prop_id, src_idx,
		fimcconvert->gem_info_src[src_idx][SCMN_Y_PLANE].gem_handle, fimcconvert->gem_info_src[src_idx][SCMN_CB_PLANE].gem_handle,
		fimcconvert->gem_info_src[src_idx][SCMN_CR_PLANE].gem_handle);
	if (ret) {
		GST_ERROR_OBJECT (fimcconvert, "src buffer map to IPP failed, src_buf_idx(%d)", src_idx);
		goto ERROR;
	}

	/* Enqueue dst buffer */
	GST_LOG_OBJECT (fimcconvert, "enqueue dst buffer : dst_idx(%d), gem_handle_y(%u), gem_handle_cb(%u), gem_handle_cr(%u)",
		dst_idx, fimcconvert->gem_info_dst[dst_idx][SCMN_Y_PLANE].gem_handle, fimcconvert->gem_info_dst[dst_idx][SCMN_CB_PLANE].gem_handle, fimcconvert->gem_info_dst[dst_idx][SCMN_CR_PLANE].gem_handle);
	ret = exynos_drm_ipp_queue_buf(fimcconvert->drm_fimc_fd, EXYNOS_DRM_OPS_DST, IPP_BUF_ENQUEUE, fimcconvert->ipp_prop_id, dst_idx,
		fimcconvert->gem_info_dst[dst_idx][SCMN_Y_PLANE].gem_handle, fimcconvert->gem_info_dst[dst_idx][SCMN_CB_PLANE].gem_handle, fimcconvert->gem_info_dst[dst_idx][SCMN_CR_PLANE].gem_handle);
	if (ret) {
		GST_ERROR_OBJECT (fimcconvert, "dst buffer map to IPP failed, dst_buf_idx(%d)", dst_idx);
		goto ERROR;
	}

	/* Set IPP to start */
	if (!fimcconvert->is_drm_inited) {
		if (!gst_fimcconvert_drm_ipp_ctrl(fimcconvert, FIMCCONVERT_IPP_CTRL_START)) {
			GST_ERROR_OBJECT (fimcconvert, "failed to gst_fimcconvert_drm_ipp_ctrl(START)");
			goto ERROR;
		}
	}

	/* Wait using select */
	FD_ZERO(&fds);
	FD_SET(0, &fds);
	FD_SET(fimcconvert->drm_fimc_fd, &fds);
	ret = select(fimcconvert->drm_fimc_fd + 1, &fds, NULL, NULL, &timeout);
	if (ret == 0 ) {
		GST_ERROR_OBJECT (fimcconvert, "select timed out..");
		goto ERROR;
	}
	if (ret == -1) {
		GST_ERROR_OBJECT (fimcconvert, "select error, error num(%d)",errno);
		goto ERROR;
	}

	/* Event handling */
	{
		char buffer[MAX_IPP_EVENT_BUFFER_SIZE] = {0};
		int len = 0;
		int i = 0;
		struct drm_event *e;
		struct drm_exynos_ipp_event *ipp_event;

#ifdef DEBUG_FOR_DV
		if (total_converted_frame_count <= 10) {
			GST_WARNING_OBJECT (fimcconvert, "before reading IPP event (no.%d)", total_converted_frame_count+1);
		}
#endif

		len = read(fimcconvert->drm_fimc_fd, buffer, MAX_IPP_EVENT_BUFFER_SIZE-1);
		if (len == 0 || len < sizeof *e) {
			GST_ERROR_OBJECT (fimcconvert, "lengh(%d) is not valid", len);
			goto ERROR;
		}

		while (i < len) {
			e = (struct drm_event *) &buffer[i];
			switch (e->type) {
			case DRM_EXYNOS_IPP_EVENT:
				ipp_event = (struct drm_exynos_ipp_event *) e;
				src_idx = ipp_event->buf_id[EXYNOS_DRM_OPS_SRC];
				dst_idx = ipp_event->buf_id[EXYNOS_DRM_OPS_DST];
				GST_LOG_OBJECT (fimcconvert, "got DRM_EXYNOS_IPP_EVENT : src_buf_idx(%d), dst_buf_idx(%d)",src_idx,dst_idx);
				break;
			default:
				GST_WARNING_OBJECT (fimcconvert, "got DRM_EXYNOS Other EVENT(%d)",e->type);
				break;
			}
			i += e->length;
		}

#ifdef DEBUG_FOR_DV
		if (total_converted_frame_count <= 10) {
			GST_WARNING_OBJECT (fimcconvert, "after reading IPP event (no.%d), got DRM_EXYNOS_IPP_EVENT : src_buf_idx(%d), dst_buf_idx(%d)",
					total_converted_frame_count+1, src_idx, dst_idx);
		}
#endif
	}

	/* Update SCMN_IMGB information for outbuf */
	if ( fimcconvert->dst_format_gst == GST_MAKE_FOURCC('S','N','1','2')
		|| fimcconvert->dst_format_gst == GST_MAKE_FOURCC('S','T','1','2')
		|| fimcconvert->dst_format_gst == GST_MAKE_FOURCC('S','U','Y','V')
		|| fimcconvert->dst_format_gst == GST_MAKE_FOURCC('S','U','Y','2')
		|| fimcconvert->dst_format_gst == GST_MAKE_FOURCC('S','Y','V','Y')
		|| fimcconvert->dst_format_gst == GST_MAKE_FOURCC('S','4','2','0')
		|| fimcconvert->dst_format_gst == GST_MAKE_FOURCC('S','R','3','2') ) {
		if (!gst_fimcconvert_set_imgb_info_for_dst(fimcconvert, outbuf)) {
			GST_ERROR_OBJECT (fimcconvert, "set SCMN_IMGB information failed..idx(%d)",dst_idx);
			goto ERROR;
		}
	}

	if (fimcconvert->is_random_src_idx) {
		if (!fimcconvert->is_src_format_std) {
			gst_fimcconvert_tbm_unmap_and_destroy_bo (fimcconvert, FIMCCONVERT_GEM_FOR_SRC, src_idx);
		}
	} else {
		fimcconvert->src_buf_idx++;
		if (fimcconvert->src_buf_idx == fimcconvert->src_buf_num || fimcconvert->src_buf_idx == MAX_SRC_BUF_NUM) {
			if (fimcconvert->is_first_set_for_src) {
				if (fimcconvert->src_buf_idx == MAX_SRC_BUF_NUM) {
				}
				GST_WARNING_OBJECT (fimcconvert, "it reached MAX_SRC_BUF_NUM(%d)", fimcconvert->src_buf_idx);
				fimcconvert->is_first_set_for_src = FALSE;
			}
			fimcconvert->src_buf_idx = 0;
		}
	}
	fimcconvert->dst_buf_idx++;
	if (fimcconvert->dst_buf_idx == fimcconvert->num_of_dst_buffers) {
		if (fimcconvert->is_first_set_for_dst) {
			fimcconvert->is_first_set_for_dst = FALSE;
		}
		fimcconvert->dst_buf_idx = 0;
	}

	{
        GstFimcConvertBuffer* fimc_buf = NULL;
        GstMemory *fimc_memory = gst_buffer_peek_memory(outbuf, gst_buffer_n_memory(outbuf) - 1);
        GstMapInfo fimc_info = GST_MAP_INFO_INIT;
        gst_memory_map(fimc_memory, &fimc_info, GST_MAP_READ);
        fimc_buf = (GstFimcConvertBuffer*)fimc_info.data;
        gst_memory_unmap(fimc_memory, &fimc_info);
        gst_buffer_prepend_memory(outbuf, gst_memory_new_wrapped(GST_MEMORY_FLAG_READONLY,
                                                                 (void*)(unsigned long)fimcconvert->gem_info_dst[dst_idx][SCMN_Y_PLANE].usr_addr,
                                                                  fimc_buf->actual_size,
                                                                  0,
                                                                  fimc_buf->actual_size,
                                                                  NULL,
                                                                  NULL));
	}

#ifdef DUMP_IMG
	GST_INFO ("DUMP DST IMG : outbuf size(%d)", GST_BUFFER_SIZE(outbuf));
	ret = util_write_rawdata("dump_raw_dst_img", GST_BUFFER_DATA(outbuf), GST_BUFFER_SIZE(outbuf));
	if (ret) {
		GST_ERROR_OBJECT (fimcconvert, "util_write_rawdata() failed");
	}
#endif

	if (!fimcconvert->is_drm_inited) {
		fimcconvert->is_drm_inited = TRUE;
		FIMCCONVERT_SEND_CUSTOM_EVENT_TO_SINK_WITH_DATA( 1 );
	}
#ifdef USE_CACHABLE_GEM
	__ta__("    flush_cache() after IPP",
	fimcconvert_gem_flush_cache(fimcconvert->drm_fimc_fd);
	);
#endif
	g_mutex_unlock (instance_lock);
	g_mutex_unlock (fimcconvert->fd_lock);

	GST_DEBUG_OBJECT (fimcconvert, "[END]");

#ifdef DEBUG_FOR_DV
	if (++total_converted_frame_count <= 10) {
		GST_WARNING_OBJECT (fimcconvert, "%2dth of IPP operation, buffer(%p)", total_converted_frame_count, outbuf);
	}
#endif

	return GST_FLOW_OK;

SKIP_CONVERTING:
	/* skipping converting in case of incoming src buf's information is not valid */
	GST_WARNING_OBJECT (fimcconvert, "SKIPPING CONVERTING..");

	fimcconvert->do_skip = FALSE;

	g_mutex_lock (fimcconvert->buf_idx_lock);
	if (fimcconvert->dst_buf_num > 0) {
		fimcconvert->dst_buf_num--;
	}
	g_mutex_unlock (fimcconvert->buf_idx_lock);

	g_mutex_unlock (instance_lock);
	g_mutex_unlock (fimcconvert->fd_lock);

	GST_DEBUG_OBJECT (fimcconvert, "[END]");
	return GST_BASE_TRANSFORM_FLOW_DROPPED;

ERROR:
	g_mutex_unlock (instance_lock);
	g_mutex_unlock (fimcconvert->fd_lock);

	gst_fimcconvert_reset(fimcconvert);

	GST_DEBUG_OBJECT (fimcconvert, "[END]");
	return GST_FLOW_ERROR;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
	GST_DEBUG_CATEGORY_INIT(fimcconvert_debug, "fimcconvert", 0, "Fimc Converter");
	return gst_element_register(plugin, "fimcconvert", GST_RANK_NONE, GST_TYPE_FIMCCONVERT);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    fimcconvert,
    "Fimc Converter",
    plugin_init, VERSION, "LGPL", "Samsung Electronics Co", "http://www.samsung.com")

