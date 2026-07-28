#include "quad.h"

#include <glad/gl.h>
#include <stdlib.h>

struct Quad {
    unsigned int vao;
    unsigned int vbo;
    unsigned int ebo;
};

static const float vertices[] = {
//  pos.x, pos.y,      u,    v
    -1.0f, -1.0f,     0.0f, 0.0f, // bottom-left
     1.0f, -1.0f,     1.0f, 0.0f, // bottom-right
     1.0f,  1.0f,     1.0f, 1.0f, // top-right
    -1.0f,  1.0f,     0.0f, 1.0f, // top-left 
};

static const unsigned int indices[] = {
    0, 1, 2, // bottom-left, bottom-right, top-right
    0, 2, 3, // bottom-left, top-right, top-left
};

Quad* quad_create() {
    Quad* q = calloc(1, sizeof(Quad));
    if (!q) return NULL;

    glGenVertexArrays(1, &q->vao);
    glGenBuffers(1, &q->vbo);
    glGenBuffers(1, &q->ebo);

    glBindVertexArray(q->vao);

    glBindBuffer(GL_ARRAY_BUFFER, q->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, q->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // Attribute 0, position (aPos in vertex.glsl), 2 floats, offset 0
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Attribute 1, UV (aTexCoord in vertex.glsl), 2 floats, offset 2*sizeof(float)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return q;
}

void quad_draw(Quad* quad) {
    if (!quad) return;

    glBindVertexArray(quad->vao);

    // 6 indices (2 triangles * 3), GL_UNSIGNED_INT matching the indices array's type
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void quad_destroy(Quad* quad) {
    if (!quad) return;
    glDeleteVertexArrays(1, &quad->vao);
    glDeleteBuffers(1, &quad->vbo);
    glDeleteBuffers(1, &quad->ebo);
    free(quad);
}