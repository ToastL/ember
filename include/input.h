#ifndef INPUT_H
#define INPUT_H

#include "ember.h"

// input.c (libinput)
int init_input(struct ember_server *server);
int on_input_readable(int fd, uint32_t mask, void *data);

// cursor.c
void init_cursor(struct ember_server *server);
void render_cursor(struct ember_server *server);

// dispatch.c
void dispatch_keyboard_key(struct ember_server *server, uint32_t key, uint32_t state);
void dispatch_keyboard_modifiers(struct ember_server *server, uint32_t mods_depressed, 
                                  uint32_t mods_latched, uint32_t mods_locked, uint32_t group);
void dispatch_pointer_motion(struct ember_server *server, double x, double y);
void dispatch_pointer_button(struct ember_server *server, uint32_t button, uint32_t state);
void set_keyboard_focus(struct ember_server *server, struct ember_surface *surface);
void update_focus(struct ember_server *server);

#endif
