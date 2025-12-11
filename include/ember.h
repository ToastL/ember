#ifndef EMBER_H
#define EMBER_H

#include <wayland-server.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <stdbool.h>

#include <libudev.h>
#include <libinput.h>

// Forward declaration
struct ember_server;

struct ember_surface {
    struct wl_resource *resource;
    struct wl_list link; // Link to server->surfaces
    
    // Double Buffering State
    struct gbm_bo *previous_bo;
    uint32_t previous_fb_id;
};

struct ember_server {
    struct wl_display *wl_display;
    struct wl_event_loop *wl_event_loop;
    
    // Core Subsystems
    struct udev *udev;
    struct libinput *libinput;
    
    // Wayland Globals
    struct wl_global *compositor;

    // DRM/GBM/EGL State
    int drm_fd;
    struct gbm_device *gbm_device;
    EGLDisplay egl_display;
    EGLContext egl_context;
    EGLConfig egl_config;

    // Output State (Monitor)
    drmModeConnector *connector;
    drmModeModeInfo mode;
    drmModeCrtc *crtc;
    struct gbm_surface *gbm_surface;
    EGLSurface egl_surface;
    
    // Double Buffering State
    struct gbm_bo *previous_bo;
    uint32_t previous_fb_id;

    // Client Resources (for broadcasting events)
    struct wl_list seat_resources;   // wl_seat clients
    struct wl_list output_resources; // wl_output clients
};

#endif
