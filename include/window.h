#ifndef WINDOW_H
#define WINDOW_H

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct Window Window;

Window* window_create(const char* title);
bool window_running(Window* window);

void window_begin_frame(Window* window);
void window_end_frame(Window* window);
void window_destroy(Window* window);

double window_get_time();

int window_width(Window* window);
int window_height(Window* window);

#endif