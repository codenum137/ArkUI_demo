//
// Created on 2025/7/10.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#ifndef ARKUI_DEMO_EGL_CORE_H
#define ARKUI_DEMO_EGL_CORE_H

#include "../video_stream_handler.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>

namespace VideoStreamNS {
class EGLCore {
public:
    explicit EGLCore()
        : yTexture_(0), uTexture_(0), vTexture_(0), VAO_(0), VBO_(0), EBO_(0), texturesInitialized_(false), width_(0),
          height_(0) {};
    ~EGLCore() {}
    bool EglContextInit(void *window);
    bool CreateEnvironment();
    bool RenderYUVFrame(const VideoFrame &frame);
    void Release();
    void UpdateSize(int width, int height);

private:
    GLuint LoadShader(GLenum type, const char *shaderSrc);
    GLuint CreateProgram(const char *vertexShader, const char *fragShader);
    bool InitYUVTextures();
    bool UpdateYUVTextures(const VideoFrame &frame);
    void DrawQuad();

private:
    EGLNativeWindowType eglWindow_;
    EGLDisplay eglDisplay_ = EGL_NO_DISPLAY;
    EGLConfig eglConfig_ = EGL_NO_CONFIG_KHR;
    EGLSurface eglSurface_ = EGL_NO_SURFACE;
    EGLContext eglContext_ = EGL_NO_CONTEXT;
    GLuint program_;
    int width_;
    int height_;

    // YUV纹理相关
    GLuint yTexture_;
    GLuint uTexture_;
    GLuint vTexture_;
    GLuint VAO_;
    GLuint VBO_;
    GLuint EBO_;
    GLint yTextureLocation_;
    GLint uTextureLocation_;
    GLint vTextureLocation_;
    bool texturesInitialized_;
};
} // namespace VideoStreamNS

#endif // ARKUI_DEMO_EGL_CORE_H
