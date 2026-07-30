#include "window.h"
#include "video.h"
#include "capturer.h"
#include "texture.h"
#include "shader.h"
#include "quad.h"
#include "parser.h"

static void print_usage() {
    printf("Usage: BadApple [options]\n");
    printf("Options:\n");
    printf("  --help, -h          Show this help message\n");
    printf("  --video <path>      Path to video file\n");
    printf("  --fshader <path>    Path to fragment shader\n");
    printf("  --vshader <path>    Path to vertex shader\n");
}

int main(int argc, char** argv) {
    if (parse_flag(argc, argv, "--help") || parse_flag(argc, argv, "-h")) {
        print_usage();
        return 0;
    }

    Window* window = window_create("Bad Apple");
    if (!window) return 1;

    window_passthrough(window, true);

    Capturer* capturer = capturer_create(
        window_width(window), window_height(window), window_native_id(window)
    );

    if (!capturer) {
        window_destroy(window);
        return 1;
    }

    const char* video_path = parse_option(argc, argv, "--video");
    if (!video_path) video_path = "video/badapple.mp4";
    Video* video = video_open(video_path);
    if (!video) {
        window_destroy(window);
        capturer_destroy(capturer);
        return 1;
    }

    Texture* texture = texture_create(video_width(video), video_height(video), TEXTURE_FORMAT_RGB8);

    const char* vshader_path = parse_option(argc, argv, "--vshader");
    const char* fshader_path = parse_option(argc, argv, "--fshader");
    if (!vshader_path) vshader_path = "shaders/vertex.glsl";
    if (!fshader_path) fshader_path = "shaders/fragment.glsl";
    
    Shader* shader = shader_create(vshader_path, fshader_path);
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