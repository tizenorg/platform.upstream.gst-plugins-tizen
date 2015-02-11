/*
 * drmsrc
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: JongHyuk Choi <jhchoi.choi@samsung.com>
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
#include "gstdrmsrc.h"

#define LOG_TRACE(message)  //g_print("DRM_SRC: %s: %d: %s - %s \n", __FILE__, __LINE__, __FUNCTION__, message);

#define GST_TAG_PLAYREADY "playready_file_path"

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src", GST_PAD_SRC, GST_PAD_ALWAYS,GST_STATIC_CAPS_ANY);


GST_DEBUG_CATEGORY_STATIC (gst_drm_src_debug);
#define GST_CAT_DEFAULT gst_drm_src_debug

enum
{
	ARG_0,
	ARG_LOCATION,
	ARG_FD
};
static void gst_drm_src_finalize (GObject * object);
static void gst_drm_src_set_property (GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_drm_src_get_property (GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);
static gboolean gst_drm_src_start (GstBaseSrc * basesrc);
static gboolean gst_drm_src_stop (GstBaseSrc * basesrc);
static gboolean gst_drm_src_is_seekable (GstBaseSrc * src);
static gboolean gst_drm_src_get_size (GstBaseSrc * src, guint64 * size);
static GstFlowReturn gst_drm_src_create (GstBaseSrc * src, guint64 offset, guint length, GstBuffer ** buffer);
static void gst_drm_src_uri_handler_init (gpointer g_iface, gpointer iface_data);

G_DEFINE_TYPE_WITH_CODE (GstDrmSrc, gst_drm_src, GST_TYPE_BASE_SRC,
                         G_IMPLEMENT_INTERFACE(GST_TYPE_URI_HANDLER, gst_drm_src_uri_handler_init));
/**
 * This function does the following:
 *  1. Sets the class details
 *  2. Adds the source pad template
 *
 * @param   g_class    [out]   gpointer
 *
 * @return  void
 */
static void gst_drm_src_base_init (gpointer g_class)
{
}
/**
 * This function does the following:
 *  1. Installs the properties
 *  2. Assigns the function pointers GObject class attributes
 *
 * @param   klass    [out]   GstDrmSrcClass Structure
 *
 * @return  void
 */
static void gst_drm_src_class_init (GstDrmSrcClass * klass)
{
	GObjectClass *gobject_class;
	GstElementClass *gstelement_class;
	GstBaseSrcClass *gstbasesrc_class;
	gobject_class = G_OBJECT_CLASS (klass);
        gstelement_class= GST_ELEMENT_CLASS (klass);
	gstbasesrc_class = GST_BASE_SRC_CLASS (klass);
	// Assigns the function pointers GObject class attributes
	gobject_class->set_property = gst_drm_src_set_property;
	gobject_class->get_property = gst_drm_src_get_property;
	//  1. Installs the properties
	g_object_class_install_property (gobject_class, ARG_FD,
		g_param_spec_int ("fd", "File-descriptor",
		"File-descriptor for the file being mmap()d", 0, G_MAXINT, 0,
		G_PARAM_READABLE));
	g_object_class_install_property (gobject_class, ARG_LOCATION,
		g_param_spec_string ("location", "File Location",
		"Location of the file to read", NULL, G_PARAM_READWRITE));

	// 2. Sets the class details
	gst_element_class_set_details_simple (gstelement_class,
		"DRM Source",
		"Source/File",
		"Read from arbitrary point in a standard/DRM file",
		"Kishore Arepalli  <kishore.a@samsung.com> and Sadanand Dodawadakar <sadanand.d@samsung.com>");
      // 3. Adds the source pad template
	gst_element_class_add_pad_template (gstelement_class, gst_static_pad_template_get (&srctemplate));
	// 4. Assigns the function pointers GObject class attributes
	gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_drm_src_finalize);
	gstbasesrc_class->start = GST_DEBUG_FUNCPTR (gst_drm_src_start);
	gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (gst_drm_src_stop);
	gstbasesrc_class->is_seekable = GST_DEBUG_FUNCPTR (gst_drm_src_is_seekable);
	gstbasesrc_class->get_size = GST_DEBUG_FUNCPTR (gst_drm_src_get_size);
	gstbasesrc_class->create = GST_DEBUG_FUNCPTR (gst_drm_src_create);


	gst_tag_register (GST_TAG_PLAYREADY, GST_TAG_FLAG_META,
			G_TYPE_STRING,
			"PlayReady File Path",
			"a tag that is specific to PlayReady File",
			NULL);
}
/**
 * This function does the following:
 *  1. Initilizes the parameters of GstDrmSrc
 *
 * @param   src    [out]   GstDrmSrc structure
 * @param   g_class    [in]   GstDrmSrcClass structure
 *
 * @return  gboolean   Returns TRUE on success and FALSE on ERROR
 */
