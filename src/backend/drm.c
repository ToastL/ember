#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <GLES2/gl2.h>
#include "backend.h"

// Simple helper to open the first available DRM card
static int open_drm_device(void) {
    drmDevicePtr devices[64];
    int num_devices = drmGetDevices2(0, devices, 64);
    if (num_devices < 0) return -1;

    int fd = -1;
    for (int i = 0; i < num_devices; i++) {
        drmDevicePtr dev = devices[i];
        if (!(dev->available_nodes & (1 << DRM_NODE_PRIMARY))) continue;
        
        fd = open(dev->nodes[DRM_NODE_PRIMARY], O_RDWR | O_CLOEXEC);
        if (fd >= 0) {
            drmModeRes *res = drmModeGetResources(fd);
            if (res) {
                drmModeFreeResources(res);
                break;
            }
            close(fd);
            fd = -1;
        }
    }
    drmFreeDevices(devices, num_devices);
    return fd;
}

int init_graphics(struct ember_server *server) {
    // 1. Open DRM Device
    server->drm_fd = open_drm_device();
    if (server->drm_fd < 0) {
        fprintf(stderr, "Failed to find DRM device\n");
        return -1;
    }

    // 2. Initialize GBM
    server->gbm_device = gbm_create_device(server->drm_fd);
    if (!server->gbm_device) return -1;

    // 3. Initialize EGL
    server->egl_display = eglGetPlatformDisplay(EGL_PLATFORM_GBM_MESA, server->gbm_device, NULL);
    if (server->egl_display == EGL_NO_DISPLAY) {
        server->egl_display = eglGetDisplay((EGLNativeDisplayType)server->gbm_device);
    }
    eglInitialize(server->egl_display, NULL, NULL);

    EGLint attributes[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };
    EGLint num_config;
    eglChooseConfig(server->egl_display, attributes, &server->egl_config, 1, &num_config);
    eglBindAPI(EGL_OPENGL_ES_API);
    
    EGLint context_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    server->egl_context = eglCreateContext(server->egl_display, server->egl_config, EGL_NO_CONTEXT, context_attribs);

    if (server->egl_context == EGL_NO_CONTEXT) {
        fprintf(stderr, "Failed to create EGL context\n");
        return -1;
    }

    return 0;
}

// --- wl_output implementation ---


int init_output(struct ember_server *server) {
    drmModeRes *resources = drmModeGetResources(server->drm_fd);
    if (!resources) return -1;

    // Find a connected connector
    for (int i = 0; i < resources->count_connectors; i++) {
        drmModeConnector *conn = drmModeGetConnector(server->drm_fd, resources->connectors[i]);
        if (conn->connection == DRM_MODE_CONNECTED && conn->count_modes > 0) {
            server->connector = conn;
            break;
        }
        drmModeFreeConnector(conn);
    }
    drmModeFreeResources(resources);

    if (!server->connector) {
        fprintf(stderr, "No connected monitor found\n");
        return -1;
    }

    // Pick the first mode (usually preferred/native)
    server->mode = server->connector->modes[0];
    printf("Selected Mode: %dx%d @ %dHz\n", server->mode.hdisplay, server->mode.vdisplay, server->mode.vrefresh);

    // Create GBM Surface (The Window)
    server->gbm_surface = gbm_surface_create(
        server->gbm_device, server->mode.hdisplay, server->mode.vdisplay,
        GBM_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING
    );
    if (!server->gbm_surface) {
        fprintf(stderr, "Failed to create GBM surface\n");
        return -1;
    }

    server->egl_surface = eglCreateWindowSurface(server->egl_display, server->egl_config, (EGLNativeWindowType)server->gbm_surface, NULL);
    eglMakeCurrent(server->egl_display, server->egl_surface, server->egl_surface, server->egl_context);

    // Find a CRTC
    if (server->connector->encoder_id) {
        drmModeEncoder *enc = drmModeGetEncoder(server->drm_fd, server->connector->encoder_id);
        if (enc) {
            if (enc->crtc_id) {
                server->crtc = drmModeGetCrtc(server->drm_fd, enc->crtc_id);
            }
            drmModeFreeEncoder(enc);
        }
    }
    
    // Fallback: Find first available CRTC if not found above
    if (!server->crtc) {
        for (int i = 0; i < resources->count_crtcs; i++) {
             server->crtc = drmModeGetCrtc(server->drm_fd, resources->crtcs[i]);
             if (server->crtc) break;
        }
    }

    if (!server->crtc) {
        fprintf(stderr, "Failed to find any CRTC!\n");
        return -1;
    }
    printf("Initialized Output (CRTC ID: %d)\n", server->crtc->crtc_id);

    // Initialize Global
    wl_list_init(&server->output_resources);
    // wl_global_create(server->wl_display, &wl_output_interface, 3, server, output_bind);

    return 0;
}

