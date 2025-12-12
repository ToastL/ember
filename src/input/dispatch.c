#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <time.h>
#include <wayland-server.h>
#include "ember.h"
#include "input.h"

// Get current time in milliseconds (for Wayland timestamps)
static uint32_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

// Find the wl_keyboard resource for a given client
static struct wl_resource *find_keyboard_for_client(struct ember_server *server, struct wl_client *client) {
    struct wl_resource *res;
    wl_resource_for_each(res, &server->keyboard_resources) {
        if (wl_resource_get_client(res) == client) {
            return res;
        }
    }
    return NULL;
}

// Find the wl_pointer resource for a given client
static struct wl_resource *find_pointer_for_client(struct ember_server *server, struct wl_client *client) {
    struct wl_resource *res;
    wl_resource_for_each(res, &server->pointer_resources) {
        if (wl_resource_get_client(res) == client) {
            return res;
        }
    }
    return NULL;
}

void dispatch_keyboard_key(struct ember_server *server, uint32_t key, uint32_t state) {
    if (!server->focused_surface) return;
    
    struct wl_client *client = wl_resource_get_client(server->focused_surface->resource);
    struct wl_resource *keyboard = find_keyboard_for_client(server, client);
    if (!keyboard) return;
    
    uint32_t serial = wl_display_next_serial(server->wl_display);
    uint32_t time = get_time_ms();
    
    wl_keyboard_send_key(keyboard, serial, time, key, state);
}

void dispatch_keyboard_modifiers(struct ember_server *server, uint32_t mods_depressed, 
                                  uint32_t mods_latched, uint32_t mods_locked, uint32_t group) {
    if (!server->focused_surface) return;
    
    struct wl_client *client = wl_resource_get_client(server->focused_surface->resource);
    struct wl_resource *keyboard = find_keyboard_for_client(server, client);
    if (!keyboard) return;
    
    uint32_t serial = wl_display_next_serial(server->wl_display);
    wl_keyboard_send_modifiers(keyboard, serial, mods_depressed, mods_latched, mods_locked, group);
}

void dispatch_pointer_motion(struct ember_server *server, double x, double y) {
    if (!server->focused_surface) return;
    
    struct wl_client *client = wl_resource_get_client(server->focused_surface->resource);
    struct wl_resource *pointer = find_pointer_for_client(server, client);
    if (!pointer) return;
    
    uint32_t time = get_time_ms();
    
    // Convert global coords to surface-local coords
    // For now, surface is at (100, 100) - TODO: Use actual surface position
    double sx = x - 100.0;
    double sy = y - 100.0;
    
    wl_pointer_send_motion(pointer, time, wl_fixed_from_double(sx), wl_fixed_from_double(sy));
}

void dispatch_pointer_button(struct ember_server *server, uint32_t button, uint32_t state) {
    if (!server->focused_surface) return;
    
    struct wl_client *client = wl_resource_get_client(server->focused_surface->resource);
    struct wl_resource *pointer = find_pointer_for_client(server, client);
    if (!pointer) return;
    
    uint32_t serial = wl_display_next_serial(server->wl_display);
    uint32_t time = get_time_ms();
    
    wl_pointer_send_button(pointer, serial, time, button, state);
}

void set_keyboard_focus(struct ember_server *server, struct ember_surface *surface) {
    struct wl_array keys;
    wl_array_init(&keys);
    
    // Send leave to old focused surface
    if (server->focused_surface && server->focused_surface != surface) {
        struct wl_client *old_client = wl_resource_get_client(server->focused_surface->resource);
        struct wl_resource *old_keyboard = find_keyboard_for_client(server, old_client);
        if (old_keyboard) {
            uint32_t serial = wl_display_next_serial(server->wl_display);
            wl_keyboard_send_leave(old_keyboard, serial, server->focused_surface->resource);
        }
    }
    
    server->focused_surface = surface;
    
    // Send enter to new focused surface
    if (surface) {
        struct wl_client *client = wl_resource_get_client(surface->resource);
        struct wl_resource *keyboard = find_keyboard_for_client(server, client);
        if (keyboard) {
            uint32_t serial = wl_display_next_serial(server->wl_display);
            wl_keyboard_send_enter(keyboard, serial, surface->resource, &keys);
        }
        
        // Also send pointer enter
        struct wl_resource *pointer = find_pointer_for_client(server, client);
        if (pointer) {
            uint32_t serial = wl_display_next_serial(server->wl_display);
            wl_pointer_send_enter(pointer, serial, surface->resource, 
                                  wl_fixed_from_double(server->cursor.x - 100.0),
                                  wl_fixed_from_double(server->cursor.y - 100.0));
        }
    }
    
    wl_array_release(&keys);
}

// Auto-focus the first surface (simple strategy)
void update_focus(struct ember_server *server) {
    if (wl_list_empty(&server->surfaces)) {
        server->focused_surface = NULL;
        return;
    }
    
    // Focus the first surface if nothing is focused
    if (!server->focused_surface) {
        struct ember_surface *first = wl_container_of(server->surfaces.next, first, link);
        set_keyboard_focus(server, first);
    }
}
