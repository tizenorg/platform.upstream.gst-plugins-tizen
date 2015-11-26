/*
 * drmdecryptor
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd. All rights reserved.
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
#ifndef __GST_DRM_DECRYPTOR_H__
#define __GST_DRM_DECRYPTOR_H__

#define VERSION "1.0"
#ifdef PACKAGE
#undef PACKAGE
#endif
#define PACKAGE "gstplugindrmdecryptor"
#include <gst/gst.h>
#include <gst/gstelement.h>
#include <glib.h>

G_BEGIN_DECLS

#define GST_DRM_DECRYPTOR(obj) (GstDrmDecryptor *) (obj)
#define GST_DRM_DECRYPTOR_TYPE (gst_drm_decryptor_get_type ())

typedef struct _GstDrmDecryptor GstDrmDecryptor;
typedef struct _GstDrmDecryptorClass GstDrmDecryptorClass;

enum
{
  PROP_0,
};

struct _GstDrmDecryptor
{
  GstElement     element;
  GstPad        *sinkpad;
  GstPad        *srcpad;

  /*< private >*/
  /* TODO add your private data here */
};

struct _GstDrmDecryptorClass
{
  GstElementClass parent_class;
};

GType gst_drm_decryptor_get_type (void);

G_END_DECLS

#endif /* __GST_DRM_DECRYPTOR_H__ */

