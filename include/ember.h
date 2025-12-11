#ifndef EMBER_H
#define EMBER_H

#include <wayland-server.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <libinput.h>
#include <libudev.h>

// Forward declarations
struct ember_server;

struct ember_cursor {
    double x, y;
    float size;
    GLuint texture_id;
    int visible;
};

struct ember_surface {
    struct wl_resource *resource;
    struct wl_list link; // Link to server->surfaces
    
    // Rendering State
    struct wl_resource *buffer; // The attached wl_buffer
    int32_t pos_x, pos_y;       // Window position
    
    // GL State
    GLuint texture_id;
    EGLImageKHR egl_image;
    
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
    struct wl_list surfaces; // List of all surfaces
    struct wl_global *shm_global;
    struct wl_global *compositor_global; 
    struct wl_global *output_global;
    struct wl_global *seat_global;
    struct wl_global *xdg_shell_global;
    struct wl_global *ddm_global;

    // DRM/GBM/EGL State
    int drm_fd;
    struct gbm_device *gbm_device;
    EGLDisplay egl_display;
    EGLContext egl_context;
    EGLConfig egl_config;
    EGLSurface egl_surface;

    // Output State (Monitor)
    drmModeConnector *connector;
    drmModeModeInfo mode;
    drmModeCrtc *crtc;
    struct gbm_surface *gbm_surface;
    
    // Rendering State
    struct gbm_bo *previous_bo;
    uint32_t previous_fb_id;
    GLuint shader_program; // Moved here from static in drm.c
    GLint loc_pos;
    GLint loc_texcoord;
    GLint loc_tex;

    // Input State
    struct ember_cursor cursor;

    // Client Resources (for broadcasting events)
    struct wl_list seat_resources;
    struct wl_list output_resources;
};

#endif
