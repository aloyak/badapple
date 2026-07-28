#include "window.h"
#include "video.h"
#include "texture.h"

int main() {
    Window* window = window_create("Bad Apple");
    if (!window) return 1;

    Video* video = video_open("badapple.mp4");
    if (!video) {
        window_destroy(window);
        return 1;
    }

    Texture* texture = create_texture(video_width(video), video_height(video));

    while (window_running(window)) {
        window_begin_frame(window);

        bool updated;
        unsigned char* frame = video_get_frame(video, window_get_time(), &updated);

        if (updated) {
            texture_update(texture, frame);
        }

        window_end_frame(window);
    
        if (video_finished(video)) break;
    }
    
    video_close(video);
    window_destroy(window);
    texture_destroy(texture);

    return 0;
}