/*
 * FimcConvert
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

#ifndef __GST_FIMC_CONVERT_H__
#define __GST_FIMC_CONVERT_H__

#include <stdio.h>
#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <string.h>
#include <gst/video/video.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/vt.h>
#include <error.h>
#include "fimc_drm_api.h"

G_BEGIN_DECLS

GST_DEBUG_CATEGORY_EXTERN (fimcconvert_debug);
#define GST_CAT_DEFAULT fimcconvert_debug

#define GST_TYPE_FIMCCONVERT (gst_fimcconvert_get_type())
#define GST_FIMCCONVERT(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_FIMCCONVERT,GstFimcConvert))
#define GST_FIMCCONVERT_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_FIMCCONVERT,GstFimcConvertClass))
#define GST_IS_FIMCCONVERT(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_FIMCCONVERT))
#define GST_IS_FIMCCONVERT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_FIMCCONVERT))

typedef struct _GstFimcConvert GstFimcConvert;
typedef struct _GstFimcConvertClass GstFimcConvertClass;
typedef struct _GstFimcConvertBuffer GstFimcConvertBuffer;
typedef struct _GemInfoSrc GemInfoSrc;
typedef struct _GemInfoDst GemInfoDst;

typedef enum {
	BUF_SHARE_METHOD_PADDR = 0,
	BUF_SHARE_METHOD_FD,
	BUF_SHARE_METHOD_TIZEN_BUFFER
} buf_share_method_t;

/* macro for sending custom event ********************************************/
#define FIMCCONVERT_SEND_CUSTOM_EVENT_TO_SINK_WITH_DATA( x_addr ) \
do \
{ \
	GstEvent *event = NULL; \
	GstStructure *st = NULL; \
	st =  gst_structure_new ("GstStructureForCustomEvent", "data-addr", G_TYPE_UINT, x_addr, NULL); \
	if (!st) { \
		GST_WARNING_OBJECT(fimcconvert,"structure make failed"); \
	} \
	GST_DEBUG ("set data addr(%x) in GstStructure",x_addr); \
	event = gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM_OOB, st); \
	if (!event) { \
	GST_WARNING_OBJECT(fimcconvert,"event make failed"); \
	} \
	if (GST_IS_PAD (fimcconvert->src_pad)) { \
		if (!gst_pad_push_event(fimcconvert->src_pad, event)) { \
			GST_DEBUG_OBJECT(fimcconvert, "sending event failed, but keep going.."); \
		} \
	} \
}while(0)

/* macro related to TBM ******************************************************/
#define FIMCCONVERT_TBM_BO_ALLOC( x_bo, x_size, x_mem_option ) \
do \
{ \
	x_bo = tbm_bo_alloc (fimcconvert->tbm, x_size, x_mem_option); \
	if (!x_bo) { \
		GST_ERROR_OBJECT(fimcconvert,"failed to tbm_bo_alloc(), size(%d)", x_size); \
		return FALSE; \
	} \
}while(0)

#define FIMCCONVERT_TBM_BO_ALLOC_AND_MAP( x_bo, x_bo_handle, x_size, x_mem_option ) \
do \
{ \
	x_bo = tbm_bo_alloc (fimcconvert->tbm, x_size, x_mem_option); \
	if (!x_bo) { \
		GST_ERROR_OBJECT(fimcconvert,"failed to tbm_bo_alloc(), size(%d)", x_size); \
		return FALSE; \
	} \
	x_bo_handle = tbm_bo_map (x_bo, TBM_DEVICE_CPU, TBM_OPTION_READ | TBM_OPTION_WRITE); \
	if (!x_bo_handle.ptr) { \
		GST_ERROR_OBJECT(fimcconvert,"failed to tbm_bo_map(), bo(%p)", x_bo); \
		return FALSE; \
	} \
	memset (x_bo_handle.ptr, 0x0, x_size); \
}while(0)

#define FIMCCONVERT_TBM_BO_GET_GEM_HANDLE( x_bo, x_bo_handle ) \
do \
{ \
	x_bo_handle = tbm_bo_get_handle(x_bo, TBM_DEVICE_2D); \
	if (!x_bo_handle.u32) { \
		GST_ERROR_OBJECT(fimcconvert,"failed to tbm_bo_get_handle(GEM_HANDLE), bo(%p), gem_handle(%d)", x_bo, x_bo_handle.u32); \
		return FALSE; \
	} else { \
		GST_LOG_OBJECT(fimcconvert,"tbm_bo_get_handle(GEM_HANDLE) finished, bo(%p), gem_handle(%d)", x_bo, x_bo_handle.u32); \
	} \
}while(0)

