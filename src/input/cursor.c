#include <stdio.h>
#include <stdlib.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include "ember.h"
#include "input.h"
#include "backend.h"

void init_cursor(struct ember_server *server) {
    // Initialize cursor at center of screen
    server->cursor.x = server->mode.hdisplay / 2.0;
    server->cursor.y = server->mode.vdisplay / 2.0;
    server->cursor.size = 24.0f;
    server->cursor.visible = 1;
    server->cursor.texture_id = 0;
}

void render_cursor(struct ember_server *server) {
    if (!server->cursor.visible) return;

    // Create cursor texture if not exists
    if (!server->cursor.texture_id) {
        glGenTextures(1, &server->cursor.texture_id);
        glBindTexture(GL_TEXTURE_2D, server->cursor.texture_id);
        // White cursor
        unsigned char white[] = {255, 255, 255, 255};
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    glBindTexture(GL_TEXTURE_2D, server->cursor.texture_id);
    
    glUseProgram(server->shader_program);
    glUniform1i(server->loc_tex, 0);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    float cx = server->cursor.x;
    float cy = server->cursor.y;
    float cursor_size = server->cursor.size;
    
    float screen_w = (float)server->mode.hdisplay;
    float screen_h = (float)server->mode.vdisplay;
    
    float x0 = (cx / screen_w) * 2.0f - 1.0f;
    float y0 = 1.0f - (cy / screen_h) * 2.0f;
    float x1 = ((cx + cursor_size) / screen_w) * 2.0f - 1.0f;
    float y1 = 1.0f - ((cy + cursor_size) / screen_h) * 2.0f;
    
    GLfloat cursor_verts[] = {
        x0, y0, 0.0f,
        x0, y1, 0.0f,
        x1, y1, 0.0f,
        x1, y0, 0.0f,
    };
    
    GLfloat cursor_tex[] = {
        0.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f,
        1.0f, 0.0f
    };
    
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, cursor_verts);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, cursor_tex);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}
