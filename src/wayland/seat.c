#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <string.h>
#include <xkbcommon/xkbcommon.h>
#include <wayland-server.h>
#include "ember.h"
#include "backend.h"

// Helper to create an anonymous file for Keymap transmission
static int os_create_anonymous_file(off_t size) {
    int fd = memfd_create("ember-keymap", MFD_CLOEXEC);
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
    // TODO: Implement custom cursor logic
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

int init_seat(struct ember_server *server) {
    wl_list_init(&server->seat_resources);
    server->seat_global = wl_global_create(server->wl_display, &wl_seat_interface, 5, server, seat_bind);
    if (!server->seat_global) {
        fprintf(stderr, "Failed to create wl_seat global\n");
        return -1;
    }
    printf("Initialized Wayland Globals (Seat)\n");
    return 0;
}
