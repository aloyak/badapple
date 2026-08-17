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
//   * Each window's pixmap is read back via MIT-SHM when available (falls
//     back to plain XGetImage otherwise), and only for windows that XDamage
//     reports as having actually changed since the last frame. Compositing
//     is still a full recomposite every frame, since a changed window can
//     affect what's visible through any window stacked above it.
//   * Compositing (alpha blending, occlusion) is done on the CPU in a
//     scratch buffer, then uploaded as one texture. Fine for a first pass,
//     but this is the next thing to move to the GPU if it's too slow
//     (GLX_EXT_texture_from_pixmap would let us skip the CPU copy entirely)
//
// TODO: Wayland / Windows support

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xdamage.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// from window.c
extern Window window_resolve_toplevel(Display* display, Window root, Window win);

typedef struct {
    Window win;
    Pixmap pixmap;
    int x, y;        // top-left, in root coordinates
    int width, height;
    int depth;        // 24 = opaque (no alpha channel), 32 = has alpha

    XImage* ximage;           // backing store for this window's pixels
    XShmSegmentInfo shminfo;  // only valid if use_shm
    int use_shm;

    Damage damage;    // XDamage handle, or None if the extension is unavailable
    int dirty;         // needs a fresh XShmGetImage/XGetImage this frame
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

    int has_shm;
    int damage_event_base; // 0 if XDamage isn't available
};

// Very fast, exact integer approximation of round(a * b / 255.0) for a,b in [0,255].
static inline unsigned char mul255(unsigned int a, unsigned int b) {
    unsigned int t = a * b + 128;
    return (unsigned char)(((t >> 8) + t) >> 8);
}

static Visual* visual_for_depth(Display* display, int screen, int depth) {
    XVisualInfo vinfo;
    if (XMatchVisualInfo(display, screen, depth, TrueColor, &vinfo)) {
        return vinfo.visual;
    }
    return DefaultVisual(display, screen);
}

