#include <stdio.h>
#include <stdlib.h>
#include "ember.h"
#include "xdg-shell-protocol.h"

// --- xdg_toplevel implementation ---

static void toplevel_destroy(struct wl_client *client, struct wl_resource *resource) {
    (void)client;
    wl_resource_destroy(resource);
}

static void toplevel_set_parent(struct wl_client *client, struct wl_resource *resource, struct wl_resource *parent) {
    (void)client; (void)resource; (void)parent;
}

static void toplevel_set_title(struct wl_client *client, struct wl_resource *resource, const char *title) {
    (void)client; (void)resource; (void)title;
}

static void toplevel_set_app_id(struct wl_client *client, struct wl_resource *resource, const char *app_id) {
    (void)client; (void)resource; (void)app_id;
}

static void toplevel_show_window_menu(struct wl_client *client, struct wl_resource *resource, struct wl_resource *seat, uint32_t serial, int32_t x, int32_t y) {
    (void)client; (void)resource; (void)seat; (void)serial; (void)x; (void)y;
}

static void toplevel_move(struct wl_client *client, struct wl_resource *resource, struct wl_resource *seat, uint32_t serial) {
    (void)client; (void)resource; (void)seat; (void)serial;
}

static void toplevel_resize(struct wl_client *client, struct wl_resource *resource, struct wl_resource *seat, uint32_t serial, uint32_t edges) {
    (void)client; (void)resource; (void)seat; (void)serial; (void)edges;
}

static void toplevel_set_max_size(struct wl_client *client, struct wl_resource *resource, int32_t width, int32_t height) {
    (void)client; (void)resource; (void)width; (void)height;
}

static void toplevel_set_min_size(struct wl_client *client, struct wl_resource *resource, int32_t width, int32_t height) {
    (void)client; (void)resource; (void)width; (void)height;
}

static void toplevel_set_maximized(struct wl_client *client, struct wl_resource *resource) {
    (void)client; (void)resource;
}

static void toplevel_unset_maximized(struct wl_client *client, struct wl_resource *resource) {
    (void)client; (void)resource;
}

static void toplevel_set_fullscreen(struct wl_client *client, struct wl_resource *resource, struct wl_resource *output) {
    (void)client; (void)resource; (void)output;
}

static void toplevel_unset_fullscreen(struct wl_client *client, struct wl_resource *resource) {
    (void)client; (void)resource;
}

static void toplevel_set_minimized(struct wl_client *client, struct wl_resource *resource) {
    (void)client; (void)resource;
}

static const struct xdg_toplevel_interface toplevel_implementation = {
    .destroy = toplevel_destroy,
    .set_parent = toplevel_set_parent,
    .set_title = toplevel_set_title,
    .set_app_id = toplevel_set_app_id,
    .show_window_menu = toplevel_show_window_menu,
    .move = toplevel_move,
    .resize = toplevel_resize,
    .set_max_size = toplevel_set_max_size,
    .set_min_size = toplevel_set_min_size,
    .set_maximized = toplevel_set_maximized,
    .unset_maximized = toplevel_unset_maximized,
    .set_fullscreen = toplevel_set_fullscreen,
    .unset_fullscreen = toplevel_unset_fullscreen,
    .set_minimized = toplevel_set_minimized,
};

// --- xdg_surface implementation ---

static void xdg_surface_destroy(struct wl_client *client, struct wl_resource *resource) {
    (void)client;
    wl_resource_destroy(resource);
}

static void xdg_surface_get_toplevel(struct wl_client *client, struct wl_resource *resource, uint32_t id) {
    struct wl_resource *toplevel_resource = wl_resource_create(client, &xdg_toplevel_interface, wl_resource_get_version(resource), id);
    wl_resource_set_implementation(toplevel_resource, &toplevel_implementation, NULL, NULL);

    
    // IMPORTANT: You must send a configure event immediately for valid initial state
    struct wl_array states;
    wl_array_init(&states);
    xdg_toplevel_send_configure(toplevel_resource, 0, 0, &states);
    wl_array_release(&states);
    
    xdg_surface_send_configure(resource, 0); // Serial 0
}

static void xdg_surface_get_popup(struct wl_client *client, struct wl_resource *resource, uint32_t id, struct wl_resource *parent, struct wl_resource *positioner) {
    (void)client; (void)resource; (void)id; (void)parent; (void)positioner;
    // TODO: Implement popups
}

static void xdg_surface_set_window_geometry(struct wl_client *client, struct wl_resource *resource, int32_t x, int32_t y, int32_t width, int32_t height) {
    (void)client; (void)resource; (void)x; (void)y; (void)width; (void)height;
}

static void xdg_surface_ack_configure(struct wl_client *client, struct wl_resource *resource, uint32_t serial) {
    (void)client; (void)resource; (void)serial;
    // Client acknowledged our configuration
}

static const struct xdg_surface_interface xdg_surface_implementation = {
    .destroy = xdg_surface_destroy,
    .get_toplevel = xdg_surface_get_toplevel,
    .get_popup = xdg_surface_get_popup,
    .set_window_geometry = xdg_surface_set_window_geometry,
    .ack_configure = xdg_surface_ack_configure,
};

// --- xdg_wm_base implementation ---

static void wm_base_destroy(struct wl_client *client, struct wl_resource *resource) {
    (void)client;
    wl_resource_destroy(resource);
}

static void wm_base_create_positioner(struct wl_client *client, struct wl_resource *resource, uint32_t id) {
    (void)client; (void)resource; (void)id;
    // TODO: Implement positioner
}

static void wm_base_get_xdg_surface(struct wl_client *client, struct wl_resource *resource, uint32_t id, struct wl_resource *surface) {
    (void)surface;
    struct wl_resource *xdg_resource = wl_resource_create(client, &xdg_surface_interface, wl_resource_get_version(resource), id);
    wl_resource_set_implementation(xdg_resource, &xdg_surface_implementation, NULL, NULL);

}

static void wm_base_pong(struct wl_client *client, struct wl_resource *resource, uint32_t serial) {
    (void)client; (void)resource; (void)serial;
    // Pong!
}

static const struct xdg_wm_base_interface wm_base_implementation = {
    .destroy = wm_base_destroy,
    .create_positioner = wm_base_create_positioner,
    .get_xdg_surface = wm_base_get_xdg_surface,
    .pong = wm_base_pong,
};

static void shell_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id) {
    struct ember_server *server = data;
    struct wl_resource *resource = wl_resource_create(client, &xdg_wm_base_interface, version, id);
    wl_resource_set_implementation(resource, &wm_base_implementation, server, NULL);

}

int init_shell(struct ember_server *server) {
    server->xdg_shell_global = wl_global_create(server->wl_display, &xdg_wm_base_interface, 1, server, shell_bind);
    if (!server->xdg_shell_global) {
        fprintf(stderr, "Failed to create xdg_wm_base global\n");
        return -1;
    }
    printf("Initialized Wayland Globals (XDG Shell)\n");
    return 0;
}
