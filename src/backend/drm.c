#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>
#include "backend.h"
#include "renderer.h"

// Helper: Open the first available DRM card
static int open_drm_device(void) {
    char path[64];
    for (int i = 0; i < 8; i++) {
        snprintf(path, sizeof(path), "/dev/dri/card%d", i);
        int fd = open(path, O_RDWR | O_CLOEXEC);
        if (fd >= 0) {
            printf("Opened DRM device: %s\n", path);
            return fd;
        }
    }
    return -1;
}

// Helper: Convert GBM Buffer to DRM Framebuffer
uint32_t get_fb_for_bo(int fd, struct gbm_bo *bo) {
    uint32_t bo_handles[4] = {0}, strides[4] = {0}, offsets[4] = {0};
    uint32_t width = gbm_bo_get_width(bo);
    uint32_t height = gbm_bo_get_height(bo);
    uint32_t format = gbm_bo_get_format(bo);
    uint32_t fb_id = 0;

    bo_handles[0] = gbm_bo_get_handle(bo).u32;
    strides[0] = gbm_bo_get_stride(bo);

    if (drmModeAddFB2(fd, width, height, format, bo_handles, strides, offsets, &fb_id, 0)) {
        fprintf(stderr, "drmModeAddFB2 failed: %m\n");
        return 0;
    }
    return fb_id;
}

// DRM Page Flip Handler (Called when VSync happens)
static void page_flip_handler(int fd, unsigned int frame,
                              unsigned int sec, unsigned int usec,
                              void *data) {
    (void)fd; (void)frame; (void)sec; (void)usec;
    struct ember_server *server = data;
    render_frame(server);
}

static drmEventContext drm_evctx = {
    .version = 2,
    .page_flip_handler = page_flip_handler,
};

void handle_drm_event(struct ember_server *server) {
    drmHandleEvent(server->drm_fd, &drm_evctx);
}

int init_drm(struct ember_server *server) {
    server->drm_fd = open_drm_device();
    if (server->drm_fd < 0) {
        fprintf(stderr, "Failed to find DRM device\n");
        return -1;
    }

    server->gbm_device = gbm_create_device(server->drm_fd);
    if (!server->gbm_device) {
        fprintf(stderr, "Failed to create GBM device\n");
        return -1;
    }
    
    // Initialize EGL (EGL context depends on GBM device)
    if (init_egl(server) < 0) {
        return -1;
    }

    return 0;
}
