#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/input.h>
#include <libinput.h>
#include <libudev.h>
#include "ember.h"
#include "input.h"

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

static const struct libinput_interface interface_ops = {
    .open_restricted = open_restricted,
    .close_restricted = close_restricted,
};

static void handle_keyboard_key(struct ember_server *server, struct libinput_event_keyboard *k) {
    uint32_t key = libinput_event_keyboard_get_key(k);
    uint32_t state = libinput_event_keyboard_get_key_state(k) == LIBINPUT_KEY_STATE_PRESSED ? 
                     WL_KEYBOARD_KEY_STATE_PRESSED : WL_KEYBOARD_KEY_STATE_RELEASED;
    
    // ESC to quit compositor
    if (state == WL_KEYBOARD_KEY_STATE_PRESSED && key == KEY_ESC) {
        wl_display_terminate(server->wl_display);
        return;
    }
    
    // Auto-focus first surface if needed
    update_focus(server);
    
    // Dispatch to focused client
    dispatch_keyboard_key(server, key, state);
}

static void handle_pointer_motion(struct ember_server *server, struct libinput_event_pointer *p) {
    double dx = libinput_event_pointer_get_dx(p);
    double dy = libinput_event_pointer_get_dy(p);
    
    // Update cursor position (clamped to screen bounds)
    server->cursor.x += dx;
    server->cursor.y += dy;
    
    // Clamp to screen bounds
    if (server->cursor.x < 0) server->cursor.x = 0;
    if (server->cursor.y < 0) server->cursor.y = 0;
    if (server->cursor.x > server->mode.hdisplay) server->cursor.x = server->mode.hdisplay;
    if (server->cursor.y > server->mode.vdisplay) server->cursor.y = server->mode.vdisplay;
    
    // Dispatch to focused client
    dispatch_pointer_motion(server, server->cursor.x, server->cursor.y);
}

static void handle_pointer_button(struct ember_server *server, struct libinput_event_pointer *p) {
    uint32_t button = libinput_event_pointer_get_button(p);
    uint32_t state = libinput_event_pointer_get_button_state(p) == LIBINPUT_BUTTON_STATE_PRESSED ?
                     WL_POINTER_BUTTON_STATE_PRESSED : WL_POINTER_BUTTON_STATE_RELEASED;
    
    // Auto-focus on click
    update_focus(server);
    
    dispatch_pointer_button(server, button, state);
}

// Internal processing
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
        case LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE: {
            struct libinput_event_pointer *p = libinput_event_get_pointer_event(ev);
            server->cursor.x = libinput_event_pointer_get_absolute_x_transformed(p, server->mode.hdisplay);
            server->cursor.y = libinput_event_pointer_get_absolute_y_transformed(p, server->mode.vdisplay);
            dispatch_pointer_motion(server, server->cursor.x, server->cursor.y);
            break;
        }
        case LIBINPUT_EVENT_POINTER_BUTTON:
            handle_pointer_button(server, libinput_event_get_pointer_event(ev));
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
    
    if (libinput_udev_assign_seat(server->libinput, "seat0") != 0) {
        fprintf(stderr, "Failed to assign seat0\n");
        return -1;
    }
    
    // Add libinput fd to Wayland event loop
    wl_event_loop_add_fd(server->wl_event_loop, libinput_get_fd(server->libinput), WL_EVENT_READABLE, on_input_readable, server);
    
    // Initialize Cursor state
    init_cursor(server);

    printf("Initialized Input (libinput)\n");
    return 0;
}
