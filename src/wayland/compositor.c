#include <stdlib.h>
#include <stdio.h>
#include <wayland-server.h>
#include "ember.h"
#include "wayland/protocols.h"

static void surface_destroy(struct wl_client *client, struct wl_resource *resource) {
    (void)client;
    wl_resource_destroy(resource);
}

static void surface_attach(struct wl_client *client, struct wl_resource *resource,
                           struct wl_resource *buffer, int32_t x, int32_t y) {
    (void)client; (void)x; (void)y;
    struct ember_surface *surface = wl_resource_get_user_data(resource);
    surface->buffer = buffer;
}

static void surface_damage(struct wl_client *client, struct wl_resource *resource,
                           int32_t x, int32_t y, int32_t width, int32_t height) {
    (void)client; (void)resource; (void)x; (void)y; (void)width; (void)height;
    // TODO: Track damage
}

static void surface_frame(struct wl_client *client, struct wl_resource *resource, uint32_t callback) {
    (void)resource;
    // Create the callback resource (required by protocol)
    struct wl_resource *cb = wl_resource_create(client, &wl_callback_interface, 1, callback);
    if (!cb) {
        wl_resource_post_no_memory(resource);
        return;
    }
    // For now, immediately signal done (proper compositor would do this after render)
    wl_callback_send_done(cb, 0);
    wl_resource_destroy(cb);
}

static void surface_set_opaque_region(struct wl_client *client, struct wl_resource *resource,
                                      struct wl_resource *region) {
    (void)client; (void)resource; (void)region;
}

static void surface_set_input_region(struct wl_client *client, struct wl_resource *resource,
                                     struct wl_resource *region) {
    (void)client; (void)resource; (void)region;
}

static void surface_commit(struct wl_client *client, struct wl_resource *resource) {
    (void)client; (void)resource;
}

static void surface_set_buffer_transform(struct wl_client *client, struct wl_resource *resource, int32_t transform) {
    (void)client; (void)resource; (void)transform;
}

static void surface_set_buffer_scale(struct wl_client *client, struct wl_resource *resource, int32_t scale) {
    (void)client; (void)resource; (void)scale;
}

static void surface_damage_buffer(struct wl_client *client, struct wl_resource *resource,
                                  int32_t x, int32_t y, int32_t width, int32_t height) {
    (void)client; (void)resource; (void)x; (void)y; (void)width; (void)height;
}

static const struct wl_surface_interface surface_interface = {
    .destroy = surface_destroy,
    .attach = surface_attach,
    .damage = surface_damage,
    .frame = surface_frame,
    .set_opaque_region = surface_set_opaque_region,
    .set_input_region = surface_set_input_region,
    .commit = surface_commit,
    .set_buffer_transform = surface_set_buffer_transform,
    .set_buffer_scale = surface_set_buffer_scale,
    .damage_buffer = surface_damage_buffer,
};


// Surface Destructor
static void surface_resource_destroy(struct wl_resource *resource) {
    struct ember_surface *surface = wl_resource_get_user_data(resource);
    if (surface) {
        wl_list_remove(&surface->link);
        free(surface);
    }
}

static void compositor_create_surface(struct wl_client *client, struct wl_resource *resource, uint32_t id) {
    struct ember_server *server = wl_resource_get_user_data(resource);
    struct wl_resource *surface_resource = wl_resource_create(client, &wl_surface_interface, wl_resource_get_version(resource), id);
    
    struct ember_surface *surface = calloc(1, sizeof(struct ember_surface));
    if (!surface) {
        wl_resource_post_no_memory(surface_resource);
        return;
    }
    
    surface->resource = surface_resource;
    wl_list_insert(&server->surfaces, &surface->link);

    wl_resource_set_implementation(surface_resource, &surface_interface, surface, surface_resource_destroy);

}

// --- wl_region implementation ---

static void region_destroy(struct wl_client *client, struct wl_resource *resource) {
    (void)client;
    wl_resource_destroy(resource);
}

static void region_add(struct wl_client *client, struct wl_resource *resource,
                       int32_t x, int32_t y, int32_t width, int32_t height) {
    (void)client; (void)resource; (void)x; (void)y; (void)width; (void)height;
    // For now, we don't track region geometry (just need the object to exist)
}

static void region_subtract(struct wl_client *client, struct wl_resource *resource,
                            int32_t x, int32_t y, int32_t width, int32_t height) {
    (void)client; (void)resource; (void)x; (void)y; (void)width; (void)height;
}

static const struct wl_region_interface region_interface = {
    .destroy = region_destroy,
    .add = region_add,
    .subtract = region_subtract,
};

static void compositor_create_region(struct wl_client *client, struct wl_resource *resource, uint32_t id) {
    (void)resource;
    struct wl_resource *region_resource = wl_resource_create(client, &wl_region_interface, 1, id);
    if (!region_resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(region_resource, &region_interface, NULL, NULL);
}

static const struct wl_compositor_interface compositor_interface = {
    .create_surface = compositor_create_surface,
    .create_region = compositor_create_region,
};

static void compositor_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id) {
    struct ember_server *server = data;
    struct wl_resource *resource = wl_resource_create(client, &wl_compositor_interface, version, id);
    wl_resource_set_implementation(resource, &compositor_interface, server, NULL);

}

// This function implementation stays here for now
int init_wayland_globals(struct ember_server *server) {
    server->compositor = wl_global_create(server->wl_display, &wl_compositor_interface, 4, server, compositor_bind);
    if (!server->compositor) {
        fprintf(stderr, "Failed to create wl_compositor global\n");
        return -1;
    }
    
    if (init_shm(server) < 0) return -1;
    if (init_shell(server) < 0) return -1;
    if (init_data_device_manager(server) < 0) return -1;
    
    printf("Initialized Wayland Globals (Compositor + SHM + Shell + DDM)\n");
    return 0;
}
