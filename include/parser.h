#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>
#include <string.h>

static inline bool parse_flag(int argc, char** argv, const char* flag) {
    for (int i = 0; i < argc; ++i) {
        if (strcmp(argv[i], flag) == 0) {
            return true;
        }
    }
    return false;
}

static inline const char* parse_option(int argc, char** argv, const char* option) {
    for (int i = 0; i < argc - 1; ++i) {
        if (strcmp(argv[i], option) == 0) {
            return argv[i + 1];
        }
    }
    return NULL;
}

#endif