static void gst_drm_src_init (GstDrmSrc * src)
{
	// 1. Initilizes the parameters of GstDrmSrc
	src->filename = NULL;
	src->fd = 0;
	src->uri = NULL;
	src->is_regular = FALSE;
	src->seekable = FALSE;
}
/**
 * This function does the following:
 *  1. deallocates the filename and uri
 *  2. calls the parent class->finalize
 *
 * @param   object    [in]   GObject Structure
 *
 * @return  void
 */
static void gst_drm_src_finalize (GObject * object)
{
	GstDrmSrc *src;

	src = GST_DRM_SRC (object);
	//  1. deallocates the filename and uri
	g_free (src->filename);
	g_free (src->uri);
	// 2. calls the parent class->finalize
	G_OBJECT_CLASS (gst_drm_src_parent_class)->finalize (object);
}
/**
 * This function does the following:
 *  1. Checks the state
 *  2. Checks the filename
 *  3. Sets the filename
 *
 * @param   src    [in]   GstDrmSrc Structure
 * @param   location    [in]   location of the file
 *
 * @return  gboolean   Returns TRUE on success and FALSE on ERROR
 */
static gboolean gst_drm_src_set_location (GstDrmSrc * src, const gchar * location)
{
	GstState state;

	GST_OBJECT_LOCK (src);
	//  1. Checks the state
	state = GST_STATE (src);
	if (state != GST_STATE_READY && state != GST_STATE_NULL)
	{
		GST_DEBUG_OBJECT (src, "setting location in wrong state");
		GST_OBJECT_UNLOCK (src);
		return FALSE;
	}
	GST_OBJECT_UNLOCK (src);
	g_free (src->filename);
	g_free (src->uri);
	//  2. Checks the filename
	if (location == NULL)
	{
		src->filename = NULL;
		src->uri = NULL;
	}
	else
	{
		// 3. Sets the filename
		src->filename = g_strdup (location);
		src->uri = gst_uri_construct ("file", src->filename);
	}
	g_object_notify (G_OBJECT (src), "location");
	return TRUE;
}
/**
 * This function does the following:
 *  1. Sets the location of the file.
 *
 * @param   object    [in]   GObject Structure
 * @param   prop_id    [in]   id of the property
 * @param   value    [in]   property value
 * @param   pspec    [in]  GParamSpec Structure
 *
 * @return  void
 */
