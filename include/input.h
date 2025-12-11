#ifndef INPUT_H
#define INPUT_H

#include "ember.h"

// input.c (libinput)
int init_input(struct ember_server *server);
int on_input_readable(int fd, uint32_t mask, void *data);

// cursor.c
void init_cursor(struct ember_server *server);
void render_cursor(struct ember_server *server);
void rotate_cursor_texture(struct ember_server *server); // Optional if needed later

#endif
