#include "capturer.h"

// X11-only screen capture, using the MIT-SHM extension for fast readback.
// TODO: Wayland / Windows support
 
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdlib.h>
#include <stdio.h>

struct Capturer {
    Display* display;
    Window root;
    int width;
    int height;

    XShmSegmentInfo shm_info;
    XImage* image;

    Texture* texture;
};

// DEBUG:
static int shm_error_handler(Display* d, XErrorEvent* e) {
    char buf[256];
    XGetErrorText(d, e->error_code, buf, sizeof(buf));
    fprintf(stderr, "X error: %s (request %d.%d, resource 0x%lx)\n",
            buf, e->request_code, e->minor_code, e->resourceid);
    return 0;
}

Capturer* capturer_create(int width, int height) {
    Capturer* c = calloc(1, sizeof(Capturer));
    if (!c) return NULL;

    c->width = width;
    c->height = height;

    c->display = XOpenDisplay(NULL);
    if (!c->display) {
        fprintf(stderr, "capturer: failed to open X display");
        free(c);
        return NULL;
    }

    if (!XShmQueryExtension(c->display)) {
        fprintf(stderr, "capturer: XShm extension not available on this X server\n");
        XCloseDisplay(c->display);
        free(c);
        return NULL;
    }

    int screen = DefaultScreen(c->display);
    c->root = RootWindow(c->display, screen);
    Visual* visual = DefaultVisual(c->display, screen);
    int depth = DefaultDepth(c->display, screen);

    c->image = XShmCreateImage(c->display, visual, depth, ZPixmap, NULL, 
                               &c->shm_info, width, height);
    if (!c->image) {
        fprintf(stderr, "capturer: XShmCreateImage failed\n");
        XCloseDisplay(c->display);
        free(c);
        return NULL;
    }
    

    c->shm_info.shmid = shmget(IPC_PRIVATE, 
                        (size_t)c->image->bytes_per_line * c->image->height, IPC_CREAT | 0600);
    if (c->shm_info.shmid < 0) {
        fprintf(stderr, "capturer: shmget failed\n");
        XDestroyImage(c->image);
        XCloseDisplay(c->display);
        free(c);
        return NULL;
    }

    c->shm_info.shmaddr = c->image->data = shmat(c->shm_info.shmid, NULL, 0);
    c->shm_info.readOnly = False;
 
    //XSetErrorHandler(shm_error_handler);
    if (!XShmAttach(c->display, &c->shm_info)) {
        fprintf(stderr, "capturer: XShmAttach failed\n");
        shmdt(c->shm_info.shmaddr);
        shmctl(c->shm_info.shmid, IPC_RMID, NULL);
        XDestroyImage(c->image);
        XCloseDisplay(c->display);
        free(c);
        return NULL;
    }
 
    shmctl(c->shm_info.shmid, IPC_RMID, NULL);
 
    c->texture = texture_create(width, height, TEXTURE_FORMAT_BGRA8);
    if (!c->texture) {
        XShmDetach(c->display, &c->shm_info);
        shmdt(c->shm_info.shmaddr);
        XDestroyImage(c->image);
        XCloseDisplay(c->display);
        free(c);
        return NULL;
    }
 
    return c;
}

void capturer_capture(Capturer* capturer) {
    if (!capturer) return;
    
    if (!XShmGetImage(capturer->display, capturer->root, capturer->image, 0, 0, AllPlanes)) {
        fprintf(stderr, "capturer: XShmGetImage failed\n");
        return;
    }

    texture_update(capturer->texture, capturer->image->data);
}

Texture* capturer_texture(Capturer* capturer) {
    return capturer ? capturer->texture : NULL;
}

void capturer_destroy(Capturer* capturer) {
    if (!capturer) return;

    if (capturer->texture) texture_destroy(capturer->texture);

    if (capturer->display) {
        if (capturer->shm_info.shmaddr) {
            XShmDetach(capturer->display, &capturer->shm_info);
            shmdt(capturer->shm_info.shmaddr);
        }
        if (capturer->image) XDestroyImage(capturer->image);
        XCloseDisplay(capturer->display);
    }

    free(capturer);
}