#ifndef WINDOW_H
#define WINDOW_H

#include <stdbool.h>
#include <stdlib.h>

typedef struct Window Window;

Window* window_create(const char* title);
bool window_running(Window* window);

void window_clear(Window* window);
void window_destroy(Window* window);

#endif