cmake_minimum_required(VERSION 4.0)

project(BadApple
    VERSION 0.1
    LANGUAGES C
)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

include(cmake/sdl3.cmake)
include(cmake/glad.cmake)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
    libavformat
    libavcodec
    libavutil
    libswscale
)
find_package(X11 REQUIRED)

add_executable(BadApple
    src/main.c
    src/window.c
    src/video.c
    src/texture.c
    src/shader.c
    src/quad.c
    src/capturer.c
)

target_include_directories(BadApple PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(BadApple PRIVATE 
    SDL3::SDL3 glad_gl_core_33 PkgConfig::FFMPEG 
    X11::X11 X11::Xext X11::Xcomposite X11::Xdamage
)

# Copy the video file to the build directory
add_custom_command(TARGET BadApple POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_CURRENT_SOURCE_DIR}/video/badapple.mp4
    $<TARGET_FILE_DIR:BadApple>/video/badapple.mp4
)

# Copy all shader sources to the build dir
add_custom_command(TARGET BadApple POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders
    $<TARGET_FILE_DIR:BadApple>/shaders
)