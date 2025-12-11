#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include "backend.h"

#ifndef GL_BGRA_EXT
#define GL_BGRA_EXT 0x80E1
#endif

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

static void output_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id) {
    struct ember_server *server = data;
    struct wl_resource *resource = wl_resource_create(client, &wl_output_interface, version, id);
    wl_list_insert(&server->output_resources, wl_resource_get_link(resource));
    
    // Send Geometry (0,0, physical size 0x0, subpixel unknown, make, model, transform default)
    wl_output_send_geometry(resource, 0, 0, 0, 0, 0, "ToastOS", "Ember Display", WL_OUTPUT_TRANSFORM_NORMAL);
    
    // Send Mode (flags: current | preferred)
    uint32_t flags = WL_OUTPUT_MODE_CURRENT | WL_OUTPUT_MODE_PREFERRED;
    wl_output_send_mode(resource, flags, server->mode.hdisplay, server->mode.vdisplay, server->mode.vrefresh * 1000);
    
    // Send Scale (1) and Done
    if (version >= 2) wl_output_send_scale(resource, 1);
    if (version >= 2) wl_output_send_done(resource);
}

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

    printf("Initialized Output (CRTC ID: %d)\n", server->crtc->crtc_id);

    // Initialize Global
    wl_list_init(&server->output_resources);
    
    server->output_global = wl_global_create(server->wl_display, &wl_output_interface, 3, server, output_bind);
    if (!server->output_global) {
        fprintf(stderr, "Failed to create wl_output global\n");
        return -1;
    }
    printf("Initialized Wayland Globals (Output)\n");

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
    "attribute vec2 texcoord;\n"
    "varying vec2 v_texcoord;\n"
    "void main() {\n"
    "    gl_Position = vec4(position, 0.0, 1.0);\n"
    "    v_texcoord = texcoord;\n"
    "}\n";

static const char *frag_shader_text =
    "precision mediump float;\n"
    "varying vec2 v_texcoord;\n"
    "uniform sampler2D tex;\n"
    "void main() {\n"
    "    gl_FragColor = texture2D(tex, v_texcoord);\n"
    "}\n";

static GLuint program;
static GLuint pos_loc;
static GLuint texcoord_loc;
static GLuint tex_loc;

static GLuint create_shader(struct ember_server *server, const char *source, GLenum type) {
    (void)server;
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    
    // Check compile status (optional for now, but good practice)
    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        fprintf(stderr, "Shader compilation failed: %s\n", infoLog);
    }
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
    texcoord_loc = glGetAttribLocation(program, "texcoord");
    tex_loc = glGetUniformLocation(program, "tex");
    
    glUseProgram(program);
    glUniform1i(tex_loc, 0); // Bind sampler to texture unit 0
}

