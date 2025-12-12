#include <stdio.h>
#include <stdlib.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include "ember.h"
#include "input.h"
#include "backend.h"

// 16x16 arrow cursor (macOS-style, vertical left edge)
// RGBA format: Each pixel is 4 bytes {R, G, B, A}
// B = Black outline, W = White fill, T = Transparent

static const unsigned char cursor_data[16 * 16 * 4] = {
    // Row 0
    255,255,255,255, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    // Row 1
    255,255,255,255, 255,255,255,255, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    // Row 2
    255,255,255,255, 0,0,0,255, 255,255,255,255, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    // Row 3
    255,255,255,255, 0,0,0,255, 0,0,0,255, 255,255,255,255, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    // Row 4
    255,255,255,255, 0,0,0,255, 0,0,0,255, 0,0,0,255, 255,255,255,255, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    // Row 5
    255,255,255,255, 0,0,0,255, 0,0,0,255, 0,0,0,255, 0,0,0,255, 255,255,255,255, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    // Row 6
    255,255,255,255, 0,0,0,255, 0,0,0,255, 0,0,0,255, 0,0,0,255, 0,0,0,255, 255,255,255,255, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    // Row 7
    255,255,255,255, 0,0,0,255, 0,0,0,255, 0,0,0,255, 0,0,0,255, 0,0,0,255, 0,0,0,255, 255,255,255,255, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    // Row 8
    255,255,255,255, 0,0,0,255, 0,0,0,255, 0,0,0,255, 0,0,0,255, 0,0,0,255, 0,0,0,255, 0,0,0,255, 255,255,255,255, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    // Row 9
    255,255,255,255, 0,0,0,255, 0,0,0,255, 0,0,0,255, 0,0,0,255, 0,0,0,255, 0,0,0,255, 0,0,0,255, 0,0,0,255, 255,255,255,255, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    // Row 10 (cutoff section)
    255,255,255,255, 0,0,0,255, 0,0,0,255, 0,0,0,255, 0,0,0,255, 0,0,0,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    // Row 11
    255,255,255,255, 0,0,0,255, 0,0,0,255, 255,255,255,255, 0,0,0,255, 0,0,0,255, 255,255,255,255, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    // Row 12
    255,255,255,255, 0,0,0,255, 255,255,255,255, 0,0,0,0, 255,255,255,255, 0,0,0,255, 0,0,0,255, 255,255,255,255, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    // Row 13
    255,255,255,255, 255,255,255,255, 0,0,0,0, 0,0,0,0, 255,255,255,255, 0,0,0,255, 0,0,0,255, 255,255,255,255, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    // Row 14
    255,255,255,255, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 255,255,255,255, 0,0,0,255, 0,0,0,255, 255,255,255,255, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    // Row 15
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 255,255,255,255, 255,255,255,255, 255,255,255,255, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
};

void init_cursor(struct ember_server *server) {
    server->cursor.x = server->mode.hdisplay / 2.0;
    server->cursor.y = server->mode.vdisplay / 2.0;
    server->cursor.size = 16.0f;
    server->cursor.visible = 1;
    server->cursor.texture_id = 0;
}

void render_cursor(struct ember_server *server) {
    if (!server->cursor.visible) return;

    // Create cursor texture if not exists
    if (!server->cursor.texture_id) {
        printf("Creating cursor texture with %zu bytes of data\n", sizeof(cursor_data));
        glGenTextures(1, &server->cursor.texture_id);
        glBindTexture(GL_TEXTURE_2D, server->cursor.texture_id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, cursor_data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        printf("Cursor texture ID: %u\n", server->cursor.texture_id);
    }

    glBindTexture(GL_TEXTURE_2D, server->cursor.texture_id);
    
    glUseProgram(server->shader_program);
    glUniform1i(server->loc_tex, 0);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float cx = server->cursor.x;
    float cy = server->cursor.y;
    float cursor_size = server->cursor.size * 2.0f; // Scale up for visibility
    
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
