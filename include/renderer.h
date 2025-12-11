#ifndef RENDERER_H
#define RENDERER_H

#include "ember.h"

int init_renderer(struct ember_server *server);
void render_frame(struct ember_server *server);

#endif
