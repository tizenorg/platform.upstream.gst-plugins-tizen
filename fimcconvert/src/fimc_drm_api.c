 /*
 * FimcConvert
 *
 * Copyright (c) 2000 - 2012 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Sangchul Lee <sc11.lee@samsung.com>, Eunchul Kim <chulspro.kim@sasmsung.com>
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

#include "fimc_drm_api.h"

int
exynos_drm_ipp_set_property(int fd,
				struct drm_exynos_ipp_property *property,
				struct drm_exynos_sz *src_sz, struct drm_exynos_sz *dst_sz,
				uint32_t src_format, uint32_t dst_format,
				enum drm_exynos_ipp_cmd cmd,
				enum drm_exynos_degree dst_degree, enum drm_exynos_flip dst_flip)
{
	struct drm_exynos_pos src_crop_pos = {0, 0, src_sz->hsize, src_sz->vsize};
	struct drm_exynos_pos dst_crop_pos = {0, 0, dst_sz->hsize, dst_sz->vsize};
	int ret = 0;

	if(!property) {
		GST_ERROR("invalid argument, property is null..");
		return -1;
	}

	memset(property, 0x00, sizeof(struct drm_exynos_ipp_property));
	property->cmd = cmd;

	switch(cmd) {
	case IPP_CMD_M2M:
		property->config[EXYNOS_DRM_OPS_SRC].ops_id = EXYNOS_DRM_OPS_SRC;
		property->config[EXYNOS_DRM_OPS_SRC].flip = EXYNOS_DRM_FLIP_NONE;
		property->config[EXYNOS_DRM_OPS_SRC].degree = EXYNOS_DRM_DEGREE_0;
		property->config[EXYNOS_DRM_OPS_SRC].fmt = src_format;
		property->config[EXYNOS_DRM_OPS_SRC].pos = src_crop_pos;
		property->config[EXYNOS_DRM_OPS_SRC].sz = *src_sz;

		property->config[EXYNOS_DRM_OPS_DST].ops_id = EXYNOS_DRM_OPS_DST;
		property->config[EXYNOS_DRM_OPS_DST].flip = dst_flip;
		property->config[EXYNOS_DRM_OPS_DST].degree = dst_degree;
		property->config[EXYNOS_DRM_OPS_DST].fmt = dst_format;

		/* Bit alignment for IPP : DST */
		switch (dst_format) {
		case DRM_FORMAT_NV12MT:
			dst_sz->hsize = ALIGN_TO_16B(dst_sz->hsize);
			dst_sz->vsize = ALIGN_TO_8B(dst_sz->vsize);
			break;
		case DRM_FORMAT_NV12:
			/* NOTE: for H/W encoder issue, use ALIGN */
			dst_sz->hsize = ALIGN_TO_16B(dst_sz->hsize);
			dst_sz->vsize = ALIGN_TO_16B(dst_sz->vsize);
			dst_crop_pos.w = ALIGN_TO_2B(dst_sz->hsize);
			break;
		case DRM_FORMAT_ARGB8888:
		case DRM_FORMAT_XRGB8888:
		case DRM_FORMAT_BGRX8888:
		case DRM_FORMAT_BGRA8888:
			dst_sz->vsize = ALIGN_TO_4B(dst_sz->vsize);
			break;
		default:
			dst_sz->hsize = ALIGN_TO_16B(dst_sz->hsize);
			dst_sz->vsize = ALIGN_TO_2B(dst_sz->vsize);
			break;
		}
		property->config[EXYNOS_DRM_OPS_DST].pos = dst_crop_pos;
		property->config[EXYNOS_DRM_OPS_DST].sz = *dst_sz;
		break;

	case IPP_CMD_WB:
	case IPP_CMD_OUTPUT:
	default:
		ret = -EINVAL;
		return ret;
	}

	ret = ioctl(fd, DRM_IOCTL_EXYNOS_IPP_SET_PROPERTY, property);
	if (ret) {
		GST_ERROR("failed to DRM_IOCTL_EXYNOS_IPP_SET_PROPERTY : %s", strerror(errno));
	}

	GST_DEBUG("DRM_IOCTL_EXYNOS_IPP_SET_PROPERTY : prop_id[%d]", property->prop_id);

	return ret;
}

