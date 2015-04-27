#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <stdlib.h>
//#include <cairo.h>
#include <sys/syscall.h>
#include <poll.h>

#include <glib.h>
#include <glib-unix.h>
#include <glib-object.h>
#include <wayland-client.h>
#include "main.h"
#include "protocol/tizen-screenshooter-client-protocol.h"

#define BUFFER_COUNT    3

struct mirror_output
{
    struct wl_output *output;
    int width, height, offset_x, offset_y;
    struct wl_list link;
};

struct mirror_buffer
{
    struct wl_buffer *buffer;
    int width, height;
    void *data;
    int queued;
    struct wl_list link;
};

static struct wl_display *display;
static struct wl_event_queue *queue;
static struct wl_shm *shm;
static struct tizen_screenshooter *shooter;
static struct tizen_screenmirror *mirror;
static struct wl_list output_list;
static struct wl_list buffer_list;

static int exit_thread = 0;

static gboolean
on_sig_timeout (gpointer user_data)
{
  GMainLoop *loop = user_data;
  exit_thread = 1;
  g_main_loop_quit (loop);
  return G_SOURCE_REMOVE;
}

static void
do_something(struct mirror_buffer *mbuffer)
{
#if 0
    printf("@@@ %s(%d) tid(%ld) dump image \n", __FUNCTION__, __LINE__, syscall(SYS_gettid));

    static int num = 0;
    char file[128];
    int buffer_stride;
    cairo_surface_t *surface;

    exit_if_fail(mbuffer->queued == 0);

    snprintf (file, 128, "mirror-%04d.png", num++);

    buffer_stride = mbuffer->width * 4;

    surface = cairo_image_surface_create_for_data(mbuffer->data,
              CAIRO_FORMAT_ARGB32,
              mbuffer->width, mbuffer->height, buffer_stride);
    cairo_surface_write_to_png(surface, file);
    cairo_surface_destroy(surface);
#else
    return;
#endif
}

static void *
xmalloc(size_t size)
{
    void *p;

    p = malloc(size);
    if (p == NULL)
    {
        fprintf(stderr, "out of memory\n");
        exit(EXIT_FAILURE);
    }

    return p;
}

static void
mirror_handle_dequeued(void *data,
                       struct tizen_screenmirror *tizen_screenmirror,
                       struct wl_buffer *buffer)
{
    struct mirror_buffer *mbuffer;

    wl_list_for_each(mbuffer, &buffer_list, link)
    {

        if (mbuffer->buffer != buffer)
            continue;

        mbuffer->queued = 0;
        printf("@@@ %s(%d) tid(%ld) buffer(%d) dequeued\n", __FUNCTION__, __LINE__,
               syscall(SYS_gettid), wl_proxy_get_id((struct wl_proxy *)buffer));

        /* buffer contains a image from the first output */
        do_something(mbuffer);

        tizen_screenmirror_queue (mirror, buffer);
        wl_display_sync(display);

        mbuffer->queued = 1;
        printf("@@@ %s(%d) tid(%ld) buffer(%d) queued\n", __FUNCTION__, __LINE__,
               syscall(SYS_gettid), wl_proxy_get_id((struct wl_proxy *)buffer));

        wl_display_sync(display);

        break;
    }
}

static void
mirror_handle_content(void *data,
                      struct tizen_screenmirror *tizen_screenmirror,
                      uint32_t content)
{
    switch(content)
    {
    case TIZEN_SCREENMIRROR_CONTENT_NORMAL:
        printf("@@@ %s(%d) tid(%ld) normal content \n", __FUNCTION__, __LINE__, syscall(SYS_gettid));
        break;
    case TIZEN_SCREENMIRROR_CONTENT_VIDEO:
        printf("@@@ %s(%d) tid(%ld) video content \n", __FUNCTION__, __LINE__, syscall(SYS_gettid));
        break;
    }
}

static void
mirror_handle_stop(void *data, struct tizen_screenmirror *tizen_screenmirror)
{
    printf("@@@ %s(%d) tid(%ld)\n", __FUNCTION__, __LINE__, syscall(SYS_gettid));
}

static const struct tizen_screenmirror_listener mirror_listener =
{
    mirror_handle_dequeued,
    mirror_handle_content,
    mirror_handle_stop
};

