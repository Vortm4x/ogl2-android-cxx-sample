#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include <android/log.h>
#include <android_native_app_glue.h>
#include <android/asset_manager.h>
#include <ndk_helper/vecmath.h>
#include <ndk_helper/shader.h>
#include <ndk_helper/gestureDetector.h>
#include <cmath>
#include <vector>
#define LOG_TAG ("mandelbrot")

using namespace ndk_helper;
using namespace shader;

//#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__))
//#define LOGD(...) ((void)__android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__))
//#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN,  LOG_TAG, __VA_ARGS__))
//#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__))

struct saved_state {
    Vec2 offset;
    GLfloat zoom;
    GLint width;
    GLint height;
};

struct graphics_engine
{
    android_app* app;
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    EGLConfig config;
    GLuint program;
    bool isAnimating;
    saved_state state;
    PinchDetector pinchDetector;
    DragDetector dragDetector;

};


void engine_init_display(graphics_engine* engine);
void engine_init_program(graphics_engine* engine);
EGLBoolean engine_draw_frame(graphics_engine* engine);
void engine_term_display(graphics_engine* engine);
void on_application_command(android_app* app, int32_t cmd);
int32_t on_input_event(struct android_app* app, AInputEvent* event);

void engine_init_display(graphics_engine* engine)
{
    const EGLint attribs[] = {
            EGL_BLUE_SIZE,       8,
            EGL_GREEN_SIZE,      8,
            EGL_RED_SIZE,        8,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_NONE
    };
    const EGLint contextAttribs[] = {
            EGL_CONTEXT_CLIENT_VERSION, 2,
            EGL_NONE
    };

    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display, nullptr, nullptr);

    EGLConfig config = {};
    EGLint numConfigs = {};
    eglChooseConfig(display, attribs, &config, 1, &numConfigs);

    EGLint format = {};
    eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);
    ANativeWindow_setBuffersGeometry(engine->app->window, 0, 0, format);

    EGLSurface surface = eglCreateWindowSurface(display, config, engine->app->window, nullptr);
    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs);

    eglMakeCurrent(display, surface, surface, context);

    engine->display = display;
    engine->surface = surface;
    engine->context = context;
    engine->config = config;

    EGLint width, height;
    eglQuerySurface(display, surface, EGL_WIDTH,  &width);
    eglQuerySurface(display, surface, EGL_HEIGHT, &height);

    saved_state* state = &engine->state;
    state->width = width;
    state->height = height;

    glViewport(0, 0, width, height);
}

void engine_init_program(graphics_engine* engine)
{
    AAssetManager* assetManager = engine->app->activity->assetManager;
    AAsset* vertexShaderAsset = AAssetManager_open(assetManager,  "shaders/shader.vert", AASSET_MODE_BUFFER);
    AAsset* fragmentShaderAsset = AAssetManager_open(assetManager,  "shaders/shader.frag", AASSET_MODE_BUFFER);

    GLuint vertexShader = {}, fragmentShader = {};

    const char* vertexShaderSource = (const char*)AAsset_getBuffer(vertexShaderAsset);
    const char* fragmentShaderSource = (const char*)AAsset_getBuffer(fragmentShaderAsset);

    auto vertexShaderSize = static_cast<int32_t>(AAsset_getLength(vertexShaderAsset));
    auto fragmentShaderSize = static_cast<int32_t>(AAsset_getLength(fragmentShaderAsset));

    CompileShader(&vertexShader, GL_VERTEX_SHADER, vertexShaderSource, vertexShaderSize);
    CompileShader(&fragmentShader, GL_FRAGMENT_SHADER, fragmentShaderSource, fragmentShaderSize);

    engine->program = glCreateProgram();
    glAttachShader(engine->program, vertexShader);
    glAttachShader(engine->program, fragmentShader);
    LinkProgram(engine->program);
    ValidateProgram( engine->program);

    AAsset_close(vertexShaderAsset);
    AAsset_close(fragmentShaderAsset);
}