int
exynos_drm_ipp_queue_buf(int fd,
					enum drm_exynos_ops_id ops_id,
					enum drm_exynos_ipp_buf_type buf_type,
					int prop_id,
					int buf_id,
					uint32_t gem_handle_y, uint32_t gem_handle_cb, uint32_t gem_handle_cr)
{
	int ret = 0;
	struct drm_exynos_ipp_queue_buf qbuf;

	memset(&qbuf, 0x00, sizeof(struct drm_exynos_ipp_queue_buf));

	if (!gem_handle_y) {
		GST_ERROR("invalid argument, gem_handle_y is null..");
		return -1;
	}

	qbuf.ops_id = ops_id;
	qbuf.buf_type = buf_type;
	qbuf.user_data = 0;
	qbuf.prop_id = prop_id;
	qbuf.buf_id = buf_id;
	qbuf.handle[EXYNOS_DRM_PLANAR_Y] = gem_handle_y;
	qbuf.handle[EXYNOS_DRM_PLANAR_CB] = gem_handle_cb;
	qbuf.handle[EXYNOS_DRM_PLANAR_CR] = gem_handle_cr;

	ret = ioctl(fd, DRM_IOCTL_EXYNOS_IPP_QUEUE_BUF, &qbuf);
	if (ret) {
		GST_ERROR("failed to DRM_IOCTL_EXYNOS_IPP_QUEUE_BUF[id:%d][buf_type:%d] : %s", ops_id, buf_type, strerror(errno));
	}

	return ret;
}

int
exynos_drm_ipp_cmd_ctrl(int fd, int prop_id, enum drm_exynos_ipp_ctrl ctrl)
{
	int ret = 0;
	struct drm_exynos_ipp_cmd_ctrl cmd_ctrl;

	memset(&cmd_ctrl, 0x00, sizeof(struct drm_exynos_ipp_cmd_ctrl));

	cmd_ctrl.prop_id = prop_id;
	cmd_ctrl.ctrl = ctrl;

	GST_DEBUG("prop_id(%d), ipp ctrl cmd(%d)", prop_id, ctrl);
	ret = ioctl(fd, DRM_IOCTL_EXYNOS_IPP_CMD_CTRL, &cmd_ctrl);
	if (ret) {
		GST_ERROR("failed to DRM_IOCTL_EXYNOS_IPP_CMD_CTRL[prop_id:%d][ctrl:%d] : %s", prop_id, ctrl, strerror(errno));
	}

	return ret;
}

int
fimcconvert_drm_create_gem(int drm_fimc_fd, struct drm_exynos_gem_create *gem)
{
	int ret = 0;

	if (drm_fimc_fd < 0) {
		GST_ERROR("DRM is not opened");
		return -1;
	}
	if (!gem) {
		GST_ERROR("gem is null..");
		return -1;
	}
	/* NOTE : structure gem should have its size */
#ifdef USE_BO_TILED
	if (!gem->gsize.size && gem->gsize.tiled.width && gem->gsize.tiled.height ) {
		GST_ERROR("gem size is null..");
		return -1;
	}
#else
	if (!gem->size) {
		GST_ERROR("gem size is null..");
		return -1;
	}
#endif

#ifdef USE_CACHABLE_GEM
	gem->flags = EXYNOS_BO_CACHABLE | EXYNOS_BO_CONTIG;
#else
	gem->flags = EXYNOS_BO_NONCACHABLE | EXYNOS_BO_CONTIG;
#endif

	ret = ioctl(drm_fimc_fd, DRM_IOCTL_EXYNOS_GEM_CREATE, gem);
	if (ret < 0) {
		GST_ERROR("failed to create gem buffer: %s", g_strerror (errno));
		return -1;
	}

	GST_DEBUG("create drm gem handle(%d)", gem->handle);

	return 0;
}