static void gst_drm_src_set_property (GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec)
{
	GstDrmSrc *src;

	g_return_if_fail (GST_IS_DRM_SRC (object));
	src = GST_DRM_SRC (object);
	switch (prop_id)
	{
		//  1. Sets the location of the file.
		case ARG_LOCATION:
			gst_drm_src_set_location (src, g_value_get_string (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}
/**
 * This function does the following:
 *  1. Provides the location of the file.
 *  2. Provides the file descriptor.
 *
 * @param   object    [in]   GObject Structure
 * @param   prop_id    [in]   id of the property
 * @param   value    [out]   property value
 * @param   pspec    [in]  GParamSpec Structure
 *
 * @return  void
 */
static void gst_drm_src_get_property (GObject * object, guint prop_id, GValue * value,GParamSpec * pspec)
{
	GstDrmSrc *src;

	g_return_if_fail (GST_IS_DRM_SRC (object));
	src = GST_DRM_SRC (object);
	switch (prop_id)
	{
		//  1. Provides the location of the file.
		case ARG_LOCATION:
			g_value_set_string (value, src->filename);
			break;
		// 2. Provides the file descriptor.
		case ARG_FD:
			g_value_set_int (value, src->fd);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

/**
 * This function does the following:
 *  1. Seeks to the specified position.
 *  2. Allocates a buffer to push the data
 *  3. Reads from the file and sets the related params
 *
 * @param   src    [in]   GstDrmSrc Structure
 * @param   offset    [in]   offset of the file to seek
 * @param   length    [in]   size of the data in bytes
 * @param   buffer    [out]   GstBuffer to hold the contents
 *
 * @return  GstFlowReturn   Returns GST_FLOW_OK on success and ERROR on failure
 */
static GstFlowReturn gst_drm_src_create_read (GstDrmSrc * src, guint64 offset, guint length, GstBuffer ** buffer)
{
	int ret;
	GstBuffer *buf;
	GstMapInfo buf_info = GST_MAP_INFO_INIT;
	// 1. Seeks to the specified position.
	if (G_UNLIKELY (src->read_position != offset))
	{
		off_t res;
		res = lseek (src->fd, offset, SEEK_SET);
		if (G_UNLIKELY (res < 0 || res != offset))
		{
			GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL), GST_ERROR_SYSTEM);
			return GST_FLOW_ERROR;
		}
		src->read_position = offset;
	}
	// 2. Allocates a buffer to push the data
	buf = gst_buffer_new_and_alloc (length);
	GST_LOG_OBJECT (src, "Reading %d bytes", length);
	// 3. Reads from the file and sets the related params
	gst_buffer_map(buf, &buf_info, GST_MAP_WRITE);
	ret = read (src->fd, buf_info.data, length);
	gst_buffer_unmap(buf, &buf_info);
	if (G_UNLIKELY (ret < 0))
	{
		GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL), GST_ERROR_SYSTEM);
		gst_buffer_unref (buf);
		return GST_FLOW_ERROR;
	}
	if (G_UNLIKELY ((guint) ret < length && src->seekable))
	{
		GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL),("unexpected end of file."));
		gst_buffer_unref (buf);
		return GST_FLOW_ERROR;
	}
	if (G_UNLIKELY (ret == 0 && length > 0))
	{
		GST_DEBUG ("non-regular file hits EOS");
		gst_buffer_unref (buf);
		return GST_FLOW_EOS;
	}
	length = ret;
	GST_BUFFER_OFFSET (buf) = offset;
	GST_BUFFER_OFFSET_END (buf) = offset + length;
	*buffer = buf;
	src->read_position += length;
	return GST_FLOW_OK;
}
/**
 * This function does the following:
 *  1. Calls DRM file read chain method for drm files.
 *  2. Calls normal file read chain method for standard files.
 *
 * @param   basesrc    [in]   BaseSrc Structure
 * @param   size    [out]   Size of the file
 *
 * @return  gboolean   Returns TRUE on success and FALSE on ERROR
 */
static GstFlowReturn gst_drm_src_create (GstBaseSrc * basesrc, guint64 offset, guint length, GstBuffer ** buffer)
{
	GstDrmSrc *src = GST_DRM_SRC (basesrc);

	// 1. Calls DRM file read chain method for drm files.

	// 2. Calls normal file read chain method for standard files.
	return gst_drm_src_create_read (src, offset, length, buffer);
}
/**
 *
 * @param   basesrc    [in]   BaseSrc Structure
 *
 * @return  gboolean   Returns TRUE if the file is seekable and FALSE if the file is not seekable
 */