EGLBoolean engine_draw_frame(graphics_engine* engine)
{
    const GLfloat vertices[] = {
        -1.0f, -1.0f, 0.0f,
        1.0f, -1.0f, 0.0f,
        -1.0f, 1.0f, 0.0f,
        -1.0f, 1.0f, 0.0f,
        1.0f, -1.0f, 0.0f,
        1.0f, 1.0f, 0.0f,
    };

    const GLfloat colorMask[] = {
        1.0f, .27f, 0.24f,
        1.0f, 0.65f, 0.14f,
        1.0f, 0.96f, 0.18f,
        0.43f, 1.0f, 0.31f,
        0.27f, 0.71f, 1.0f,
        0.39f, 0.4f, 1.0f,
        0.61f, 0.33f, 1.0f,
    };

    const GLint maxIterations = 255;

    glUseProgram(engine->program);

    //VBO setup (vertex buffer objects)
    GLuint vertexBuffer = {};
    glGenBuffers(1, &vertexBuffer);

    //send data to GPU
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    GLuint positionHandle = glGetAttribLocation(engine->program, "inPosition");
    glEnableVertexAttribArray(positionHandle);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glVertexAttribPointer(positionHandle, 3, GL_FLOAT, GL_FALSE, 0, nullptr);


    //uniforms
    GLint colorMaskHandle = glGetUniformLocation(engine->program, "colorMask");
    glUniform3fv(colorMaskHandle, 7, colorMask);

    GLint screenResolutionHandle = glGetUniformLocation(engine->program, "screenResolution");
    glUniform2f(screenResolutionHandle, GLfloat(engine->state.width), GLfloat(engine->state.height));

    GLfloat offsetX, offsetY;
    engine->state.offset.Value(offsetX, offsetY);

    GLint offsetHandle = glGetUniformLocation(engine->program, "offset");
    glUniform2f(offsetHandle, offsetX,offsetY);

    GLint zoomHandle = glGetUniformLocation(engine->program, "zoom");
    glUniform1f(zoomHandle, engine->state.zoom);

    GLint maxIterationsHandle = glGetUniformLocation(engine->program, "maxIterations");
    glUniform1i(maxIterationsHandle, maxIterations);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    return eglSwapBuffers(engine->display, engine->surface);
}

void engine_term_display(graphics_engine* engine)
{
    if (engine->display != EGL_NO_DISPLAY)
    {
        eglMakeCurrent(engine->display, EGL_NO_SURFACE, EGL_NO_SURFACE,EGL_NO_CONTEXT);

        if (engine->context != EGL_NO_CONTEXT)
        {
            eglDestroyContext(engine->display, engine->context);
        }

        if (engine->surface != EGL_NO_SURFACE)
        {
            eglDestroySurface(engine->display, engine->surface);
        }

        eglTerminate(engine->display);
    }

    engine->program = {};
    engine->display = EGL_NO_DISPLAY;
    engine->context = EGL_NO_CONTEXT;
    engine->surface = EGL_NO_SURFACE;
}

void on_application_command(android_app* app, int32_t cmd) {
    auto* engine = static_cast<graphics_engine*>(app->userData);

    switch (cmd)
    {
        case APP_CMD_INIT_WINDOW:
            //isn't called on orientation change - be careful
            //
            if (engine->app->window != nullptr)
            {
                engine_init_display(engine);
                engine_init_program(engine);
                engine->dragDetector.SetConfiguration(engine->app->config);
                engine->pinchDetector.SetConfiguration(engine->app->config);
            }
            break;
        case APP_CMD_TERM_WINDOW:
            // The window is being hidden or closed, clean it up.
            engine_term_display(engine);
            break;
        case APP_CMD_SAVE_STATE:
            // save application state - called before window is hidden (activity will be destroyed)
            engine->app->savedState = (saved_state*) malloc(sizeof(saved_state));
            *(saved_state*)engine->app->savedState = engine->state;
            engine->app->savedStateSize = sizeof(struct saved_state);
            break;
        case APP_CMD_CONFIG_CHANGED:
            //track screen|orientation a.k.a config change and recreate surface
            engine->isAnimating = false;

            eglMakeCurrent(engine->display, EGL_NO_SURFACE, EGL_NO_SURFACE,EGL_NO_CONTEXT);

            if (engine->surface != EGL_NO_SURFACE)
            {
                eglDestroySurface(engine->display, engine->surface);
            }

            engine->surface = eglCreateWindowSurface(engine->display, engine->config, engine->app->window, nullptr);
            eglMakeCurrent(engine->display, engine->surface, engine->surface, engine->context);

            EGLint width, height;
            eglQuerySurface(engine->display, engine->surface, EGL_WIDTH,  &width);
            eglQuerySurface(engine->display, engine->surface, EGL_HEIGHT, &height);

            glViewport(0, 0, width, height);

            engine->state.width = width;
            engine->state.height = height;

            engine->dragDetector.SetConfiguration(engine->app->config);
            engine->pinchDetector.SetConfiguration(engine->app->config);

            engine->isAnimating = true;
            break;
        case APP_CMD_GAINED_FOCUS:
            //the app's activity window has gained input focus (none of devices are monitoring at the moment)
            //also called on orientation/screen change (LOST_FOCUS isn't sent in this case)
            engine->isAnimating = true;
            break;
        case APP_CMD_LOST_FOCUS:
            // the app's activity window has lost input (none of devices are monitoring at the moment)
            engine->isAnimating = false;

            break;
        default:
            break;
    }
}


