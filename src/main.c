#include "window.h"
#include "video.h"

int main() {
    Window* window = window_create("Bad Apple");
    if (!window) return -1;

    Video* video = video_open("badapple.mp4");
    if (!video) return -1;

    while (window_running(window)) {
        window_begin_frame(window);

        bool updated;
        unsigned char* frame = video_get_frame(video, window_get_time(), &updated);

        if (updated) {
            // Create texture -> Pass to shader
        }

        window_end_frame(window);
    
        if (video_finished(video)) break;
    }
    
    video_close(video);
    window_destroy(window);

    return 0;
}