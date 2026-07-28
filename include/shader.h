#ifndef SHADER_H
#define SHADER_H

typedef struct Shader Shader;

Shader* shader_create(const char* vert, const char* frag);

void shader_use(Shader* shader);

void shader_set_int(Shader* shader, const char* uniform, int value);
void shader_set_float(Shader* shader, const char* uniform, float value);

void shader_destroy(Shader* shader);

#endif