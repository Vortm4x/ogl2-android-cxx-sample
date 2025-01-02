cmake_minimum_required(VERSION 3.21)

project(mandelbrot LANGUAGES C CXX)


include(android_ndk.cmake)

add_library(${PROJECT_NAME} SHARED
    main.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    android
    native_app_glue
    ndk_helper
    EGL
    GLESv2
    log
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${NDK_ANDROID_DIR}
)