#define FIMCCONVERT_TBM_BO_GET_VADDR( x_bo, x_bo_handle, x_size ) \
do \
{ \
	x_bo_handle = tbm_bo_get_handle(x_bo, TBM_DEVICE_CPU); \
	if (!x_bo_handle.ptr) { \
		GST_ERROR_OBJECT(fimcconvert,"failed to tbm_bo_get_handle(VADDR), bo(%p), vaddr(%p)", x_bo, x_bo_handle.ptr); \
		return FALSE; \
	} else { \
		memset (x_bo_handle.ptr, 0x0, x_size); \
		GST_LOG_OBJECT(fimcconvert,"tbm_bo_get_handle(VADDR) finished, bo(%p), vaddr(%p)", x_bo, x_bo_handle.ptr); \
	} \
}while(0)

/* color spaces **************************************************************/
/* YUV planar type */
#define SCMN_CS_YUV400              0 /* Y */
#define SCMN_CS_YUV420              1 /* Y:U:V 4:2:0 */
#define SCMN_CS_YUV422              2 /* Y:U:V 4:2:2 */
#define SCMN_CS_YUV444              3 /* Y:U:V 4:4:4 */
#define SCMN_CS_YV12                4 /* Y:V:U 4:2:0 */
#define SCMN_CS_I420                SCMN_CS_YUV420 /* Y:U:V */
#define SCMN_CS_IYUV                SCMN_CS_YUV420 /* Y:U:V */
#define SCMN_CS_YV16                5 /* Y:V:U 4:2:2 */
#define SCMN_CS_NV12                6
#define SCMN_CS_NV21                7
#define SCMN_CS_YUV422N             SCMN_CS_YUV422
#define SCMN_CS_YUV422W             8
#define SCMN_CS_GRAY                SCMN_CS_YUV400
#define SCMN_CS_Y800                SCMN_CS_YUV400
#define SCMN_CS_Y16                 9
#define SCMN_CS_GRAYA               10
#define SCMN_CS_NV12_T64X32         11 /* 64x32 Tiled NV12 type */

/* YUV pack type */
#define SCMN_CS_UYVY                100
#define SCMN_CS_YUYV                101
#define SCMN_CS_YUY2                SCMN_CS_YUYV
#define SCMN_CS_V422                SCMN_CS_YUYV
#define SCMN_CS_YUNV                SCMN_CS_YUYV
#define SCMN_CS_Y8A8                110

/* RGB pack type */
#define SCMN_CS_RGB565              200
#define SCMN_CS_BGR565              201

#define SCMN_CS_RGB555              210 /* 1 : R(5) : G(5) : R(5) */
#define SCMN_CS_BGR555              211

#define SCMN_CS_RGB666              300
#define SCMN_CS_BGR666              301

#define SCMN_CS_RGB888              400
#define SCMN_CS_BGR888              401

#define SCMN_CS_RGBA8888            500
#define SCMN_CS_BGRA8888            501
#define SCMN_CS_ARGB8888            502
#define SCMN_CS_ABGR8888            503

/* unknown color space */
#define SCMN_CS_UNKNOWN             1000

/* macro for color space *****************************************************/
#define SCMN_CS_IS_YUV(cs)          ((cs)>=0 && (cs)<200)
#define SCMN_CS_IS_YUV_PLANAR(cs)   ((cs)>=0 && (cs)<100)
#define SCMN_CS_IS_YUV_PACK(cs)     ((cs)>=100 && (cs)<200)
#define SCMN_CS_IS_RGB_PACK(cs)     ((cs)>=200 && (cs)<600)
#define SCMN_CS_IS_RGB16_PACK(cs)   ((cs)>=200 && (cs)<300)
#define SCMN_CS_IS_RGB18_PACK(cs)   ((cs)>=300 && (cs)<400)
#define SCMN_CS_IS_RGB24_PACK(cs)   ((cs)>=400 && (cs)<500)
#define SCMN_CS_IS_RGB32_PACK(cs)   ((cs)>=500 && (cs)<600)

/* max channel count *********************************************************/
#define SCMN_IMGB_MAX_PLANE         (4)