// Helper: Convert GBM Buffer to DRM Framebuffer
static uint32_t get_fb_for_bo(int fd, struct gbm_bo *bo) {
    uint32_t fb_id;
    uint32_t handle = gbm_bo_get_handle(bo).u32;
    uint32_t stride = gbm_bo_get_stride(bo);
    uint32_t width = gbm_bo_get_width(bo);
    uint32_t height = gbm_bo_get_height(bo);
    
    drmModeAddFB(fd, width, height, 24, 32, stride, handle, &fb_id);
    return fb_id;
}

static void page_flip_handler(int fd, unsigned int frame,
                              unsigned int sec, unsigned int usec,
                              void *data) {
    (void)fd; (void)frame; (void)sec; (void)usec;
    struct ember_server *server = data;
    render_frame(server);
}

// ... (previous includes)
// #include "protocol.h" // Removed

// Simple Shaders
static const char *vert_shader_text =
    "attribute vec2 position;\n"
    "void main() {\n"
    "    gl_Position = vec4(position, 0.0, 1.0);\n"
    "}\n";

static const char *frag_shader_text =
    "precision mediump float;\n"
    "void main() {\n"
    "    gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);\n"
    "}\n";

static GLuint program;
static GLuint pos_loc;

static GLuint create_shader(struct ember_server *server, const char *source, GLenum type) {
    (void)server;
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    return shader;
}

static void init_shaders(struct ember_server *server) {
    GLuint vert = create_shader(server, vert_shader_text, GL_VERTEX_SHADER);
    GLuint frag = create_shader(server, frag_shader_text, GL_FRAGMENT_SHADER);
    
    program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);
    
    pos_loc = glGetAttribLocation(program, "position");
}

void render_frame(struct ember_server *server) {
    static int initialized = 0;
    if (!initialized) {
        init_shaders(server);
        initialized = 1;
    }

    // 1. Draw Background (Pulsing Blue)
    static float color = 0.0f;
    static int direction = 1;
    color += 0.01f * direction;
    if (color >= 1.0f) direction = -1;
    if (color <= 0.0f) direction = 1;
    glClearColor(0.0, 0.0, color, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    
    // 2. Swap Buffers (EGL -> GBM)
    eglSwapBuffers(server->egl_display, server->egl_surface);

    // 3. Get the underlying buffer object (GBM BO)
    struct gbm_bo *bo = gbm_surface_lock_front_buffer(server->gbm_surface);
    if (!bo) {
        fprintf(stderr, "Failed to lock front buffer\n");
        return;
    }
    uint32_t fb_id = get_fb_for_bo(server->drm_fd, bo);

    // 4. Set CRTC (Modeset / Pageflip)
    static int first_frame = 1;
    if (first_frame) {
        printf("Performing first mode set (CRTC: %p, Conn: %p)\n", server->crtc, server->connector);
        if (!server->crtc || !server->connector) {
            fprintf(stderr, "CRTC or Connector missing!\n");
            return;
        }

        // First frame requires a full Modeset (turning on the screen)
        int ret = drmModeSetCrtc(server->drm_fd, server->crtc->crtc_id, fb_id, 0, 0, &server->connector->connector_id, 1, &server->mode);
        if (ret < 0) {
             fprintf(stderr, "drmModeSetCrtc failed: %m\n");
             return;
        }
        first_frame = 0;
    } else {
        // Subsequent frames use PageFlip (vsync)
        int ret = drmModePageFlip(server->drm_fd, server->crtc->crtc_id, fb_id, DRM_MODE_PAGE_FLIP_EVENT, server);
        if (ret < 0) {
            fprintf(stderr, "drmModePageFlip failed: %m\n");
            return;
        }
    }

    // Cleanup previous buffer
    if (server->previous_bo) {
        drmModeRmFB(server->drm_fd, server->previous_fb_id);
        gbm_surface_release_buffer(server->gbm_surface, server->previous_bo);
    }
    server->previous_bo = bo;
    server->previous_fb_id = fb_id;
}

// Global DRM Event Context
// This tells libdrm which function to call when a page flip completes
static drmEventContext drm_evctx = {
    .version = 2,
    .page_flip_handler = page_flip_handler,
};

// --- wl_output implementation ---


void handle_drm_event(struct ember_server *server) {
    drmHandleEvent(server->drm_fd, &drm_evctx);
}
