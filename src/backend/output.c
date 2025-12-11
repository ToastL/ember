#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-server.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>
#include <EGL/egl.h>
#include "ember.h"
#include "backend.h"
#include "renderer.h"

// --- wl_output implementation ---

static void output_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id) {
    struct ember_server *server = data;
    struct wl_resource *resource = wl_resource_create(client, &wl_output_interface, version, id);
    wl_list_insert(&server->output_resources, wl_resource_get_link(resource));

    // Send geometry
    wl_output_send_geometry(resource, 0, 0, 
                            server->connector->mmWidth, 
                            server->connector->mmHeight, 
                            WL_OUTPUT_SUBPIXEL_UNKNOWN, 
                            "Generic", "Monitor", 
                            WL_OUTPUT_TRANSFORM_NORMAL);

    // Send scale
    if (version >= 2) {
        wl_output_send_scale(resource, 1);
    }

    // Send mode (current | preferred)
    wl_output_send_mode(resource, 
                        WL_OUTPUT_MODE_CURRENT | WL_OUTPUT_MODE_PREFERRED,
                        server->mode.hdisplay, 
                        server->mode.vdisplay, 
                        server->mode.vrefresh * 1000);

    // Done
    if (version >= 2) {
        wl_output_send_done(resource);
    }
    
    printf("Client bound to wl_output\n");
}

int init_output(struct ember_server *server) {
    // 1. Get connector
    drmModeRes *res = drmModeGetResources(server->drm_fd);
    if (!res) {
        fprintf(stderr, "Failed to get DRM resources\n");
        return -1;
    }

    for (int i = 0; i < res->count_connectors; i++) {
        drmModeConnector *conn = drmModeGetConnector(server->drm_fd, res->connectors[i]);
        if (conn->connection == DRM_MODE_CONNECTED && conn->count_modes > 0) {
            server->connector = conn;
            server->mode = conn->modes[0]; // Pick first mode
            break;
        }
        drmModeFreeConnector(conn);
    }
    
    if (!server->connector) {
        fprintf(stderr, "No connected monitor found\n");
        return -1;
    }

    printf("Selected Mode: %dx%d @ %dHz\n", server->mode.hdisplay, server->mode.vdisplay, server->mode.vrefresh);

    // 2. Create GBM Surface (the backbuffer)
    server->gbm_surface = gbm_surface_create(server->gbm_device, 
                                             server->mode.hdisplay, 
                                             server->mode.vdisplay, 
                                             GBM_FORMAT_XRGB8888, 
                                             GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
    if (!server->gbm_surface) {
        fprintf(stderr, "Failed to create GBM surface\n");
        return -1;
    }

    // 3. Create EGL Surface
    server->egl_surface = eglCreateWindowSurface(server->egl_display, server->egl_config, (EGLNativeWindowType)server->gbm_surface, NULL);
    if (server->egl_surface == EGL_NO_SURFACE) {
        fprintf(stderr, "Failed to create EGL surface\n");
        return -1;
    }

    // 4. Make EGL Context Current (REQUIRED before any GL calls)
    if (!eglMakeCurrent(server->egl_display, server->egl_surface, server->egl_surface, server->egl_context)) {
        fprintf(stderr, "Failed to make EGL context current\n");
        return -1;
    }
    printf("EGL context made current\n");

    // 5. Initialize Renderer (Shaders)
    if (init_renderer(server) < 0) {
        return -1;
    }

    // 5. Find CRTC
    drmModeEncoder *enc = drmModeGetEncoder(server->drm_fd, server->connector->encoder_id);
    if (enc) {
        server->crtc = drmModeGetCrtc(server->drm_fd, enc->crtc_id);
        drmModeFreeEncoder(enc);
    }
    
    // Fallback if no encoder/crtc attached
    if (!server->crtc) {
        // Just pick the first one
        // Note: Real compositor needs better logic
         server->crtc = drmModeGetCrtc(server->drm_fd, res->crtcs[0]);
    }

    if (!server->crtc) {
        fprintf(stderr, "Failed to find any CRTC!\n");
        return -1;
    }

    printf("Initialized Output (CRTC ID: %d)\n", server->crtc->crtc_id);
    
    // 6. Setup Wayland Global
    wl_list_init(&server->output_resources);
    server->output_global = wl_global_create(server->wl_display, &wl_output_interface, 3, server, output_bind);
    if (!server->output_global) {
        fprintf(stderr, "Failed to create wl_output global\n");
        return -1;
    }
    
    printf("Initialized Wayland Globals (Output)\n");
    return 0;
}
