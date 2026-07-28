#ifndef QUAD_H
#define QUAD_H

typedef struct Quad Quad;

Quad* quad_create();

void quad_draw(Quad* quad);
void quad_destroy(Quad* quad);

#endif