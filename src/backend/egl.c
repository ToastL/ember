#include <stdio.h>
#include <stdlib.h>
#include <gbm.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "backend.h"

int init_egl(struct ember_server *server) {
    server->egl_display = eglGetPlatformDisplay(EGL_PLATFORM_GBM_MESA, server->gbm_device, NULL);
    if (server->egl_display == EGL_NO_DISPLAY) {
        fprintf(stderr, "Failed to get EGL display\n");
        return -1;
    }

    if (!eglInitialize(server->egl_display, NULL, NULL)) {
        fprintf(stderr, "Failed to initialize EGL\n");
        return -1;
    }

    // Choose EGL Config
    EGLint attributes[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };
    EGLint count;
    if (!eglChooseConfig(server->egl_display, attributes, &server->egl_config, 1, &count) || count == 0) {
        fprintf(stderr, "Failed to choose EGL config\n");
        return -1;
    }

    // Create EGL Context
    static const EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
    eglBindAPI(EGL_OPENGL_ES_API);
    server->egl_context = eglCreateContext(server->egl_display, server->egl_config, EGL_NO_CONTEXT, context_attribs);
    if (server->egl_context == EGL_NO_CONTEXT) {
        fprintf(stderr, "Failed to create EGL context\n");
        return -1;
    }

    return 0;
}
