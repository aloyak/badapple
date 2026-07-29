#!/bin/sh

echo "Building Project..."
mkdir build && cd build
cmake .. 
make
echo "Build Complete!"