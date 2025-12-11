#include <stdio.h>
#include "ember.h"

int init_shm(struct ember_server *server) {
    // Use the built-in Wayland SHM implementation
    // This properly handles wl_shm_buffer_get() for buffer access
    if (wl_display_init_shm(server->wl_display) < 0) {
        fprintf(stderr, "Failed to initialize wl_shm\n");
        return -1;
    }
    printf("Initialized Wayland Globals (SHM)\n");
    return 0;
}
