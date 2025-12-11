#include <stdio.h>
#include "ember.h"

// --- wl_data_source implementation ---
static void data_source_offer(struct wl_client *client, struct wl_resource *resource, const char *mime_type) {
    (void)client; (void)resource; (void)mime_type;
}

static void data_source_destroy(struct wl_client *client, struct wl_resource *resource) {
    (void)client;
    wl_resource_destroy(resource);
}

static void data_source_set_actions(struct wl_client *client, struct wl_resource *resource, uint32_t dnd_actions) {
    (void)client; (void)resource; (void)dnd_actions;
}

static const struct wl_data_source_interface data_source_interface = {
    .offer = data_source_offer,
    .destroy = data_source_destroy,
    .set_actions = data_source_set_actions,
};

// --- wl_data_device implementation ---
static void data_device_start_drag(struct wl_client *client, struct wl_resource *resource,
                                   struct wl_resource *source, struct wl_resource *origin,
                                   struct wl_resource *icon, uint32_t serial) {
    (void)client; (void)resource; (void)source; (void)origin; (void)icon; (void)serial;
}

static void data_device_set_selection(struct wl_client *client, struct wl_resource *resource,
                                      struct wl_resource *source, uint32_t serial) {
    (void)client; (void)resource; (void)source; (void)serial;
}

static void data_device_release(struct wl_client *client, struct wl_resource *resource) {
    (void)client;
    wl_resource_destroy(resource);
}

static const struct wl_data_device_interface data_device_interface = {
    .start_drag = data_device_start_drag,
    .set_selection = data_device_set_selection,
    .release = data_device_release,
};

// --- wl_data_device_manager implementation ---
static void ddm_create_data_source(struct wl_client *client, struct wl_resource *resource, uint32_t id) {
    (void)resource;
    struct wl_resource *source = wl_resource_create(client, &wl_data_source_interface, 3, id);
    if (!source) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(source, &data_source_interface, NULL, NULL);
}

static void ddm_get_data_device(struct wl_client *client, struct wl_resource *resource, uint32_t id, struct wl_resource *seat) {
    (void)resource; (void)seat;
    struct wl_resource *device = wl_resource_create(client, &wl_data_device_interface, 3, id);
    if (!device) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(device, &data_device_interface, NULL, NULL);

}

static const struct wl_data_device_manager_interface ddm_interface = {
    .create_data_source = ddm_create_data_source,
    .get_data_device = ddm_get_data_device,
};

static void ddm_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id) {
    (void)data;
    struct wl_resource *resource = wl_resource_create(client, &wl_data_device_manager_interface, version, id);
    wl_resource_set_implementation(resource, &ddm_interface, NULL, NULL);

}

int init_data_device_manager(struct ember_server *server) {
    server->ddm_global = wl_global_create(server->wl_display, &wl_data_device_manager_interface, 3, server, ddm_bind);
    if (!server->ddm_global) {
        fprintf(stderr, "Failed to create wl_data_device_manager global\n");
        return -1;
    }
    printf("Initialized Wayland Globals (Data Device Manager)\n");
    return 0;
}
