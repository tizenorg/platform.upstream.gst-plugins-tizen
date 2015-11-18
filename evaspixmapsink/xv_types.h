/*                                                              */
/* File name : xv_types.h                                       */
/* Author : YoungHoon Jung (yhoon.jung@samsung.com)             */
/* Protocol Version : 1.0.1 (Dec 16th 2009)                       */
/* This file is for describing Xv APIs' buffer encoding method. */
/*                                                              */

#define XV_PUTIMAGE_HEADER	0xDEADCD01
#define XV_PUTIMAGE_VERSION	0x00010001

/* Return Values */
#define XV_OK 0
#define XV_HEADER_ERROR -1
#define XV_VERSION_MISMATCH -2

/* Video Mode */
#define VIDEO_MODE_TV_LCD	1
#define VIDEO_MODE_TVONLY	2
#define VIDEO_MODE_LCDONLY	3
#define VIDEO_MODE_TVCAPTION	4

/* Buffer Type */
#define XV_BUF_TYPE_DMABUF  0
#define XV_BUF_TYPE_LEGACY  1

/* Data structure for XvPutImage / XvShmPutImage */
typedef struct {
	unsigned int _header; /* for internal use only */
	unsigned int _version; /* for internal use only */

	unsigned int YBuf;
	unsigned int CbBuf;
	unsigned int CrBuf;
	unsigned int BufType;
} XV_PUTIMAGE_DATA, * XV_PUTIMAGE_DATA_PTR;

static void XV_PUTIMAGE_INIT_DATA(XV_PUTIMAGE_DATA_PTR data)
{
	data->_header = XV_PUTIMAGE_HEADER;
	data->_version = XV_PUTIMAGE_VERSION;
}
