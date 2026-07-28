#ifndef TEXTURE_H
#define TEXTURE_H

typedef struct Texture Texture;

typedef enum {
    TEXTURE_FORMAT_RGB8, // Video frames (3 bytes per pixel)
    TEXTURE_FORMAT_BGRA8, // X11's screen capture (4 bytes per pixel)
} TextureFormat;

Texture* texture_create(int width, int height, TextureFormat format);

void texture_update(Texture* tex, const void* data);
void texture_bind(Texture* tex, unsigned int unit);
void texture_destroy(Texture* tex);

#endif