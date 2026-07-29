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

// Native X11 window XID for this window (0 on failure or non-X11 backends).
// Used by the capturer so it can identify and exclude this window
unsigned long window_native_id(Window* window);

#endif