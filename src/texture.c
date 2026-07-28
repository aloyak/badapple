#include "texture.h"

#include <glad/gl.h>
#include <stdlib.h>

struct Texture {
    unsigned int id;
    int width;
    int height;
    TextureFormat format;
};

static void format_to_gl(TextureFormat format, unsigned int* internal_format, 
                        unsigned int* data_format, int* bytes_per_pixel) {

    switch (format) {
        case TEXTURE_FORMAT_RGB8:
            *internal_format = GL_RGB;
            *data_format = GL_RGB;
            *bytes_per_pixel = 3;
            break;
        case TEXTURE_FORMAT_BGRA8:
            *internal_format = GL_RGBA;
            *data_format = GL_BGRA;
            *bytes_per_pixel = 4;
            break;
    }
}

Texture* texture_create(int width, int height, TextureFormat format) {
    Texture* t = calloc(1, sizeof(Texture));
    if (!t) return NULL;
 
    t->width = width;
    t->height = height;
    t->format = format;
 
    unsigned int internal_format, data_format;
    int bytes_per_pixel;
    format_to_gl(format, &internal_format, &data_format, &bytes_per_pixel);
 
    glGenTextures(1, &t->id);
    glBindTexture(GL_TEXTURE_2D, t->id);
 
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); //NOTE: Check for GL_NEAREST
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
 
    // OpenGL defaults to assuming rows are 4-byte aligned RGB8 rows (3
    // bytes/pixel) can violate that depending on width, causing shearing
    // BGRA8 rows (4 bytes/pixel) are always 4-byte aligned regardless of
    // width, so this only actually matters for the RGB8 case, but setting it
    // to the pixel's own byte size is correct and harmless for both.
    glPixelStorei(GL_UNPACK_ALIGNMENT, bytes_per_pixel);
 
    glTexImage2D(GL_TEXTURE_2D, 0, (int)internal_format, width, height, 0,
                 data_format, GL_UNSIGNED_BYTE, NULL);
 
    glBindTexture(GL_TEXTURE_2D, 0);
 
    return t;
}

void texture_update(Texture* tex, const void* data) {
    if (!tex || !data) return;
 
    unsigned int internal_format, data_format;
    int bytes_per_pixel;
    format_to_gl(tex->format, &internal_format, &data_format, &bytes_per_pixel);
 
    glBindTexture(GL_TEXTURE_2D, tex->id);
 
    // Re writes to the already allocated texture, a lot cheaper than
    // calling glTexImage2D again as that would reallocate instead of overwrite
    // "Sub" = substitute?
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tex->width, tex->height,
                     data_format, GL_UNSIGNED_BYTE, data);
 
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