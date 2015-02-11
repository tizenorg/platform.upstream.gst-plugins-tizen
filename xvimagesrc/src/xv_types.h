/**************************************************************************

xserver-xorg-video-exynos

Copyright 2010 - 2011 Samsung Electronics co., Ltd. All Rights Reserved.

Contact: Boram Park <boram1288.park@samsung.com>

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sub license, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice (including the
next paragraph) shall be included in all copies or substantial portions
of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/

/*                                                              */
/* File name : xv_types.h                                       */
/* Author : Boram Park (boram1288.park@samsung.com)             */
/* Protocol Version : 1.0.1 (Dec 16th 2009)                       */
/* This file is for describing Xv APIs' buffer encoding method. */
/*                                                              */

#ifndef __XV_TYPE_H__
#define __XV_TYPE_H__

#define XV_DATA_HEADER	0xDEADCD01
#define XV_DATA_VERSION	0x00010001

/* Return Values */
#define XV_OK 0
#define XV_HEADER_ERROR -1
#define XV_VERSION_MISMATCH -2

/* Video Mode */
#define DISPLAY_MODE_DEFAULT                                      0
#define DISPLAY_MODE_PRI_VIDEO_ON_AND_SEC_VIDEO_FULL_SCREEN       1
#define DISPLAY_MODE_PRI_VIDEO_OFF_AND_SEC_VIDEO_FULL_SCREEN      2

/* Color space range */
#define CSC_RANGE_NARROW        0
#define CSC_RANGE_WIDE          1

/* Buffer Type */
#define XV_BUF_TYPE_DMABUF  0
#define XV_BUF_TYPE_LEGACY  1
#define XV_BUF_PLANE_NUM    3

/* Data structure for XvPutImage / XvShmPutImage */
typedef struct
{
    unsigned int _header; /* for internal use only */
    unsigned int _version; /* for internal use only */

    unsigned int YBuf;
    unsigned int CbBuf;
    unsigned int CrBuf;
    unsigned int BufType;
    unsigned int dmabuf_fd[XV_BUF_PLANE_NUM];
    unsigned int gem_handle[XV_BUF_PLANE_NUM];
    void *bo[XV_BUF_PLANE_NUM];
} XV_DATA, * XV_DATA_PTR;

static void
#ifdef __GNUC__
__attribute__ ((unused))
#endif
XV_INIT_DATA (XV_DATA_PTR data)
{
    data->_header = XV_DATA_HEADER;
    data->_version = XV_DATA_VERSION;
}

static int
#ifdef __GNUC__
__attribute__ ((unused))
#endif
XV_VALIDATE_DATA (XV_DATA_PTR data)
{
    if (data->_header != XV_DATA_HEADER)
        return XV_HEADER_ERROR;
    if (data->_version != XV_DATA_VERSION)
        return XV_VERSION_MISMATCH;
    return XV_OK;
}

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
    int w[SCMN_IMGB_MAX_PLANE];
    /* height of each image plane */
    int h[SCMN_IMGB_MAX_PLANE];
    /* stride of each image plane */
    int s[SCMN_IMGB_MAX_PLANE];
    /* elevation of each image plane */
    int e[SCMN_IMGB_MAX_PLANE];
    /* user space address of each image plane */
    void *a[SCMN_IMGB_MAX_PLANE];
    /* physical address of each image plane, if needs */
    void *p[SCMN_IMGB_MAX_PLANE];
    /* color space type of image */
    int cs;
    /* left postion, if needs */
    int x;
    /* top position, if needs */
    int y;
    /* to align memory */
    int __dummy2;
    /* arbitrary data */
    int data[16];
    /* dma buf fd */
    int dmabuf_fd[SCMN_IMGB_MAX_PLANE];
    /* buffer share method */
    int buf_share_method;
    /* Y plane size in case of ST12 */
    int y_size;
    /* UV plane size in case of ST12 */
    int uv_size;
    /* Tizen buffer object */
    void *bo[SCMN_IMGB_MAX_PLANE];
    /* JPEG data */
    void *jpeg_data;
    /* JPEG size */
    int jpeg_size;
    /* TZ memory buffer */
    int tz_enable;
} SCMN_IMGB;

#endif