#define SCMN_Y_PLANE                 0
#define SCMN_CB_PLANE                1
#define SCMN_CR_PLANE                2

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
	/* Y plane size in case of ST12 */
	int y_size;
	/* UV plane size in case of ST12 */
	int uv_size;
	/* TBM buffer object */
	tbm_bo bo[SCMN_IMGB_MAX_PLANE];
	/* JPEG data */
	void *jpeg_data;
	/* JPEG size */
	int jpeg_size;
	/* DRM node fd for TBM */
	int drm_fd;
} SCMN_IMGB;

#define FIMCCONVERT_DST_WIDTH_DEFAULT 0
#define FIMCCONVERT_DST_WIDTH_MIN 0
#define FIMCCONVERT_DST_WIDTH_MAX 32767
#define FIMCCONVERT_DST_HEIGHT_DEFAULT 0
#define FIMCCONVERT_DST_HEIGHT_MIN 0
#define FIMCCONVERT_DST_HEIGHT_MAX 32767
#define FIMCCONVERT_DST_BUFFER_MIN 2
#define FIMCCONVERT_DST_BUFFER_DEFAULT 3
#define FIMCCONVERT_DST_BUFFER_MAX 12

#define SRC_BUF_NUM_FOR_STD_FMT  6		/* fixed number of buffer for standard colorspace format */
#define MAX_SRC_BUF_NUM          12	/* Max number of upstream src plugins's buffer */
#define MAX_DST_BUF_NUM          FIMCCONVERT_DST_BUFFER_MAX

/* Dump raw img (src and dst) */
//#define DUMP_IMG

//#define USE_TBM
#define USE_DETECT_JPEG
#define MEM_OP_NONCACHE_NONCONTIG ( TBM_BO_DEFAULT | TBM_BO_NONCACHABLE )
#define MEM_OP_NONCACHE_CONTIG    ( TBM_BO_SCANOUT | TBM_BO_NONCACHABLE )

struct _GstFimcConvertBuffer {
	GstBuffer *buffer;
	GstFimcConvert *fimcconvert;
	guint actual_size;
};

struct _GemInfoSrc {
	unsigned int gem_handle;
	uint64_t phy_addr;
	int dma_buf_fd;
	tbm_bo bo;
	uint64_t size;
};

struct _GemInfoDst {
	unsigned int gem_handle;
	uint64_t usr_addr;
	uint64_t phy_addr;
	int dma_buf_fd;
	tbm_bo bo;
	uint64_t size;
};

struct _GstFimcConvert {
	GstBaseTransform element;
	GstPad* src_pad;

	/* Src paramters */
	gint src_caps_width;
	gint src_caps_height;
	guint src_buf_size;		/**< for a standard colorspace format */
	guint32 src_format_drm;		/**< fourcc colorspace format for src */
	gboolean is_src_format_std;

	/* Dst paramters */
	gint dst_caps_width;
	gint dst_caps_height;
	guint dst_buf_size;
	guint32 dst_format_drm;		/**< fourcc colorspace format for dst */
	guint32 dst_format_gst;		/**< fourcc colorspace format of GST */

	/* Properties */
	guint rotate_ang;
	gint dst_width;
	gint dst_height;
	gint dst_flip;
	gboolean is_random_src_idx;
	guint num_of_dst_buffers;

	/* DRM/GEM information */
	gint drm_fimc_fd;
	guint32 ipp_prop_id;
	GemInfoSrc gem_info_src[MAX_SRC_BUF_NUM][SCMN_IMGB_MAX_PLANE];
	GemInfoDst gem_info_src_normal_fmt[MAX_SRC_BUF_NUM];
	GemInfoDst gem_info_dst[MAX_DST_BUF_NUM][SCMN_IMGB_MAX_PLANE];
	buf_share_method_t buf_share_method_type;

	/* Tizen Buffer Manager */
	tbm_bufmgr tbm;
	gint drm_tbm_fd;

	gboolean do_skip;
	gboolean is_same_caps;
	gboolean is_drm_inited;
	gboolean is_first_set_for_src;
	gboolean is_first_set_for_dst;
	guint src_buf_num;
	guint src_buf_idx;
	guint dst_buf_num;
	guint dst_buf_idx;

	/* JPEG data */
	void *jpeg_data;
	int jpeg_data_size;

	GMutex *fd_lock;
	GMutex *buf_idx_lock;
};

struct _GstFimcConvertClass {
  GstBaseTransformClass parent_class;
};

GType gst_fimcconvert_get_type(void);

G_END_DECLS

#endif /* __GST_FIMC_CONVERT_H__ */


