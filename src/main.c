#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include "ember.h"
#include "backend.h"
#include "input.h"

// Callback when DRM FD is ready (Page Flip Complete)
static int on_drm_event(int fd, uint32_t mask, void *data) {
    struct ember_server *server = data;
    handle_drm_event(server);
    return 1;
}

int main(int argc, char *argv[]) {
    printf("Starting Ember Compositor...\n");
    struct ember_server server = {0};
    
    // Ensure we see output immediately
    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IOLBF, 0);

    server.wl_display = wl_display_create();
    server.wl_event_loop = wl_display_get_event_loop(server.wl_display);

    if (init_graphics(&server) < 0) return 1;
    if (init_output(&server) < 0) return 1;
    if (init_input(&server) < 0) return 1;

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
