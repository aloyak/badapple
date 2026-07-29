#ifndef CAPTURER_H
#define CAPTURER_H

#include "texture.h"

typedef struct Capturer Capturer;

Capturer* capturer_create(int width, int height, unsigned long own_id);

void capturer_capture(Capturer* capturer);
Texture* capturer_texture(Capturer* capturer);
void capturer_destroy(Capturer* capturer);

#endif