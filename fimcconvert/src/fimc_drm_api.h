#ifndef __FIMC_H__
#define __FIMC_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <gst/gstinfo.h>

/* headers for drm and gem */
#include <exynos_drm.h>
#include <drm_fourcc.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <X11/Xmd.h>
#include <dri2/dri2.h>
#include <libdrm/drm.h>

/* tbm */
#include <tbm_bufmgr.h>

/* ioctl */
#include <sys/ioctl.h>

GST_DEBUG_CATEGORY_EXTERN(fimcconvert_debug);
#define GST_CAT_DEFAULT fimcconvert_debug
//#define USE_CACHABLE_GEM

#if 1
/* 2 non contiguous plane YCbCr */
#define fourcc_code(a,b,c,d) ((uint32_t)(a) | ((uint32_t)(b) << 8) | \
			      ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24))
#define DRM_FORMAT_NV12M	fourcc_code('N', 'M', '1', '2') /* 2x2 subsampled Cr:Cb plane */
#define DRM_FORMAT_NV12MT	fourcc_code('T', 'M', '1', '2') /* 2x2 subsampled Cr:Cb plane 64x32 macroblocks */
#endif

/* macro for align ***********************************************************/
#define ALIGN_TO_2B(x)		((((x)  + (1 <<  1) - 1) >>  1) <<  1)
#define ALIGN_TO_4B(x)		((((x)  + (1 <<  2) - 1) >>  2) <<  2)
#define ALIGN_TO_8B(x)		((((x)  + (1 <<  3) - 1) >>  3) <<  3)
#define ALIGN_TO_16B(x)		((((x)  + (1 <<  4) - 1) >>  4) <<  4)
#define ALIGN_TO_32B(x)		((((x)  + (1 <<  5) - 1) >>  5) <<  5)
#define ALIGN_TO_128B(x)	((((x)  + (1 <<  7) - 1) >>  7) <<  7)
#define ALIGN_TO_2KB(x)		((((x)  + (1 << 11) - 1) >> 11) << 11)
#define ALIGN_TO_8KB(x)		((((x)  + (1 << 13) - 1) >> 13) << 13)
#define ALIGN_TO_64KB(x)	((((x)  + (1 << 16) - 1) >> 16) << 16)

int exynos_drm_ipp_set_property(int fd, struct drm_exynos_ipp_property *property, struct drm_exynos_sz *src_sz, struct drm_exynos_sz *dst_sz, uint32_t src_format, uint32_t dst_format, enum drm_exynos_ipp_cmd cmd, enum drm_exynos_degree degree, enum drm_exynos_flip flip);
int exynos_drm_ipp_queue_buf(int fd, enum drm_exynos_ops_id ops_id, enum drm_exynos_ipp_buf_type buf_type, int prop_id, int buf_id, uint32_t gem_handle_y, uint32_t gem_handle_cb, uint32_t gem_handle_cr);
int exynos_drm_ipp_cmd_ctrl(int fd, int prop_id, enum drm_exynos_ipp_ctrl ctrl);
int fimcconvert_drm_create_gem(int drm_fimc_fd, struct drm_exynos_gem_create *gem);
void fimcconvert_drm_close_gem(int drm_fimc_fd, unsigned int gem_handle);
int fimcconvert_drm_convert_fd_to_gemhandle(int drm_fimc_fd, int dmabuf_fd,  uint32_t *gem_handle);
int fimcconvert_drm_convert_gemhandle_to_fd(int drm_fimc_fd, uint32_t gem_handle, int *dmabuf_fd);
int fimcconvert_drm_gem_get_phy_addr(int drm_fimc_fd,  uint32_t gem_handle, uint64_t *phy_addr);
int fimcconvert_drm_gem_import_phy_addr(int drm_fimc_fd, struct drm_exynos_gem_phy_imp *gem_phy_imp);
int fimcconvert_drm_mmap_gem(int drm_fimc_fd, struct drm_exynos_gem_mmap *mmap);
int fimcconvert_gem_flush_cache(int drm_fimc_fd);
#endif
