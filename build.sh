#!/bin/sh

BACKEND="x11"

while [ "$#" -gt 0 ]; do
    case "$1" in
        --x11)
            BACKEND="x11"
            ;;
        --wayland)
            BACKEND="wayland"
            ;;
        *)
            echo "Unknown argument: $1"
            echo "Usage: $0 [--x11 | --wayland]"
            exit 1
            ;;
    esac
    shift
done

if [ "$BACKEND" = "wayland" ]; then
    USE_WAYLAND="ON"
else
    USE_WAYLAND="OFF"
fi

echo "Building Project (backend: $BACKEND)..."
mkdir -p build && cd build
cmake4 -DUSE_WAYLAND="$USE_WAYLAND" ..
make
echo "Build Complete!"