static gboolean gst_drm_src_is_seekable (GstBaseSrc * basesrc)
{
	GstDrmSrc *src = GST_DRM_SRC (basesrc);
	return src->seekable;
}
/**
 * This function does the following:
 *  1. Gets the filesize for drm file by using seek oprations
 *  2. Gets the file size for standard file by using statistics
 *
 * @param   basesrc    [in]   BaseSrc Structure
 * @param   size    [in]   Size of the file
 *
 * @return  gboolean   Returns TRUE on success and FALSE on ERROR
 */
static gboolean gst_drm_src_get_size (GstBaseSrc * basesrc, guint64 * size)
{
	struct stat stat_results;
	GstDrmSrc *src = GST_DRM_SRC (basesrc);
	unsigned int offset;

	//  1. Gets the filesize for drm file by using seek oprations

	// 2. Gets the file size for standard file by using statistics
	if (fstat (src->fd, &stat_results) < 0)
		return FALSE;
	*size = stat_results.st_size;
	return TRUE;
}
/**
 * This function does the following:
 *  1. Checks the filename
 *  2. Opens the file and check statistics of the file
 *  7. Checks the seeking for standard files
 *
 * @param   basesrc    [in]   BaseSrc Structure
 *
 * @return  gboolean   Returns TRUE on success and FALSE on ERROR
 */
static gboolean gst_drm_src_start (GstBaseSrc * basesrc)
{
	GstDrmSrc *src = GST_DRM_SRC (basesrc);
	struct stat stat_results;
	off_t ret;
	// 1. Checks the filename
	if (src->filename == NULL || src->filename[0] == '\0')
	{
		GST_ELEMENT_ERROR (src, RESOURCE, NOT_FOUND,("No file name specified for reading."), (NULL));
		return FALSE;
	}
	// 2. Opens the file and check statistics of the file
	GST_INFO_OBJECT (src, "opening file %s", src->filename);
	src->fd = open (src->filename, O_RDONLY | O_BINARY);
	if (src->fd < 0)
	{
		if(errno == ENOENT)
		{
			GST_ELEMENT_ERROR (src, RESOURCE, NOT_FOUND, (NULL),("No such file \"%s\"", src->filename));
			return FALSE;
		}
		GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, ("Could not open file \"%s\" for reading.", src->filename), GST_ERROR_SYSTEM);
		return FALSE;
	}
	if (fstat (src->fd, &stat_results) < 0)
	{
		GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, ("Could not get info on \"%s\".", src->filename), (NULL));
		close (src->fd);
		return FALSE;
	}
	if (S_ISDIR (stat_results.st_mode))
	{
		GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, ("\"%s\" is a directory.", src->filename), (NULL));
		close (src->fd);
		return FALSE;
	}
	if (S_ISSOCK (stat_results.st_mode))
	{
		GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, ("File \"%s\" is a socket.", src->filename), (NULL));
		close (src->fd);
		return FALSE;
	}
	src->read_position = 0;

	// 7. Checks the seeking for standard files
	if (S_ISREG (stat_results.st_mode))
		src->is_regular = TRUE;
	ret = lseek (src->fd, 0, SEEK_END);
	if (ret < 0)
	{
		GST_LOG_OBJECT (src, "disabling seeking, not in mmap mode and lseek "
			"failed: %s", g_strerror (errno));
		src->seekable = FALSE;
	}
	else
	{
		src->seekable = TRUE;
	}
	lseek (src->fd, 0, SEEK_SET);
	src->seekable = src->seekable && src->is_regular;
	return TRUE;
}
/**
 * This function does the following:
 *  1. Closes the file desciptor and resets the flags
 *
 * @param   basesrc    [in]   BaseSrc Structure
 *
 * @return  gboolean   Returns TRUE on success and FALSE on ERROR
 */
static gboolean gst_drm_src_stop (GstBaseSrc * basesrc)
{
	GstDrmSrc *src = GST_DRM_SRC (basesrc);

	// 1. Closes the file desciptor and resets the flags
	if(src->fd > 0)
		close (src->fd);
	src->fd = 0;
	src->is_regular = FALSE;
	return TRUE;
}
/**
 *
 * @param   void
 *
 * @return  GstURIType   Returns GST_URI_SRC
 */

