/* Gstreamer
 * Copyright (C) <2011> Intel Corporation
 * Copyright (C) <2011> Collabora Ltd.
 * Copyright (C) <2011> Thibault Saunier <thibault.saunier@collabora.com>
 *
 * Some bits C-c,C-v'ed and s/4/3 from h264parse and videoparsers/h264parse.c:
 *    Copyright (C) <2010> Mark Nauwelaerts <mark.nauwelaerts@collabora.co.uk>
 *    Copyright (C) <2010> Collabora Multimedia
 *    Copyright (C) <2010> Nokia Corporation
 *
 *    (C) 2005 Michal Benes <michal.benes@itonis.tv>
 *    (C) 2008 Wim Taymans <wim.taymans@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/**
 * Common code for NAL parsing from h264 and h265 parsers.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <gst/base/gstbytereader.h>
#include <gst/base/gstbitreader.h>
#include <string.h>

guint ceil_log2 (guint32 v);

typedef struct
{
  const guint8 *data;
  guint size;

  guint n_epb;                  /* Number of emulation prevention bytes */
  guint byte;                   /* Byte position */
  guint bits_in_cache;          /* bitpos in the cache of next bit */
  guint8 first_byte;
  guint64 cache;                /* cached bytes */
} WFDNalReader;

void wfd_nal_reader_init (WFDNalReader * nr, const guint8 * data, guint size);

gboolean wfd_nal_reader_read (WFDNalReader * nr, guint nbits);
gboolean wfd_nal_reader_skip (WFDNalReader * nr, guint nbits);
gboolean wfd_nal_reader_skip_long (WFDNalReader * nr, guint nbits);
guint wfd_nal_reader_get_pos (const WFDNalReader * nr);
guint wfd_nal_reader_get_remaining (const WFDNalReader * nr);
guint wfd_nal_reader_get_epb_count (const WFDNalReader * nr);

gboolean wfd_nal_reader_is_byte_aligned (WFDNalReader * nr);
gboolean wfd_nal_reader_has_more_data (WFDNalReader * nr);

#define WFD_NAL_READER_READ_BITS_H(bits) \
gboolean wfd_nal_reader_get_bits_uint##bits (WFDNalReader *nr, guint##bits *val, guint nbits)

WFD_NAL_READER_READ_BITS_H (8);
WFD_NAL_READER_READ_BITS_H (16);
WFD_NAL_READER_READ_BITS_H (32);

#define WFD_NAL_READER_PEEK_BITS_H(bits) \
gboolean wfd_nal_reader_peek_bits_uint##bits (const WFDNalReader *nr, guint##bits *val, guint nbits)

WFD_NAL_READER_PEEK_BITS_H (8);

gboolean wfd_nal_reader_get_ue (WFDNalReader * nr, guint32 * val);
gboolean wfd_nal_reader_get_se (WFDNalReader * nr, gint32 * val);

#define CHECK_ALLOWED(val, min, max) { \
  if (val < min || val > max) { \
    GST_WARNING ("value not in allowed range. value: %d, range %d-%d", \
                     val, min, max); \
    goto error; \
  } \
}

#define READ_UINT8(nr, val, nbits) { \
  if (!wfd_nal_reader_get_bits_uint8 (nr, &val, nbits)) { \
    GST_WARNING ("failed to read uint8, nbits: %d", nbits); \
    goto error; \
  } \
}

#define READ_UINT16(nr, val, nbits) { \
  if (!wfd_nal_reader_get_bits_uint16 (nr, &val, nbits)) { \
  GST_WARNING ("failed to read uint16, nbits: %d", nbits); \
    goto error; \
  } \
}

#define READ_UINT32(nr, val, nbits) { \
  if (!wfd_nal_reader_get_bits_uint32 (nr, &val, nbits)) { \
  GST_WARNING ("failed to read uint32, nbits: %d", nbits); \
    goto error; \
  } \
}

#define READ_UINT64(nr, val, nbits) { \
  if (!wfd_nal_reader_get_bits_uint64 (nr, &val, nbits)) { \
    GST_WARNING ("failed to read uint32, nbits: %d", nbits); \
    goto error; \
  } \
}

#define READ_UE(nr, val) { \
  if (!wfd_nal_reader_get_ue (nr, &val)) { \
    GST_WARNING ("failed to read UE"); \
    goto error; \
  } \
}

#define READ_UE_ALLOWED(nr, val, min, max) { \
  guint32 tmp; \
  READ_UE (nr, tmp); \
  CHECK_ALLOWED (tmp, min, max); \
  val = tmp; \
}

#define READ_SE(nr, val) { \
  if (!wfd_nal_reader_get_se (nr, &val)) { \
    GST_WARNING ("failed to read SE"); \
    goto error; \
  } \
}

#define READ_SE_ALLOWED(nr, val, min, max) { \
  gint32 tmp; \
  READ_SE (nr, tmp); \
  CHECK_ALLOWED (tmp, min, max); \
  val = tmp; \
}

gint scan_for_start_codes (const guint8 * data, guint size);
