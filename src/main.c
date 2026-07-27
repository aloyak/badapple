#include "window.h"

int main() {
    Window* window = window_create("Bad Apple");

    while (window_running(window)) {
        window_clear(window);
    }
    
    window_destroy(window);

    return 0;
}