void androidToGL(saved_state* state, Vec2 &v)
{
    float x, y;
    v.Value(x, y);
    v = Vec2(state->width - x, y);
}

int32_t on_input_event(struct android_app* app, AInputEvent* event)
{
    auto *engine = static_cast<graphics_engine*>(app->userData);
    saved_state* state = &engine->state;

    int32_t eventType =  AInputEvent_getType(event);

    if(eventType == AINPUT_EVENT_TYPE_MOTION)
    {
        GESTURE_STATE gestureState = GESTURE_STATE_NONE;

        DragDetector* dragDetector = &engine->dragDetector;
        PinchDetector* pinchDetector = &engine->pinchDetector;

        static Vec2 lastDragPos;
        gestureState = dragDetector->Detect(event);

        switch (gestureState)
        {
            case GESTURE_STATE_START: {
                dragDetector->GetPointer(lastDragPos);
                androidToGL(state, lastDragPos);

                break;
            }
            case GESTURE_STATE_MOVE: {
                Vec2 newDragPos;

                dragDetector->GetPointer(newDragPos);
                androidToGL(state, newDragPos);

                state->offset += (newDragPos - lastDragPos) / state->zoom;

                lastDragPos = newDragPos;

                break;
            }
            case GESTURE_STATE_NONE: {
                lastDragPos = Vec2();
                break;
            }
        }

        static Vec2 lastPinchPointerA, lastPinchPointerB;

        static const GLfloat zoomSpeed = 1.0f;
        static const GLfloat minZoom = 0.25f;

        gestureState = pinchDetector->Detect(event);

        switch (gestureState)
        {
            case GESTURE_STATE_START:
            {
                pinchDetector->GetPointers(lastPinchPointerA, lastPinchPointerB);
                androidToGL(state, lastPinchPointerA);
                androidToGL(state, lastPinchPointerB);
                break;
            }
            case GESTURE_STATE_MOVE: {
                Vec2 newPinchPointerA, newPinchPointerB;

                pinchDetector->GetPointers(newPinchPointerA, newPinchPointerB);
                androidToGL(state, newPinchPointerA);
                androidToGL(state, newPinchPointerB);

                GLfloat lastZoomDist = (lastPinchPointerA - lastPinchPointerB).Length();
                GLfloat newZoomDist = (newPinchPointerA - newPinchPointerB).Length();
                GLfloat scaleFactor = pow(newZoomDist / lastZoomDist, zoomSpeed);

                //zoom center if (0,0) - view center
                Vec2 resolution(GLfloat(state->width), GLfloat(state->height));
                Vec2 lastZoomCenter = (lastPinchPointerA + lastPinchPointerB - resolution) / 2.0f;
                Vec2 newZoomCenter = (newPinchPointerA + newPinchPointerB - resolution) / 2.0f;

                Vec2 lastWorldZoomCenter = (lastZoomCenter / state->zoom) + state->offset;

                state->zoom *= scaleFactor;
                if(state->zoom < minZoom) state->zoom = minZoom;

                Vec2 newWorldZoomCenter = (newZoomCenter / state->zoom) + state->offset;

                state->offset += newWorldZoomCenter - lastWorldZoomCenter;

                lastPinchPointerA = newPinchPointerA;
                lastPinchPointerB = newPinchPointerB;
                break;
            }
            case GESTURE_STATE_NONE: {
                lastPinchPointerA = Vec2();
                lastPinchPointerB = Vec2();
                break;
            }
        }
    }

    return 1;
}

void android_main(android_app* app)
{
    graphics_engine engine = {};

    app->userData = &engine;
    app->onAppCmd = on_application_command;
    app->onInputEvent = on_input_event;

    engine.app = app;
    engine.state.zoom = 1.0f;

    if (app->savedState != nullptr)
    {
        engine.state = *(struct saved_state*)app->savedState;
    }

    while (true)
    {
        int events = 0;
        android_poll_source *source = nullptr;

        while (ALooper_pollOnce(engine.isAnimating ? 0 : -1, nullptr, &events, (void **) &source) >= 0)
        {
            if (source != nullptr)
            {
                source->process(app, source);
            }

            if (app->destroyRequested != 0)
            {
                engine.isAnimating = false;
                engine_term_display(&engine);
                return;
            }
        }

        if (engine.isAnimating)
        {
            if(engine_draw_frame(&engine) == EGL_TRUE)
            {
//                engine.state.angle += 0.01f;
            }
        }
    }
}