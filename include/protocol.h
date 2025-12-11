#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "ember.h"

int init_wayland_globals(struct ember_server *server);
int init_shm(struct ember_server *server);
int init_shell(struct ember_server *server);
int init_data_device_manager(struct ember_server *server);

#endif
