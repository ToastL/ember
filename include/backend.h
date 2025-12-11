#ifndef BACKEND_H
#define BACKEND_H

#include "ember.h"

// Initialize the DRM/GBM/EGL stack
int init_graphics(struct ember_server *server);

// Initialize the DRM output (Modeset)
int init_output(struct ember_server *server);

// Render a single frame
void render_frame(struct ember_server *server);

// Handle DRM events (Page Flip completion)
void handle_drm_event(struct ember_server *server);

// Upload client pixels to texture
void update_surface_texture(struct ember_surface *surface, void *data, int width, int height, int stride, uint32_t format);

#endif
