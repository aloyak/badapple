#include "window.h"

#include <SDL3/SDL.h>
#include <glad/gl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define Window X11Window
#include <X11/Xlib.h>
#include <X11/extensions/shape.h>
#undef Window

struct Window {
    SDL_Window* handle;
    SDL_GLContext gl_context;
    int width;
    int height;
    bool should_close;
};

Window* window_create(const char* title) {
    if (!SDL_Init(SDL_INIT_VIDEO)) return NULL;

    Window* window = malloc(sizeof(Window));

    if (!window) {
        SDL_Quit();
        return NULL;
    }

    window->should_close = false;

    SDL_DisplayID display = SDL_GetPrimaryDisplay();
    const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(display);
    window->width = mode ? mode->w : 800;
    window->height = mode ? mode->h : 600;

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_WindowFlags flags = SDL_WINDOW_OPENGL | 
                            SDL_WINDOW_TRANSPARENT | 
                            SDL_WINDOW_BORDERLESS | // No title bar or borders
                            SDL_WINDOW_ALWAYS_ON_TOP | // Always on top
                            SDL_WINDOW_NOT_FOCUSABLE | 
                            SDL_WINDOW_FULLSCREEN;

    window->handle = SDL_CreateWindow(title, window->width, window->height, flags);
    if (!window->handle) {
        printf("Failed to create SDL window\n");
        free(window);
        SDL_Quit();
        return NULL;
    }

    //SDL_SetWindowPosition(window->handle, 0, 0);

    window->gl_context = SDL_GL_CreateContext(window->handle);
    if (!window->gl_context) {
        SDL_DestroyWindow(window->handle);
        free(window);
        SDL_Quit();
        return NULL;
    }

    SDL_GL_MakeCurrent(window->handle, window->gl_context);

    if (!gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress)) {  // glad2: gladLoadGL, not gladLoadGLLoader
        SDL_GL_DestroyContext(window->gl_context);
        SDL_DestroyWindow(window->handle);
        free(window);
        SDL_Quit();
        return NULL;
    }

    SDL_GL_SetSwapInterval(1); // Enable VSync
    glViewport(0, 0, window->width, window->height);

    SDL_SyncWindow(window->handle);

    return window;
}

void window_destroy(Window* window) {
    if (!window) return;

    if (window->gl_context) {
        SDL_GL_DestroyContext(window->gl_context);
    }

    if (window->handle) {
        SDL_DestroyWindow(window->handle);
    }

    SDL_Quit();
    free(window);
}

void window_begin_frame(Window* window) {
    if (!window) return;

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void window_end_frame(Window* window) {
    SDL_GL_SwapWindow(window->handle);
    
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            window->should_close = true;
        }
    }
}

bool window_running(Window* window) {
    return window ? !window->should_close : false;
}

int window_width(Window* window) { return window ? window->width : 0; }
int window_height(Window* window) { return window ? window->height : 0; }

double window_get_time() {
    return (double)SDL_GetPerformanceCounter() / (double)SDL_GetPerformanceFrequency();
}

unsigned long window_native_id(Window* window) {
    if (!window || !window->handle) return 0;

    SDL_PropertiesID props = SDL_GetWindowProperties(window->handle);
    if (!props) return 0;

    // Only for X11 right now
    Sint64 xid = SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
    return (unsigned long)xid;
}

// Loop up the window hierarchy to find the top-level ancestor of a given window
X11Window window_resolve_toplevel(Display* display, X11Window root, X11Window win) {
    X11Window current = win;

    for (;;) {
        X11Window root_return, parent_return;
        X11Window* children = NULL;
        unsigned int nchildren = 0;

        if (!XQueryTree(display, current, &root_return, &parent_return, &children, &nchildren)) {
            return current;
        }
        if (children) XFree(children);

        if (parent_return == root || parent_return == 0) {
            return current;
        }
        current = parent_return;
    }
}

void window_passthrough(Window* window, bool enabled) {
    if (!window || !window->handle) return;

    SDL_PropertiesID props = SDL_GetWindowProperties(window->handle);
    if (!props) return;
    
    Display* display = (Display*)SDL_GetPointerProperty(props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
    X11Window xid = (X11Window)SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
    if (!display || !xid) {
        fprintf(stderr, "window: click-through requires the X11 backend\n");
        return;
    }

    int event_base, error_base;
    if (!XShapeQueryExtension(display, &event_base, &error_base)) {
        fprintf(stderr, "window: XShape extension not available, can't set click-through\n");
        return;
    }

    int screen = DefaultScreen(display);
    X11Window root = RootWindow(display, screen);
    X11Window toplevel = window_resolve_toplevel(display, root, xid);

    if (enabled) {
        XShapeCombineRectangles(display, toplevel, ShapeInput, 0, 0, NULL, 0, ShapeSet, 0);
        if (toplevel != xid) {
            XShapeCombineRectangles(display, xid, ShapeInput, 0, 0, NULL, 0, ShapeSet, 0);
        }
    } else {
        XRectangle rect = { 0, 0, (unsigned short)window->width, (unsigned short)window->height };
        XShapeCombineRectangles(display, toplevel, ShapeInput, 0, 0, &rect, 1, ShapeSet, 0);
        if (toplevel != xid) {
            XShapeCombineRectangles(display, xid, ShapeInput, 0, 0, &rect, 1, ShapeSet, 0);
        }
    }

    XFlush(display);
}