void render_frame(struct ember_server *server) {
    static int initialized = 0;
    if (!initialized) {
        init_shaders(server);
        initialized = 1;
    }

    // 1. Draw Background (Dark Grey)
    glClearColor(0.1, 0.1, 0.1, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);

    // Enable Blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // 2. Render Surfaces
    struct ember_surface *surface;
    wl_list_for_each_reverse(surface, &server->surfaces, link) {
        if (!surface->buffer) {
            continue;
        }

        struct wl_shm_buffer *shm_buffer = wl_shm_buffer_get(surface->buffer);
        if (!shm_buffer) {
             continue;
        }

        int32_t width = wl_shm_buffer_get_width(shm_buffer);
        int32_t height = wl_shm_buffer_get_height(shm_buffer);
        void *data = wl_shm_buffer_get_data(shm_buffer);

        // Upload Texture if needed (Simple: Upload every frame for now for correctness)
        // TODO: Optimize using damage tracking
        if (!surface->texture_id) {
            glGenTextures(1, &surface->texture_id);
        }
        
        glBindTexture(GL_TEXTURE_2D, surface->texture_id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        
        // Wayland uses ARGB8888, OpenGL expects BGRA or we swizzle.
        // For simplicity, let's assume WL_SHM_FORMAT_ARGB8888 maps roughly to GL_BGRA_EXT
        // Note: Check extension support or just use GL_RGBA and hope colors are flipped (RB swapped).
        glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA_EXT, width, height, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, data);
        
        GLenum err = glGetError();
        if (err != GL_NO_ERROR) {
            printf("GL Error uploading texture: 0x%x\n", err);
        }

        // Draw Quad (Client coordinates to NDC)
        // Normalized Device Coordinates: -1.0 to 1.0
        // We need a projection matrix, but for now let's just draw Fullscreen or a fixed quad.
        // TODO: Implement proper coordinate projection.
        // Temporary: Draw covering 50% of screen center
        GLfloat vVertices[] = { 
             -0.5f,  0.5f, 0.0f,  // Top Left
             -0.5f, -0.5f, 0.0f,  // Bottom Left
              0.5f, -0.5f, 0.0f,  // Bottom Right
              0.5f,  0.5f, 0.0f   // Top Right
        };
        
        // Texture Coords (Flip Y because Wayland is Top-Left, GL is Bottom-Left)
        GLfloat vTexCoords[] = {
            0.0f, 0.0f,
            0.0f, 1.0f,
            1.0f, 1.0f,
            1.0f, 0.0f
        };
        
        // We need an element buffer or draw arrays (Triangle Fan for Quad)
        
        // Vertex Attrib (Position)
        glVertexAttribPointer(pos_loc, 3, GL_FLOAT, GL_FALSE, 0, vVertices);
        glEnableVertexAttribArray(pos_loc);

        // Texture Coords
        glVertexAttribPointer(texcoord_loc, 2, GL_FLOAT, GL_FALSE, 0, vTexCoords);
        glEnableVertexAttribArray(texcoord_loc);
        
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }
    
    // 3. Draw Cursor (simple white square)
    {        
        // Cursor size in pixels (larger for 4K)
        float cursor_size = 32.0f;
        float cx = server->cursor_x;
        float cy = server->cursor_y;
        
        // Convert to normalized device coordinates (-1 to 1)
        float screen_w = (float)server->mode.hdisplay;
        float screen_h = (float)server->mode.vdisplay;
        
        float x0 = (cx / screen_w) * 2.0f - 1.0f;
        float y0 = 1.0f - (cy / screen_h) * 2.0f;  // Flip Y
        float x1 = ((cx + cursor_size) / screen_w) * 2.0f - 1.0f;
        float y1 = 1.0f - ((cy + cursor_size) / screen_h) * 2.0f;
        
        GLfloat cursor_verts[] = {
            x0, y0, 0.0f,
            x0, y1, 0.0f,
            x1, y1, 0.0f,
            x1, y0, 0.0f,
        };
        
        GLfloat cursor_tex[] = {
            0.0f, 0.0f,
            0.0f, 1.0f,
            1.0f, 1.0f,
            1.0f, 0.0f
        };
        
        // Create a small white texture for cursor
        static GLuint cursor_tex_id = 0;
        if (!cursor_tex_id) {
            glGenTextures(1, &cursor_tex_id);
            glBindTexture(GL_TEXTURE_2D, cursor_tex_id);
            unsigned char white[] = {255, 255, 255, 255};
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        }
        
        glBindTexture(GL_TEXTURE_2D, cursor_tex_id);
        
        // Ensure attribs are enabled
        glEnableVertexAttribArray(pos_loc);
        glEnableVertexAttribArray(texcoord_loc);
        
        glVertexAttribPointer(pos_loc, 3, GL_FLOAT, GL_FALSE, 0, cursor_verts);
        glVertexAttribPointer(texcoord_loc, 2, GL_FLOAT, GL_FALSE, 0, cursor_tex);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }
    
    // 4. Swap Buffers (EGL -> GBM)
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
