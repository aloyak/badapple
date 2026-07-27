cmake_minimum_required(VERSION 4.0)

project(BadApple
    VERSION 0.1
    LANGUAGES C
)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

include(cmake/glfw.cmake)
include(cmake/glad.cmake)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
    libavformat
    libavcodec
    libavutil
    libswscale
)

add_executable(BadApple
    src/main.c
    src/window.c
    src/video.c
)

target_include_directories(BadApple PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(BadApple PRIVATE glfw glad_gl_core_33 PkgConfig::FFMPEG)


# Copy the video file to the build directory
add_custom_command(TARGET BadApple POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_CURRENT_SOURCE_DIR}/video/badapple.mp4
    $<TARGET_FILE_DIR:BadApple>
)