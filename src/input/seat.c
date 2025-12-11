#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/input.h>
#include "ember.h"

// Open/Close restricted devices (required by libinput)
static int open_restricted(const char *path, int flags, void *user_data) {
    (void)user_data;
    int fd = open(path, flags);
    return fd < 0 ? -errno : fd;
}

static void close_restricted(int fd, void *user_data) {
    (void)user_data;
    close(fd);
}

// --- libinput implementation ---

static const struct libinput_interface interface_ops = {
    .open_restricted = open_restricted,
    .close_restricted = close_restricted,
};

static void handle_keyboard_key(struct ember_server *server, struct libinput_event_keyboard *k) {
    uint32_t key = libinput_event_keyboard_get_key(k);
    enum libinput_event_type type = libinput_event_get_type((struct libinput_event *)k); // Helper logic

    // Just print for now
    if (libinput_event_keyboard_get_key_state(k) == LIBINPUT_KEY_STATE_PRESSED) {
        printf("Key Pressed: %d\n", key);
        if (key == KEY_ESC) {
            printf("ESC pressed. Exiting...\n");
            wl_display_terminate(server->wl_display);
        }
    }
}

static void handle_pointer_motion(struct ember_server *server, struct libinput_event_pointer *p) {
    (void)server;
    double dx = libinput_event_pointer_get_dx(p);
    double dy = libinput_event_pointer_get_dy(p);
    printf("Pointer Motion: %.2f, %.2f\n", dx, dy); 
}

static void process_events(struct ember_server *server) {
    struct libinput_event *ev;
    while ((ev = libinput_get_event(server->libinput))) {
        enum libinput_event_type type = libinput_event_get_type(ev);
        
        switch (type) {
        case LIBINPUT_EVENT_KEYBOARD_KEY:
            handle_keyboard_key(server, libinput_event_get_keyboard_event(ev));
            break;
        case LIBINPUT_EVENT_POINTER_MOTION:
            handle_pointer_motion(server, libinput_event_get_pointer_event(ev));
            break;
        default:
            break;
        }
        
        libinput_event_destroy(ev);
    }
}

// Called by Wayland Event Loop when libinput has data
int on_input_readable(int fd, uint32_t mask, void *data) {
    (void)fd; (void)mask;
    struct ember_server *server = data;
    if (libinput_dispatch(server->libinput) != 0) {
        fprintf(stderr, "libinput dispatch failed\n");
        return 0;
    }
    process_events(server);
    return 1;
}

int init_input(struct ember_server *server) {
    server->udev = udev_new();
    if (!server->udev) {
        fprintf(stderr, "Failed to initialize udev\n");
        return -1;
    }

    server->libinput = libinput_udev_create_context(&interface_ops, NULL, server->udev);
    if (!server->libinput) {
        fprintf(stderr, "Failed to create libinput context\n");
        return -1;
    }

    // Assign "seat0" (the main physical seat)
    if (libinput_udev_assign_seat(server->libinput, "seat0") != 0) {
        fprintf(stderr, "Failed to assign seat0\n");
        return -1;
    }

    // Hook into Wayland Loop
    int fd = libinput_get_fd(server->libinput);
    wl_event_loop_add_fd(server->wl_event_loop, fd, WL_EVENT_READABLE, on_input_readable, server);

    printf("Initialized Input (libinput)\n");
    return 0;
}
