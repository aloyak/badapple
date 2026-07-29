#include "capturer.h"

// Rather than screenshotting the final composited root window (which would
// include our own transparent overlay's previous frame and create a feedback
// loop), this builds the background image itself: it asks the X server to
// redirect every OTHER top-level window's rendering into its own off-screen
// pixmap, reads those pixmaps back, and manually blends them together bottom-to-top. 
// The window is never part of that set, so there's no loop
//
//   * The window list is captured once, at capturer_create() time. Windows
//     opened/closed/moved/resized afterwards won't be reflected. The proper
//     fix is to track SubstructureNotify events on the root window and keep
//     the window list live
//   * Each window's pixmap is read back with a plain XGetImage every frame,
//     which is a round trip per window and will not scale well with many
//     windows open. The real fix is either XShmGetImage per pixmap, or
//     (better) binding each pixmap directly to a GL texture with
//     GLX_EXT_texture_from_pixmap so there's no CPU copy at all
//   * Compositing (alpha blending, occlusion) is done on the CPU in a
//     scratch buffer, then uploaded as one texture. Fine for a first pass,
//     but this is the next thing to move to the GPU if it's too slow
//
// TODO: Wayland / Windows support

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xcomposite.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    Window win;
    Pixmap pixmap;
    int x, y;        // top-left, in root coordinates
    int width, height;
    int depth;        // 24 = opaque (no alpha channel), 32 = has alpha
} CapturedWindow;

struct Capturer {
    Display* display;
    Window root;
    Window own_window;
    int width;
    int height;

    CapturedWindow* windows;
    int window_count;

    unsigned char* compose_buffer; // RGBA8 scratch, width*height*4

    Texture* texture;
};

static void capturer_enumerate_windows(Capturer* c) {
    Window root_return, parent_return;
    Window* children = NULL;
    unsigned int nchildren = 0;

    if (!XQueryTree(c->display, c->root, &root_return, &parent_return, &children, &nchildren)) {
        fprintf(stderr, "capturer: XQueryTree failed\n");
        return;
    }

    c->windows = calloc(nchildren, sizeof(CapturedWindow));
    c->window_count = 0;

    for (unsigned int i = 0; i < nchildren; i++) {
        Window win = children[i];
        if (win == c->own_window) continue;

        XWindowAttributes attrs;
        if (!XGetWindowAttributes(c->display, win, &attrs)) continue;
        if (attrs.map_state != IsViewable) continue;
        if (attrs.class == InputOnly) continue;

        int abs_x, abs_y;
        Window child_return;
        if (!XTranslateCoordinates(c->display, win, c->root, 0, 0, &abs_x, &abs_y, &child_return)) {
            continue;
        }

        XCompositeRedirectWindow(c->display, win, CompositeRedirectAutomatic);
        Pixmap pixmap = XCompositeNameWindowPixmap(c->display, win);
        if (!pixmap) continue;

        CapturedWindow* cw = &c->windows[c->window_count++];
        cw->win = win;
        cw->pixmap = pixmap;
        cw->x = abs_x;
        cw->y = abs_y;
        cw->width = attrs.width;
        cw->height = attrs.height;
        cw->depth = attrs.depth;
    }

    if (children) XFree(children);
}

Capturer* capturer_create(int width, int height, unsigned long own_id) {
    Capturer* c = calloc(1, sizeof(Capturer));
    if (!c) return NULL;

    c->width = width;
    c->height = height;
    c->own_window = (Window)own_id;

    c->display = XOpenDisplay(NULL);
    if (!c->display) {
        fprintf(stderr, "capturer: failed to open X display\n");
        free(c);
        return NULL;
    }

    int composite_event_base, composite_error_base;
    if (!XCompositeQueryExtension(c->display, &composite_event_base, &composite_error_base)) {
        fprintf(stderr, "capturer: Composite extension not available on this X server\n");
        XCloseDisplay(c->display);
        free(c);
        return NULL;
    }

    int major = 0, minor = 0;
    XCompositeQueryVersion(c->display, &major, &minor);
    if (major == 0 && minor < 2) {
        fprintf(stderr, "capturer: Composite extension too old (need >= 0.2 for NameWindowPixmap)\n");
        XCloseDisplay(c->display);
        free(c);
        return NULL;
    }

    int screen = DefaultScreen(c->display);
    c->root = RootWindow(c->display, screen);

    if (c->own_window == 0) {
        fprintf(stderr, "capturer: warning: no own_window_id given, our own window won't be excluded\n");
    }

    capturer_enumerate_windows(c);

    c->compose_buffer = calloc((size_t)width * height, 4);
    if (!c->compose_buffer) {
        fprintf(stderr, "capturer: failed to allocate scratch buffer\n");
        free(c->windows);
        XCloseDisplay(c->display);
        free(c);
        return NULL;
    }

    c->texture = texture_create(width, height, TEXTURE_FORMAT_BGRA8);
    if (!c->texture) {
        free(c->compose_buffer);
        free(c->windows);
        XCloseDisplay(c->display);
        free(c);
        return NULL;
    }

    return c;
}

static void blit_window(Capturer* c, const CapturedWindow* cw, const unsigned char* src) {
    for (int row = 0; row < cw->height; row++) {
        int dst_row = cw->y + row;
        if (dst_row < 0 || dst_row >= c->height) continue;

        for (int col = 0; col < cw->width; col++) {
            int dst_col = cw->x + col;
            if (dst_col < 0 || dst_col >= c->width) continue;

            const unsigned char* s = src + ((size_t)row * cw->width + col) * 4;
            unsigned char* d = c->compose_buffer + ((size_t)dst_row * c->width + dst_col) * 4;

            if (cw->depth != 32) {
                d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = 255;
                continue;
            }

            unsigned int src_a = s[3];
            unsigned int inv_a = 255 - src_a;
            d[0] = (unsigned char)((s[0] * src_a + d[0] * inv_a) / 255);
            d[1] = (unsigned char)((s[1] * src_a + d[1] * inv_a) / 255);
            d[2] = (unsigned char)((s[2] * src_a + d[2] * inv_a) / 255);
            d[3] = (unsigned char)(src_a + (d[3] * inv_a) / 255);
        }
    }
}

void capturer_capture(Capturer* capturer) {
    if (!capturer) return;

    memset(capturer->compose_buffer, 0, (size_t)capturer->width * capturer->height * 4);

    for (int i = 0; i < capturer->window_count; i++) {
        CapturedWindow* cw = &capturer->windows[i];

        XImage* image = XGetImage(capturer->display, cw->pixmap, 0, 0,
                                   cw->width, cw->height, AllPlanes, ZPixmap);
        if (!image) continue; // window may have gone away since enumeration

        blit_window(capturer, cw, (const unsigned char*)image->data);

        XDestroyImage(image);
    }

    texture_update(capturer->texture, capturer->compose_buffer);
}

Texture* capturer_texture(Capturer* capturer) {
    return capturer ? capturer->texture : NULL;
}

void capturer_destroy(Capturer* capturer) {
    if (!capturer) return;

    if (capturer->texture) texture_destroy(capturer->texture);

    for (int i = 0; i < capturer->window_count; i++) {
        if (capturer->windows[i].pixmap) {
            XFreePixmap(capturer->display, capturer->windows[i].pixmap);
        }
        XCompositeUnredirectWindow(capturer->display, capturer->windows[i].win, CompositeRedirectAutomatic);
    }
    free(capturer->windows);
    free(capturer->compose_buffer);

    if (capturer->display) {
        XCloseDisplay(capturer->display);
    }

    free(capturer);
}