set(NDK_ANDROID_DIR         ${ANDROID_NDK}/sources/android)
set(NDK_HELPER_DIR          ${NDK_ANDROID_DIR}/ndk_helper)
set(NDK_NATIVE_APP_GLUE_DIR ${NDK_ANDROID_DIR}/native_app_glue)


add_library(ndk_helper STATIC
    ${NDK_HELPER_DIR}/vecmath.cpp
    ${NDK_HELPER_DIR}/shader.cpp
    ${NDK_HELPER_DIR}/gestureDetector.cpp
    ${NDK_HELPER_DIR}/JNIHelper.cpp
    ${NDK_HELPER_DIR}/interpolator.cpp
    ${NDK_HELPER_DIR}/tapCamera.cpp
    ${NDK_HELPER_DIR}/perfMonitor.cpp
    ${NDK_HELPER_DIR}/GLContext.cpp
    ${NDK_HELPER_DIR}/gl3stub.c
)

target_include_directories(ndk_helper PUBLIC
    ${NDK_HELPER_DIR}
    ${NDK_NATIVE_APP_GLUE_DIR}
)


add_library(native_app_glue STATIC
    ${NDK_NATIVE_APP_GLUE_DIR}/android_native_app_glue.c
)

target_include_directories(native_app_glue PUBLIC
    ${NDK_NATIVE_APP_GLUE_DIR}
)

# Export ANativeActivity_onCreate(),
# Refer to: https://github.com/android-ndk/ndk/issues/381.
set(CMAKE_SHARED_LINKER_FLAGS
    "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate"
)
