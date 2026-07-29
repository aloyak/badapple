# Bad Apple!!

<p align="center">
  <img src="./preview.png" alt="Bad Apple" width="600"/>
</p>

## Overview

This project started as a simple experiment to create a version of the "Bad Apple" videoclip running on top of the screen in real time.

Interestingly, this same code base happened to have some potential to be used in more general scenarios. It can be used as a postprocessing layer 
that runs on top of the screen and thus can be used to apply any effects to the screen buffer in real time with the huge power and flexibility of
OpenGL's shaders.

## Supported Platforms

This project is currently only supported on **Linux** with **X11**. Expanding to other platforms is the current priority.

## Building

This project uses CMake as a build system. Requires **CMake 4.0 or higher**.

Note that all dependencies are included with CMake's FetchContent module

```sh

git clone https://github.com/aloyak/badapple.git
cd badapple
chmod +x build.sh
./build.sh

```

## Contributing

Very much welcome!