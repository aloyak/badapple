#include "window.h"
#include "video.h"
#include "texture.h"
#include "shader.h"

int main() {
    Window* window = window_create("Bad Apple");
    if (!window) return 1;

    Video* video = video_open("video/badapple.mp4");
    if (!video) {
        window_destroy(window);
        return 1;
    }

    Texture* texture = texture_create(video_width(video), video_height(video));

    Shader* shader = shader_create("shaders/vertex.glsl", "shaders/fragment.glsl");
    if(!shader) {
        texture_destroy(texture);
        video_close(video);
        window_destroy(window);
        return 1;
    }

    shader_use(shader);
    shader_set_int(shader, "frameTexture", 0);

    while (window_running(window)) {
        window_begin_frame(window);

        bool updated;
        unsigned char* frame = video_get_frame(video, window_get_time(), &updated);

        if (updated) {
            texture_update(texture, frame);
        }

        shader_use(shader);
        texture_bind(texture, 0);
        //quad draw

        window_end_frame(window);
    
        if (video_finished(video)) break;
    }
    
    shader_destroy(shader);
    texture_destroy(texture);
    video_close(video);
    window_destroy(window);

    return 0;
}