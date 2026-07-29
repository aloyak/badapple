#include "window.h"
#include "video.h"
#include "capturer.h"
#include "texture.h"
#include "shader.h"
#include "quad.h"

int main() {
    Window* window = window_create("Bad Apple");
    if (!window) return 1;

    Capturer* capturer = capturer_create(
        window_width(window), window_height(window), window_native_id(window)
    );
    
    if (!capturer) {
        window_destroy(window);
        return 1;
    }

    Video* video = video_open("video/badapple.mp4");
    if (!video) {
        window_destroy(window);
        capturer_destroy(capturer);
        return 1;
    }

    Texture* texture = texture_create(video_width(video), video_height(video), TEXTURE_FORMAT_RGB8);

    Shader* shader = shader_create("shaders/vertex.glsl", "shaders/fragment.glsl");
    if (!shader) {
        texture_destroy(texture);
        video_close(video);
        capturer_destroy(capturer);
        window_destroy(window);
        return 1;
    }

    Quad* quad = quad_create();

    shader_use(shader);
    shader_set_int(shader, "u_frameTexture", 0);
    shader_set_int(shader, "u_screenTexture", 1);

    while (window_running(window)) {
        window_begin_frame(window);

        bool updated;
        unsigned char* frame = video_get_frame(video, window_get_time(), &updated);
        if (updated) {
            texture_update(texture, frame);
        }

        capturer_capture(capturer);

        shader_use(shader);
        texture_bind(texture, 0);
        texture_bind(capturer_texture(capturer), 1);
        quad_draw(quad);

        window_end_frame(window);

        if (video_finished(video)) break;
    }

    quad_destroy(quad);
    shader_destroy(shader);
    texture_destroy(texture);
    video_close(video);
    capturer_destroy(capturer);
    window_destroy(window);

    return 0;
}