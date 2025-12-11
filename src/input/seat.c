#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/input.h>
#include <string.h>
#include <sys/mman.h>
#include <xkbcommon/xkbcommon.h>
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

// Helper to create an anonymous file for Keymap transmission
static int os_create_anonymous_file(off_t size) {
    int fd = memfd_create("ember-keymap", MFD_CLOEXEC | MFD_ALLOW_SEALING);
    if (fd < 0) {
        fprintf(stderr, "memfd_create failed: %s\n", strerror(errno));
        return -1;
    }

    if (ftruncate(fd, size) < 0) {
        fprintf(stderr, "ftruncate failed: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    return fd;
}

// --- wl_pointer implementation ---

static void pointer_set_cursor(struct wl_client *client, struct wl_resource *resource, uint32_t serial, struct wl_resource *surface, int32_t hotspot_x, int32_t hotspot_y) {
    (void)client; (void)resource; (void)serial; (void)surface; (void)hotspot_x; (void)hotspot_y;
}

static void pointer_release(struct wl_client *client, struct wl_resource *resource) {
    (void)client;
    wl_resource_destroy(resource);
}

static const struct wl_pointer_interface pointer_interface = {
    .set_cursor = pointer_set_cursor,
    .release = pointer_release,
};

// --- wl_keyboard implementation ---

static void keyboard_release(struct wl_client *client, struct wl_resource *resource) {
    (void)client;
    wl_resource_destroy(resource);
}

static const struct wl_keyboard_interface keyboard_interface = {
    .release = keyboard_release,
};

// --- wl_seat implementation ---

static void seat_get_pointer(struct wl_client *client, struct wl_resource *resource, uint32_t id) {
    struct wl_resource *pointer_resource = wl_resource_create(client, &wl_pointer_interface, wl_resource_get_version(resource), id);
    wl_resource_set_implementation(pointer_resource, &pointer_interface, NULL, NULL);

}

static void seat_get_keyboard(struct wl_client *client, struct wl_resource *resource, uint32_t id) {
    struct wl_resource *keyboard_resource = wl_resource_create(client, &wl_keyboard_interface, wl_resource_get_version(resource), id);
    wl_resource_set_implementation(keyboard_resource, &keyboard_interface, NULL, NULL);

    // Mandatory: Send repeat info (rate/delay)
    if (wl_resource_get_version(keyboard_resource) >= 4) {
        wl_keyboard_send_repeat_info(keyboard_resource, 25, 600);
    }

    // Create and Send Keymap (REQUIRED for GTK clients)
    
    struct xkb_context *ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    struct xkb_keymap *keymap = xkb_keymap_new_from_names(ctx, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);
    char *keymap_string = xkb_keymap_get_as_string(keymap, XKB_KEYMAP_FORMAT_TEXT_V1);
    size_t size = strlen(keymap_string) + 1;
    
    int fd = os_create_anonymous_file(size);
    if (fd >= 0) {
        void *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (ptr != MAP_FAILED) {
            strcpy(ptr, keymap_string);
            munmap(ptr, size);
            wl_keyboard_send_keymap(keyboard_resource, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, fd, size);
        }
        close(fd);
    } else {
        fprintf(stderr, "Failed to create keymap file\n");
    }
    
    free(keymap_string);
    xkb_keymap_unref(keymap);
    xkb_context_unref(ctx);
    
}

static void seat_get_touch(struct wl_client *client, struct wl_resource *resource, uint32_t id) {
    (void)client; (void)resource; (void)id;
}

static void seat_release(struct wl_client *client, struct wl_resource *resource) {
    (void)client;
    wl_resource_destroy(resource);
}

static const struct wl_seat_interface seat_interface = {
    .get_pointer = seat_get_pointer,
    .get_keyboard = seat_get_keyboard,
    .get_touch = seat_get_touch,
    .release = seat_release,
};

static void seat_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id) {
    struct ember_server *server = data;
    (void)version;
    struct wl_resource *resource = wl_resource_create(client, &wl_seat_interface, version, id);
    wl_list_insert(&server->seat_resources, wl_resource_get_link(resource));
    wl_resource_set_implementation(resource, &seat_interface, server, NULL);

    // Advertise capabilities (Pointer + Keyboard)
    uint32_t caps = WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_KEYBOARD;
    wl_seat_send_capabilities(resource, caps);

    if (wl_resource_get_version(resource) >= 2) {
        wl_seat_send_name(resource, "seat0");
    }
}

// --- libinput implementation ---

static const struct libinput_interface interface_ops = {
    .open_restricted = open_restricted,
    .close_restricted = close_restricted,
};

static void handle_keyboard_key(struct ember_server *server, struct libinput_event_keyboard *k) {
    uint32_t key = libinput_event_keyboard_get_key(k);
    if (libinput_event_keyboard_get_key_state(k) == LIBINPUT_KEY_STATE_PRESSED) {
        if (key == KEY_ESC) {
            wl_display_terminate(server->wl_display);
        }
    }
}

static void handle_pointer_motion(struct ember_server *server, struct libinput_event_pointer *p) {
    double dx = libinput_event_pointer_get_dx(p);
    double dy = libinput_event_pointer_get_dy(p);
    
    // Update cursor position (clamped to screen bounds)
    server->cursor_x += dx;
    server->cursor_y += dy;
    
    // Clamp to screen bounds
    if (server->cursor_x < 0) server->cursor_x = 0;
    if (server->cursor_y < 0) server->cursor_y = 0;
    if (server->cursor_x > server->mode.hdisplay) server->cursor_x = server->mode.hdisplay;
    if (server->cursor_y > server->mode.vdisplay) server->cursor_y = server->mode.vdisplay;
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
        case LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE: {
            // For touchpads and absolute positioning devices
            struct libinput_event_pointer *p = libinput_event_get_pointer_event(ev);
            server->cursor_x = libinput_event_pointer_get_absolute_x_transformed(p, server->mode.hdisplay);
            server->cursor_y = libinput_event_pointer_get_absolute_y_transformed(p, server->mode.vdisplay);
            break;
        }
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

    // Initialize wl_seat Global
    wl_list_init(&server->seat_resources);

    server->seat_global = wl_global_create(server->wl_display, &wl_seat_interface, 5, server, seat_bind);
    if (!server->seat_global) {
        fprintf(stderr, "Failed to create wl_seat global\n");
        return -1;
    }
    printf("Initialized Wayland Globals (Seat)\n");

    return 0;
}