static int create_shm_image(Capturer* c, CapturedWindow* cw, int screen) {
    Visual* visual = visual_for_depth(c->display, screen, cw->depth);

    cw->ximage = XShmCreateImage(c->display, visual, cw->depth, ZPixmap, NULL,
                                  &cw->shminfo, cw->width, cw->height);
    if (!cw->ximage) return 0;

    cw->shminfo.shmid = shmget(IPC_PRIVATE,
                                (size_t)cw->ximage->bytes_per_line * cw->ximage->height,
                                IPC_CREAT | 0600);
    if (cw->shminfo.shmid < 0) {
        XDestroyImage(cw->ximage);
        cw->ximage = NULL;
        return 0;
    }

    cw->shminfo.shmaddr = cw->ximage->data = shmat(cw->shminfo.shmid, NULL, 0);
    cw->shminfo.readOnly = False;

    if (!XShmAttach(c->display, &cw->shminfo)) {
        shmdt(cw->shminfo.shmaddr);
        shmctl(cw->shminfo.shmid, IPC_RMID, NULL);
        XDestroyImage(cw->ximage);
        cw->ximage = NULL;
        return 0;
    }

    // Mark the segment for removal now. It stays alive until every attached
    // process (us and the X server) detaches it, so we don't have to
    // remember to shmctl(IPC_RMID) again in capturer_destroy.
    shmctl(cw->shminfo.shmid, IPC_RMID, NULL);

    return 1;
}

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

    int screen = DefaultScreen(c->display);

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
        cw->dirty = 1; // force a fetch on the first frame

        cw->use_shm = c->has_shm && create_shm_image(c, cw, screen);
        if (!cw->use_shm) {
            fprintf(stderr, "capturer: falling back to XGetImage for window 0x%lx\n",
                    (unsigned long)win);
        }

        if (c->damage_event_base) {
            cw->damage = XDamageCreate(c->display, win, XDamageReportNonEmpty);
        }
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

    int shm_major, shm_minor;
    Bool shm_pixmaps;
    c->has_shm = XShmQueryVersion(c->display, &shm_major, &shm_minor, &shm_pixmaps)
                 && XShmQueryExtension(c->display);
    if (!c->has_shm) {
        fprintf(stderr, "capturer: MIT-SHM not available, falling back to XGetImage (slower)\n");
    }

    int damage_error_base;
    if (!XDamageQueryExtension(c->display, &c->damage_event_base, &damage_error_base)) {
        fprintf(stderr, "capturer: XDamage not available, will re-fetch every window every frame\n");
        c->damage_event_base = 0;
    }

    int screen = DefaultScreen(c->display);
    c->root = RootWindow(c->display, screen);

    if (c->own_window == 0) {
        fprintf(stderr, "capturer: warning: no own_window_id given, our own window won't be excluded\n");
    } else {
        Window resolved = window_resolve_toplevel(c->display, c->root, c->own_window);
        if (resolved != c->own_window) {
            fprintf(stderr, "capturer: own window is reparented (0x%lx -> 0x%lx), excluding the top-level ancestor\n",
            (unsigned long)c->own_window, (unsigned long)resolved);
        }
        c->own_window = resolved;
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

// Opaque windows (depth 24): plain copy, alpha forced to 255. Bounds are
// clipped once per row rather than checked per pixel.
static void blit_window_opaque(Capturer* c, const CapturedWindow* cw, const unsigned char* src) {
    for (int row = 0; row < cw->height; row++) {
        int dst_row = cw->y + row;
        if (dst_row < 0 || dst_row >= c->height) continue;

        int col_start = cw->x < 0 ? -cw->x : 0;
        int col_end = cw->width;
        if (cw->x + col_end > c->width) col_end = c->width - cw->x;
        if (col_end <= col_start) continue;

        const unsigned char* s = src + ((size_t)row * cw->width + col_start) * 4;
        unsigned char* d = c->compose_buffer + ((size_t)dst_row * c->width + (cw->x + col_start)) * 4;

        for (int col = col_start; col < col_end; col++, s += 4, d += 4) {
            d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = 255;
        }
    }
}

// Windows with an alpha channel (depth 32): source-over blend.
static void blit_window_alpha(Capturer* c, const CapturedWindow* cw, const unsigned char* src) {
    for (int row = 0; row < cw->height; row++) {
        int dst_row = cw->y + row;
        if (dst_row < 0 || dst_row >= c->height) continue;

        int col_start = cw->x < 0 ? -cw->x : 0;
        int col_end = cw->width;
        if (cw->x + col_end > c->width) col_end = c->width - cw->x;
        if (col_end <= col_start) continue;

        const unsigned char* s = src + ((size_t)row * cw->width + col_start) * 4;
        unsigned char* d = c->compose_buffer + ((size_t)dst_row * c->width + (cw->x + col_start)) * 4;

        for (int col = col_start; col < col_end; col++, s += 4, d += 4) {
            unsigned int src_a = s[3];
            unsigned int inv_a = 255 - src_a;
            d[0] = mul255(s[0], src_a) + mul255(d[0], inv_a);
            d[1] = mul255(s[1], src_a) + mul255(d[1], inv_a);
            d[2] = mul255(s[2], src_a) + mul255(d[2], inv_a);
            d[3] = (unsigned char)(src_a + mul255(d[3], inv_a));
        }
    }
}

void capturer_capture(Capturer* capturer) {
    if (!capturer) return;

    // Drain pending damage events and mark the affected windows dirty.
    // XDamageSubtract resets the window's damage region so it can accumulate
    // fresh damage for the next frame.
    if (capturer->damage_event_base) {
        XEvent ev;
        while (XCheckTypedEvent(capturer->display, capturer->damage_event_base + XDamageNotify, &ev)) {
            XDamageNotifyEvent* dev = (XDamageNotifyEvent*)&ev;
            for (int i = 0; i < capturer->window_count; i++) {
                if (capturer->windows[i].damage == dev->damage) {
                    capturer->windows[i].dirty = 1;
                    XDamageSubtract(capturer->display, dev->damage, None, None);
                    break;
                }
            }
        }
    }

    // Only pay the round trip for windows that actually changed (or, with no
    // XDamage support, every window every frame).
    for (int i = 0; i < capturer->window_count; i++) {
        CapturedWindow* cw = &capturer->windows[i];
        if (!cw->dirty) continue;

        if (cw->use_shm) {
            if (!XShmGetImage(capturer->display, cw->pixmap, cw->ximage, 0, 0, AllPlanes)) {
                continue; // window may have gone away since enumeration
            }
        } else {
            XImage* image = XGetImage(capturer->display, cw->pixmap, 0, 0,
                                       cw->width, cw->height, AllPlanes, ZPixmap);
            if (!image) continue;
            if (cw->ximage) XDestroyImage(cw->ximage);
            cw->ximage = image; // cached until the next dirty frame
        }

        cw->dirty = 0;
    }

    // Windows can overlap, so a change in one can affect what shows through
    // any window stacked above it. Recomposite everything from the
    // (possibly cached, possibly freshly-fetched) per-window pixels rather
    // than trying to patch in just the changed rectangles.
    memset(capturer->compose_buffer, 0, (size_t)capturer->width * capturer->height * 4);

    for (int i = 0; i < capturer->window_count; i++) {
        CapturedWindow* cw = &capturer->windows[i];
        if (!cw->ximage) continue;

        if (cw->depth != 32) {
            blit_window_opaque(capturer, cw, (const unsigned char*)cw->ximage->data);
        } else {
            blit_window_alpha(capturer, cw, (const unsigned char*)cw->ximage->data);
        }
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
        CapturedWindow* cw = &capturer->windows[i];

        if (cw->damage) {
            XDamageDestroy(capturer->display, cw->damage);
        }

        if (cw->ximage) {
            if (cw->use_shm) {
                XShmDetach(capturer->display, &cw->shminfo);
                shmdt(cw->shminfo.shmaddr);
            }
            XDestroyImage(cw->ximage);
        }

        if (cw->pixmap) {
            XFreePixmap(capturer->display, cw->pixmap);
        }
        XCompositeUnredirectWindow(capturer->display, cw->win, CompositeRedirectAutomatic);
    }
    free(capturer->windows);
    free(capturer->compose_buffer);

    if (capturer->display) {
        XCloseDisplay(capturer->display);
    }

    free(capturer);
}