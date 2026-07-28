#include "texture.h"

#include <glad/gl.h>
#include <stdlib.h>

struct Texture {
    unsigned int id;
    int width;
    int height;
};

Texture* create_texture(int width, int height) {
    Texture* t = calloc(1, sizeof(Texture));
    if (!t) return NULL;

    t->width = width;
    t->height = height;

    glGenTextures(1, &t->id);

    glTextureParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); //NOTE: Check for GL_NEAREST
    glTextureParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // OpenGL defaults to assuming rows are 4-byte aligned,
    // If width*3 isn't a multiple of 4,the image comes out 
    // sheared. Settings this to 1 just tells it to always 
    // trust the data as given
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);

    glBindTexture(GL_TEXTURE_2D, 0);

    return t;
}

void texture_update(Texture* tex, const unsigned char* data) {
    if (!tex || !data) return;

    glBindTexture(GL_TEXTURE_2D, tex->id);

    // Re writes to the already allocated texture, a lot cheaper than
    // calling glTexImage2D again as that would reallocate instead of overwrite
    // "Sub" = substitute?
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tex->width, tex->height,
                     GL_RGB, GL_UNSIGNED_BYTE, data);

    glBindTexture(GL_TEXTURE_2D, 0);
}

void texture_bind(Texture* tex, unsigned int unit) {
    if (!tex) return;

    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, tex->id);
}

void texture_destroy(Texture* tex) {
    if (!tex) return;
    glDeleteTextures(1, &tex->id);
    free(tex);
}