int
fimcconvert_gem_flush_cache(int drm_fimc_fd)
{
	int ret = 0;
	struct drm_exynos_gem_cache_op cache_op;

	if (drm_fimc_fd < 0) {
		GST_ERROR("DRM is not opened");
		return -1;
	}

	cache_op.flags = 0;
	cache_op.flags = (EXYNOS_DRM_ALL_CACHES_CORES | EXYNOS_DRM_CACHE_FSH_ALL);

	ret = ioctl (drm_fimc_fd, DRM_IOCTL_EXYNOS_GEM_CACHE_OP, &cache_op);
	if  (ret < 0) {
		GST_ERROR("failed to gem cache op: %s", g_strerror (errno));
		return -1;
	}

	GST_DEBUG("success to gem_flush_cache");

	return 0;
}

int
fimcconvert_drm_mmap_gem(int drm_fimc_fd, struct drm_exynos_gem_mmap *mmap)
{
	int ret = 0;

	if (drm_fimc_fd < 0) {
		GST_ERROR("DRM is not opened");
		return -1;
	}
	if (!mmap) {
		GST_ERROR("mmap is null..");
		return -1;
	}
	/* NOTE : structure mmap should have its size */
	if (!mmap->size) {
		GST_ERROR("mmap size is null..");
		return -1;
	}

	ret = ioctl(drm_fimc_fd, DRM_IOCTL_EXYNOS_GEM_MMAP, mmap);
	if (ret < 0) {
		GST_ERROR("failed to mmap gem: %s", g_strerror (errno));
		return -1;
	}

	GST_DEBUG("mmapped addr(0x%x)", (void*)(unsigned long)mmap->mapped);

	return 0;
}

void
fimcconvert_drm_close_gem(int drm_fimc_fd, unsigned int gem_handle)
{
	int ret = 0;
	struct drm_gem_close close_arg = {0,};

	if (drm_fimc_fd < 0) {
		GST_ERROR("DRM is not opened");
		return;
	}
	if (gem_handle == 0) {
		GST_ERROR("invalid gem_handle(%d)",gem_handle);
		return;
	}

	close_arg.handle = gem_handle;
	ret = ioctl(drm_fimc_fd, DRM_IOCTL_GEM_CLOSE, &close_arg);
	if (ret < 0) {
		GST_ERROR("failed to DRM_IOCTL_GEM_CLOSE, gem handle(%d) : %s",gem_handle, g_strerror (errno));
		return;
	} else {
		GST_DEBUG("close drm gem handle(%d)", gem_handle);
	}

	return;
}

int
fimcconvert_drm_convert_fd_to_gemhandle(int drm_fimc_fd, int dmabuf_fd, uint32_t *gem_handle)
{
	int ret = 0;
	struct drm_prime_handle prime_arg = {0,};

	if (drm_fimc_fd < 0) {
		GST_ERROR("DRM is not opened");
		return -1;
	}
	/* some planes may have fd value 0, but keep going.. */
	if (dmabuf_fd <= 0) {
		GST_DEBUG("fd(%d) is not valid", dmabuf_fd); /* temporarily change log level to DEBUG for reducing WARNING level log */
		return 0;
	}

	prime_arg.fd = dmabuf_fd;
	ret = ioctl(drm_fimc_fd, DRM_IOCTL_PRIME_FD_TO_HANDLE, &prime_arg);
	if (ret<0) {
		GST_ERROR("failed to DRM_IOCTL_PRIME_FD_TO_HANDLE, fd(%d): %s", dmabuf_fd, g_strerror (errno));
		return -1;
	}

	GST_DEBUG("converted fd(%d) to gem_handle(%u)", dmabuf_fd, prime_arg.handle);

	*gem_handle = prime_arg.handle;

	return 0;
}

