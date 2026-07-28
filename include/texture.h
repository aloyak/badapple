#ifndef TEXTURE_H
#define TEXTURE_H

typedef struct Texture Texture;

Texture* texture_create(int width, int height);

void texture_update(Texture* tex, const unsigned char* data);
void texture_bind(Texture* tex, unsigned int unit);
void texture_destroy(Texture* tex);

#endif.