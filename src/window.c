#include "window.h"

#define GLFW_INCLUDE_NONE
#include <glad/gl.h>
#include <GLFW/glfw3.h>


struct Window {
    GLFWwindow* handle;
    int width;
    int height;
};

Window* window_create(const char* title) {
    if (!glfwInit()) return NULL;

    Window* window = malloc(sizeof(Window));

    if (!window) {
        glfwTerminate();
        return NULL;
    }

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();

    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    window->width = mode->width;
    window->height = mode->height;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE); // No title bar or borders
    glfwWindowHint(GLFW_FLOATING, GLFW_TRUE); // Always on top
    glfwWindowHint(GLFW_MOUSE_PASSTHROUGH, GLFW_TRUE);

    window->handle = glfwCreateWindow(window->width, window->height, title, NULL, NULL);
    if (!window->handle) {
        free(window);
        glfwTerminate();
        return NULL;
    }

    glfwSetWindowPos(window->handle, 0, 0);

    glfwMakeContextCurrent(window->handle);

    if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress)) {  // glad2: gladLoadGL, not gladLoadGLLoader
        glfwDestroyWindow(window->handle);
        free(window);
        glfwTerminate();
        return NULL;
    }

    glfwSwapInterval(1); // Enable VSync
    glViewport(0, 0, window->width, window->height);

    return window;
}

void window_destroy(Window* window) {
    if (!window) return;

    if (window->handle) {
        glfwDestroyWindow(window->handle);
    }

    glfwTerminate();
    free(window);
}

void window_clear(Window* window) {
    if (!window) return;

    glClearColor(0.0f, 0.0f, 0.0f, 0.5f);
    glClear(GL_COLOR_BUFFER_BIT);

    glfwSwapBuffers(window->handle);
    glfwPollEvents();
}

bool window_running(Window* window) {
    return !glfwWindowShouldClose(window->handle);
}