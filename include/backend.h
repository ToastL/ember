#ifndef BACKEND_H
#define BACKEND_H

#include "ember.h"

// drm.c
int init_drm(struct ember_server *server);
void handle_drm_event(struct ember_server *server);
uint32_t get_fb_for_bo(int fd, struct gbm_bo *bo);

// egl.c
int init_egl(struct ember_server *server);

// output.c
int init_output(struct ember_server *server);

#endif
