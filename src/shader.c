#include "shader.h"

#include <glad/gl.h>
#include <stdlib.h>
#include <stdio.h>

struct Shader {
    unsigned int program;
};

static const char* read_source(const char* path) {
    FILE* file = fopen(path, "rb");
    if (!file) return NULL;

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);

    char* source = malloc(size + 1);
    if (!source) {
        fclose(file);
        return NULL;
    }

    fread(source, 1, size, file);
    source[size] = '\0';

    fclose(file);
    return source;
}

static unsigned int compile_stage(unsigned int type, const char* source) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL); // 1 = one source string
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        fprintf(stderr, "Shader compile error! (%s):\n%s\n",
                type == GL_VERTEX_SHADER ? "vertex" : "fragment", log);
        glDeleteShader(shader);

        return 0;
    }

    return shader;
}

Shader* shader_create(const char* vert_path, const char* frag_path) {
    const char* vert_source = read_source(vert_path);
    const char* fragment_src = read_source(frag_path);
    if (!vert_source) return NULL;
    if (!fragment_src) {
        free((void*)vert_source);
        return NULL;
    }
    
    unsigned int vs = compile_stage(GL_VERTEX_SHADER, vert_source);
    if (!vs) return NULL;

    unsigned int fs = compile_stage(GL_FRAGMENT_SHADER, fragment_src);
    if (!fs) {
        glDeleteShader(vs);
        return NULL;
    }

    unsigned int program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(program, sizeof(log), NULL, log);
    
        fprintf(stderr, "shader link error:\n%s\n", log);
        glDeleteShader(vs);
        glDeleteShader(fs);
        glDeleteProgram(program);
        return NULL;
    }

    // Once linked, the vertex and fragment stages are already inside
    // the program object, and thus they are not needed
    glDeleteShader(vs);
    glDeleteShader(fs);

    Shader* s = calloc(1, sizeof(Shader));
    if (!s) {
        glDeleteProgram(program);
        return NULL;
    }
    s->program = program;

    return s;
}

void shader_use(Shader* shader) {
    if (!shader) return;
    glUseProgram(shader->program);
}

void shader_set_int(Shader* shader, const char* uniform, int value) {
    if (!shader) return;
    int location = glGetUniformLocation(shader->program, uniform);
    glUniform1i(location, value);
}
void shader_set_float(Shader* shader, const char* uniform, float value) {
    if (!shader) return;
    int location = glGetUniformLocation(shader->program, uniform);
    glUniform1f(location, value);
}

void shader_destroy(Shader* shader) {
    if (!shader) return;
    glDeleteProgram(shader->program);
    free(shader);
}