static struct mirror_output*
find_output(void)
{
    struct mirror_output *moutput;

    wl_list_for_each(moutput, &output_list, link)
    {
        /* return first output */
        return moutput;
    }

    return NULL;
}

static void
display_handle_geometry(void *data,
                        struct wl_output *wl_output,
                        int x,
                        int y,
                        int physical_width,
                        int physical_height,
                        int subpixel,
                        const char *make,
                        const char *model,
                        int transform)
{
    struct mirror_output *moutput = wl_output_get_user_data(wl_output);

    exit_if_fail(wl_output == moutput->output);

    moutput->offset_x = x;
    moutput->offset_y = y;
}

static void
display_handle_mode(void *data,
                    struct wl_output *wl_output,
                    uint32_t flags,
                    int width,
                    int height,
                    int refresh)
{
    struct mirror_output *moutput = wl_output_get_user_data(wl_output);

    exit_if_fail(wl_output == moutput->output);

    if (flags & WL_OUTPUT_MODE_CURRENT)
    {
        moutput->width = width;
        moutput->height = height;
    }
}

static const struct wl_output_listener output_listener =
{
    display_handle_geometry,
    display_handle_mode
};

static void
handle_global(void *data, struct wl_registry *registry,
              uint32_t name, const char *interface, uint32_t version)
{
    static struct mirror_output *moutput;

    if (strcmp(interface, "wl_output") == 0)
    {
        moutput = xmalloc(sizeof *moutput);
        moutput->output = wl_registry_bind(registry, name, &wl_output_interface, 1);
        exit_if_fail (moutput->output != NULL);

        wl_output_add_listener(moutput->output, &output_listener, moutput);

        wl_list_insert(&output_list, &moutput->link);
    }
    else if (strcmp(interface, "wl_shm") == 0)
    {
        shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
        exit_if_fail (shm != NULL);
    }
    else if (strcmp(interface, "tizen_screenshooter") == 0)
    {
        shooter = wl_registry_bind(registry, name, &tizen_screenshooter_interface, 1);
        exit_if_fail (shooter != NULL);

        moutput = find_output();
        exit_if_fail (moutput != NULL);

        mirror = tizen_screenshooter_get_screenmirror(shooter, moutput->output);
        exit_if_fail (mirror != NULL);

        tizen_screenmirror_add_listener(mirror, &mirror_listener, NULL);
    }
}

static void
handle_global_remove(void *data, struct wl_registry *registry, uint32_t name)
{
}

static const struct wl_registry_listener registry_listener =
{
    handle_global,
    handle_global_remove
};

static int
create_anonymous_file(off_t size)
{
    static const char template[] = "/shooter-XXXXXX";
    const char *path;
    char *name;
    int fd;
    int ret;

    path = getenv("XDG_RUNTIME_DIR");
    if (!path)
    {
        errno = ENOENT;
        return -1;
    }

    name = malloc(strlen(path) + sizeof(template));
    if (!name)
        return -1;

    strcpy(name, path);
    strcat(name, template);

    fd = mkstemp(name);
    if (fd >= 0)
        unlink(name);

    free(name);

    if (fd < 0)
        return -1;

    ret = ftruncate(fd, size);
    if (ret < 0)
    {
        close(fd);
        return -1;
    }

    return fd;
}

static struct mirror_buffer *
create_shm_buffer(int width, int height)
{
    struct mirror_buffer *mbuffer;
    struct wl_shm_pool *pool;
    int fd, size, stride;
    void *data;

    mbuffer = xmalloc(sizeof *mbuffer);

    stride = width * 4;
    size = stride * height;

    fd = create_anonymous_file(size);
    if (fd < 0)
    {
        fprintf(stderr, "creating a buffer file for %d B failed: %m\n",
                size);
        free(mbuffer);
        return NULL;
    }

    data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED)
    {
        fprintf(stderr, "mmap failed: %m\n");
        free(mbuffer);
        close(fd);
        return NULL;
    }

    pool = wl_shm_create_pool(shm, fd, size);
    close(fd);
    mbuffer->buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride,
            WL_SHM_FORMAT_XRGB8888);
    wl_shm_pool_destroy(pool);

    mbuffer->width = width;
    mbuffer->height = height;
    mbuffer->data = data;

    return mbuffer;
}

