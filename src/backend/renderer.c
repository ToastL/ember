#include <stdio.h>
#include <stdlib.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <gbm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <wayland-server.h>
#include "ember.h"
#include "renderer.h"
#include "backend.h"
#include "input.h"

// Simple Shaders
static const char *vert_shader_text =
    "attribute vec3 position;\n"
    "attribute vec2 texcoord;\n"
    "varying vec2 v_texcoord;\n"
    "void main() {\n"
    "    gl_Position = vec4(position, 1.0);\n"
    "    v_texcoord = texcoord;\n"
    "}\n";

static const char *frag_shader_text =
    "precision mediump float;\n"
    "varying vec2 v_texcoord;\n"
    "uniform sampler2D tex;\n"
    "void main() {\n"
    "    gl_FragColor = texture2D(tex, v_texcoord);\n"
    "}\n";

static GLuint create_shader(struct ember_server *server, const char *source, GLenum type) {
    (void)server;
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status) {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        fprintf(stderr, "Shader compilation failed: %s\n", log);
        return 0;
    }
    return shader;
}

int init_renderer(struct ember_server *server) {
    GLuint vert = create_shader(server, vert_shader_text, GL_VERTEX_SHADER);
    GLuint frag = create_shader(server, frag_shader_text, GL_FRAGMENT_SHADER);
    
    server->shader_program = glCreateProgram();
    glAttachShader(server->shader_program, vert);
    glAttachShader(server->shader_program, frag);
    
    // Explicitly bind attributes to safe locations BEFORE linking
    glBindAttribLocation(server->shader_program, 0, "position");
    glBindAttribLocation(server->shader_program, 1, "texcoord");
    
    glLinkProgram(server->shader_program);
    
    GLint status;
    glGetProgramiv(server->shader_program, GL_LINK_STATUS, &status);
    if (!status) {
        char log[1024];
        glGetProgramInfoLog(server->shader_program, sizeof(log), NULL, log);
        fprintf(stderr, "Program link failed: %s\n", log);
        return -1;
    }
    
    glUseProgram(server->shader_program);
    
    server->loc_pos = glGetAttribLocation(server->shader_program, "position");
    server->loc_texcoord = glGetAttribLocation(server->shader_program, "texcoord");
    server->loc_tex = glGetUniformLocation(server->shader_program, "tex");

    printf("Renderer initialized: loc_pos=%d, loc_texcoord=%d\n", server->loc_pos, server->loc_texcoord);
    
    return 0;
}

void render_frame(struct ember_server *server) {
    // 1. Make Context Current
    eglMakeCurrent(server->egl_display, server->egl_surface, server->egl_surface, server->egl_context);
    
    // Clear Background (Deep Blue)
    glClearColor(0.2f, 0.2f, 0.4f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Explicitly set viewport
    glViewport(0, 0, server->mode.hdisplay, server->mode.vdisplay);
    
    glUseProgram(server->shader_program);
    glUniform1i(server->loc_tex, 0);

    // Alpha Blending
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

        if (!surface->texture_id) {
            glGenTextures(1, &surface->texture_id);
        }
        
        glBindTexture(GL_TEXTURE_2D, surface->texture_id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        
        uint32_t format = wl_shm_buffer_get_format(shm_buffer);
        GLenum gl_format = GL_BGRA_EXT;
        if (format == WL_SHM_FORMAT_XRGB8888 || format == WL_SHM_FORMAT_ARGB8888) {
             gl_format = GL_BGRA_EXT;
        }
        
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, gl_format, GL_UNSIGNED_BYTE, data);
        
        float screen_w = (float)server->mode.hdisplay;
        float screen_h = (float)server->mode.vdisplay;
        
        float x = 100.0f; 
        float y = 100.0f;
        
        float x0 = (x / screen_w) * 2.0f - 1.0f;
        float y0 = 1.0f - (y / screen_h) * 2.0f;
        float x1 = ((x + width) / screen_w) * 2.0f - 1.0f;
        float y1 = 1.0f - ((y + height) / screen_h) * 2.0f;

        GLfloat vVertices[] = {
             x0, y0, 0.0f,
             x0, y1, 0.0f,
             x1, y1, 0.0f,
             x1, y0, 0.0f 
        };
        
        GLfloat vTexCoords[] = {
            0.0f, 0.0f,
            0.0f, 1.0f,
            1.0f, 1.0f,
            1.0f, 0.0f
        };
        
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, vVertices);
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, vTexCoords);
        glEnableVertexAttribArray(1);
        
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }
    
    // 3. Render Cursor
    render_cursor(server);

    // 4. Swap Buffers (EGL -> GBM)
    eglSwapBuffers(server->egl_display, server->egl_surface);

    // 5. Get the underlying buffer object (GBM BO)
    struct gbm_bo *bo = gbm_surface_lock_front_buffer(server->gbm_surface);
    if (!bo) {
        fprintf(stderr, "Failed to lock front buffer\n");
        return;
    }
    uint32_t fb_id = get_fb_for_bo(server->drm_fd, bo);

    // 6. Set CRTC (Modeset / Pageflip)
    static int first_frame = 1;
    if (first_frame) {
        printf("Performing first mode set (CRTC: %p, Conn: %p)\n", server->crtc, server->connector);
        if (!server->crtc || !server->connector) {
            fprintf(stderr, "CRTC or Connector missing!\n");
            return;
        }

        int ret = drmModeSetCrtc(server->drm_fd, server->crtc->crtc_id, fb_id, 0, 0, &server->connector->connector_id, 1, &server->mode);
        if (ret < 0) {
             fprintf(stderr, "drmModeSetCrtc failed: %m\n");
             return;
        }
        first_frame = 0;
    } else {
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