int
fimcconvert_drm_convert_gemhandle_to_fd(int drm_fimc_fd, uint32_t gem_handle, int *dmabuf_fd)
{
	int ret = 0;
	struct drm_prime_handle prime;

	if (drm_fimc_fd < 0) {
		GST_ERROR("DRM is not opened");
		return -1;
	}
	if (gem_handle <= 0) {
		GST_WARNING("gem_handle(%u) is not valid", gem_handle);
		return 0;
	}

	memset (&prime, 0, sizeof (struct drm_prime_handle));
	prime.handle = gem_handle;
	ret = ioctl(drm_fimc_fd, DRM_IOCTL_PRIME_HANDLE_TO_FD, &prime);
	if (ret<0) {
		GST_ERROR("failed to DRM_IOCTL_PRIME_HANDLE_TO_FD: %s", g_strerror (errno));
		return -1;
	}

	GST_DEBUG("converted gem_handle(%u) to fd(%d)", gem_handle, prime.fd);

	*dmabuf_fd = prime.fd;

	return 0;
}

int
fimcconvert_drm_gem_get_phy_addr(int drm_fimc_fd,  uint32_t gem_handle, uint64_t *phy_addr)
{
	int ret = 0;
	struct drm_exynos_gem_get_phy gem_dst_phy;

	if (drm_fimc_fd < 0) {
		GST_ERROR("DRM is not opened");
		return -1;
	}
	if (gem_handle <= 0) {
		GST_ERROR("gem_handle(%d) is not valid", gem_handle);
		return -1;
	}

	/* initialize structure for gem_get_phy */
	memset(&gem_dst_phy, 0x00, sizeof(struct drm_exynos_gem_get_phy));
	gem_dst_phy.gem_handle = gem_handle;

	ret = ioctl(drm_fimc_fd, DRM_IOCTL_EXYNOS_GEM_GET_PHY, &gem_dst_phy);
	if (ret < 0) {
		GST_ERROR("failed to get physical address: %s",g_strerror (errno));
		return -1;
	}

	GST_DEBUG("gem_handle=%d, phy addr=0x%x", gem_handle, (void*)(unsigned long)gem_dst_phy.phy_addr);

	*phy_addr = gem_dst_phy.phy_addr;

	return 0;
}

int
fimcconvert_drm_gem_import_phy_addr(int drm_fimc_fd, struct drm_exynos_gem_phy_imp *gem_phy_imp)
{
	int ret = 0;

	if (drm_fimc_fd < 0) {
		GST_ERROR("DRM is not opened");
		return -1;
	}
	if (!gem_phy_imp) {
		GST_ERROR("gem_phy_imp is null..");
		return -1;
	}

	if (!gem_phy_imp->phy_addr) {
		GST_ERROR("phy_addr is null.. can not execute DRM_IOCTL_EXYNOS_GEM_PHY_IMP");
		return -1;
	}
#ifdef USE_BO_TILED
	if (!gem_phy_imp->gsize.tiled.width || !gem_phy_img->gsize.tiled.height) {
#else
	if (!gem_phy_imp->size) {
#endif
		GST_ERROR("size if null..can not execute DRM_IOCTL_EXYNOS_GEM_PHY_IMP");
		return -1;
	}

	ret = ioctl(drm_fimc_fd, DRM_IOCTL_EXYNOS_GEM_PHY_IMP, gem_phy_imp);
	if (ret < 0) {
		GST_ERROR("DRM_IOCTL_EXYNOS_GEM_PHY_IMP failed : %s",g_strerror (errno));
		return -1;
	}

	GST_DEBUG("gem_handle=%d, phy addr=0x%x", gem_phy_imp->gem_handle, (void*)(unsigned long)gem_phy_imp->phy_addr);

	return 0;
}