static gpointer
thread_main (gpointer data)
{
    printf("@@@ %s(%d) tid(%ld)\n", __FUNCTION__, __LINE__, syscall(SYS_gettid));
    struct mirror_buffer *mbuffer;
    struct mirror_output *moutput;
    struct pollfd pfd;
    int i;

    moutput = find_output();

    for (i = 0; i < BUFFER_COUNT; i++)
    {
        mbuffer = create_shm_buffer(moutput->width, moutput->height);
        wl_list_insert(&buffer_list, &mbuffer->link);

        tizen_screenmirror_queue (mirror, mbuffer->buffer);
        mbuffer->queued = 1;
    }

    tizen_screenmirror_start(mirror);
    wl_display_roundtrip(display);

    printf("@@@ %s(%d) tid(%ld) screenmirror start \n", __FUNCTION__, __LINE__, syscall(SYS_gettid));

    pfd.fd = wl_display_get_fd(display);
    pfd.events = POLLIN;

    /* thread loop */
    while (!exit_thread)
    {
        while (wl_display_prepare_read_queue (display, queue) != 0)
            wl_display_dispatch_queue_pending (display, queue);
        wl_display_flush (display);

        if (poll(&pfd, 1, -1)  < 0)
        {
            wl_display_cancel_read (display);
            break;
        }
        else
        {
            wl_display_read_events (display);
            wl_display_dispatch_queue_pending (display, queue);
        }
    }

    printf("@@@ %s(%d) tid(%ld) exit thread\n", __FUNCTION__, __LINE__, syscall(SYS_gettid));

    return NULL;
}

int main(int argc, char *argv[])
{
    struct wl_registry *registry;
    struct mirror_output *moutput, *otemp;
    struct mirror_buffer *mbuffer, *btemp;
    GMainLoop *main_loop;
    GThread *thread;

    wl_list_init(&output_list);
    wl_list_init(&buffer_list);

    display = wl_display_connect(NULL);
    exit_if_fail (display != NULL);

    registry = wl_display_get_registry(display);
    queue = wl_display_create_queue (display);
    wl_proxy_set_queue ((struct wl_proxy *)display, queue);
    wl_proxy_set_queue ((struct wl_proxy *)registry, queue);
    wl_registry_add_listener(registry, &registry_listener, NULL);

    wl_display_dispatch_queue(display, queue);
    wl_display_roundtrip_queue(display, queue);

    if (shooter == NULL)
    {
        fprintf(stderr, "display doesn't support tizen-screenshooter\n");
        return -1;
    }

    thread = g_thread_try_new ("mirror_thread", thread_main, NULL, NULL);

    /* main loop */
    main_loop = g_main_loop_new(NULL, FALSE);
    g_timeout_add (2000, on_sig_timeout, main_loop);

    printf("@@@ %s(%d) tid(%ld) loop start\n", __FUNCTION__, __LINE__, syscall(SYS_gettid));
    g_main_loop_run(main_loop);

    if (thread)
        g_thread_join (thread);

    printf("@@@ %s(%d) tid(%ld) loop end\n", __FUNCTION__, __LINE__, syscall(SYS_gettid));

    /* destroy resources */
    g_main_loop_unref (main_loop);
    wl_registry_destroy(registry);
    wl_list_for_each_safe(moutput, otemp, &output_list, link)
    {
        if (moutput->output)
            wl_output_destroy (moutput->output);
        wl_list_remove (&moutput->link);
        free(moutput);
    }
    wl_list_for_each_safe(mbuffer, btemp, &buffer_list, link)
    {
        if (mbuffer->buffer)
            wl_buffer_destroy (mbuffer->buffer);
        wl_list_remove (&mbuffer->link);
        free(mbuffer);
    }
    if (shm)
        wl_shm_destroy(shm);
    if (mirror)
        tizen_screenmirror_destroy(mirror);
    if (shooter)
        tizen_screenshooter_destroy(shooter);
    if (queue)
        wl_event_queue_destroy(queue);
    if (mirror)
    {
        wl_display_flush(display);
        wl_display_disconnect(display);
    }

    return 0;
}