static GstURIType gst_drm_src_uri_get_type (GType type)
{
	return GST_URI_SRC;
}

/**
 * This function does the following:
 *  1. Defines the list of protocols
 *
 * @param   void
 *
 * @return  gchar **   Returns the protocol list
 */

static const gchar * const* gst_drm_src_uri_get_protocols (GType type)
{
    static const gchar *protocols[] = { "file", NULL };
	return protocols;
}
/**
 *
 * @param   handler [in] GstURIHandler structure
 *
 * @return  gchar*   Returns the uri
 */
static gchar * gst_drm_src_uri_get_uri (GstURIHandler *handler)
{
	GstDrmSrc *src = GST_DRM_SRC (handler);
    return g_strdup(src->uri);
}
/**
 * This function does the following:
 *  1. Checks the protocol
 *  2. Checks the whether it is absolute or not
 *  3 sets the location
 *
 * @param   handler [in] GstURIHandler structure
 * @param   uri [in] uri string
 *
 * @return  gboolean   Returns TRUE on success and FALSE on Error
 */
static gboolean gst_drm_src_uri_set_uri (GstURIHandler *handler, const gchar * uri,GError ** error)
{
	gchar *protocol, *location;
	gboolean ret;
	GstDrmSrc *src = GST_DRM_SRC (handler);
	// 1. Checks the protocol
	protocol = gst_uri_get_protocol (uri);
	if (strcmp (protocol, "file") != 0)
	{
		g_free (protocol);
		return FALSE;
	}
	g_free (protocol);
	if (g_str_has_prefix (uri, "file://localhost/"))
	{
		char *tmp;
		tmp = g_strconcat ("file://", uri + 16, NULL);
		location = gst_uri_get_location (tmp);
		g_free (tmp);
	}
	else if (strcmp (uri, "file://") == 0)
	{
		gst_drm_src_set_location (src, NULL);
		return TRUE;
	}
	else
	{
		location = gst_uri_get_location (uri);
	}
	if (!location)
		return FALSE;
	// 2. Checks the whether it is absolute or not
	if (!g_path_is_absolute (location))
	{
		g_free (location);
		return FALSE;
	}
	// 3 sets the location
	ret = gst_drm_src_set_location (src, location);
	g_free (location);
	return ret;
}
/**
 * This function does the following:
 *  1. Assignes the function pointer for URI related stuff
 *
 * @param   g_iface [in] an interface to URI handler
 * @param   iface_data [in] a gpointer
 *
 * @return  void
 */
static void gst_drm_src_uri_handler_init (gpointer g_iface, gpointer iface_data)
{
	GstURIHandlerInterface *iface = (GstURIHandlerInterface *) g_iface;
	// 1. Assigning the function pointer for URI related stuff
	iface->get_type = gst_drm_src_uri_get_type;
	iface->get_protocols = gst_drm_src_uri_get_protocols;
	iface->get_uri = gst_drm_src_uri_get_uri;
	iface->set_uri = gst_drm_src_uri_set_uri;
}
/**
 * This function does the following:
 *  1. Registers an element as drmsrc
 *
 * @param   i_pPlugin [in] a plug-in structure
 *
 * @return  gboolean TRUE on SUCCESS and FALSE on Error
 */
static gboolean plugin_init(GstPlugin* i_pPlugin)
{
	return gst_element_register(i_pPlugin, "drmsrc", GST_RANK_NONE, GST_TYPE_DRM_SRC);;
}
/**
 * This function does the following:
 *  1. plugin defination
 *
 */
GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
						 GST_VERSION_MINOR,
						 drmsrc,
						 "Plugin to read data from standad/DRM File",
						 plugin_init,
						 VERSION,
						 "LGPL",
						 "Samsung Electronics Co",
						 "http://www.samsung.com/")

