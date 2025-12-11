#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include "backend.h"
#include "renderer.h"
#include "input.h"
#include "wayland/protocols.h"

// Callback when DRM FD is ready (Page Flip Complete)
static int on_drm_event(int fd, uint32_t mask, void *data) {
    (void)fd; (void)mask;
    struct ember_server *server = data;
    handle_drm_event(server);
    return 1;
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;
    printf("Starting Ember Compositor...\n");
    struct ember_server server = {0};
    
    // Ensure we see output immediately
    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IOLBF, 0);
    
    // Set XDG_RUNTIME_DIR if not set (needed for openvt)
    if (!getenv("XDG_RUNTIME_DIR")) {
        setenv("XDG_RUNTIME_DIR", "/tmp", 1);
        printf("Set XDG_RUNTIME_DIR=/tmp\n");
    }

    server.wl_display = wl_display_create();
    server.wl_event_loop = wl_display_get_event_loop(server.wl_display);
    wl_list_init(&server.surfaces);

    // 1. Initialize Backend (DRM -> GBM -> EGL)
    if (init_drm(&server) < 0) return 1;
    
    // 2. Initialize Output (Modesetting + Renderer + wl_output)
    if (init_output(&server) < 0) return 1;
    
    // 3. Initialize Input (libinput + cursor)
    if (init_input(&server) < 0) return 1;
    
    // 4. Initialize Wayland Globals (Compositor, Shell, Seat, etc.)
    if (init_wayland_globals(&server) < 0) return 1;
    
    // Note: init_seat is separate? pure protocol init
    // Let's check init_wayland_globals implementation in wayland/compositor.c
    // It calls init_shm, init_shell, init_ddm.
    // It does NOT call init_seat. We must call it manually or add it to init_wayland_globals.
    // I will call it manually here to be safe and explicit.
    if (init_seat(&server) < 0) return 1;

    // Render one frame immediately to turn the screen on (Modeset)
    render_frame(&server);
    // Render a second frame to start the PageFlip loop (Async Event)
    render_frame(&server);

    // Hook DRM events into the Wayland Event Loop
    wl_event_loop_add_fd(server.wl_event_loop, server.drm_fd, WL_EVENT_READABLE, on_drm_event, &server);

    const char *socket = wl_display_add_socket_auto(server.wl_display);
    if (!socket) {
        fprintf(stderr, "Failed to create Wayland socket\n");
        return 1;
    }
    printf("Running on WAYLAND_DISPLAY=%s\n", socket);
    fflush(stdout);

    wl_display_run(server.wl_display);
    
    wl_display_destroy(server.wl_display);
    return 0;
}
