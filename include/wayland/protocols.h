#ifndef PROTOCOLS_H
#define PROTOCOLS_H

#include "ember.h"

int init_wayland_globals(struct ember_server *server);

// Individual protocol initializers (called by init_wayland_globals)
int init_compositor(struct ember_server *server);
int init_shm(struct ember_server *server);
int init_shell(struct ember_server *server);
int init_seat(struct ember_server *server);
int init_data_device_manager(struct ember_server *server);

#endif
