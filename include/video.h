#ifndef VIDEO_H
#define VIDEO_H

#include <stdbool.h>

typedef struct Video Video;

Video* video_open(const char* path);
void video_close(Video* v);

int video_width(Video* v);
int video_height(Video* v);


// Returns a pointer to RGB24 data, and internally decides
// if time advanced far enough to continue to the next frame,
// in that case, it set out to true
unsigned char* video_get_frame(Video* v, double time, bool* out);

bool video_finished(Video* v);

#endif