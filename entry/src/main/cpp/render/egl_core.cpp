//
// Created on 2025/7/10.
// #include "../common/common.h"

// Is are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// 强制定义HarmonyOS平台，必须在包含EGL头文件之前
#ifndef __OHOS__
#define __OHOS__
#endif

#ifndef OHOS_PLATFORM
#define OHOS_PLATFORM
#endif

#include "egl_core.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <EGL/eglplatform.h>
#include <GLES3/gl3.h>
#include <cmath>
#include <cstdio>
#include <hilog/log.h>
#include <native_window/external_window.h>

#include "../common/common.h"
#include "plugin_render.h"

// 编译时类型检查
#include <type_traits>

// 确保在HarmonyOS上EGLNativeWindowType是指针类型
#ifdef __OHOS__
static_assert(std::is_pointer<EGLNativeWindowType>::value || sizeof(EGLNativeWindowType) == sizeof(void *),
              "EGLNativeWindowType should be compatible with pointer type on HarmonyOS platform");
#endif

namespace VideoStreamNS {
namespace {
/**
 * YUV420 to RGB vertex shader.
 */
const char YUV_VERTEX_SHADER[] = "#version 300 es\n"
                                 "layout(location = 0) in vec4 a_position;\n"
                                 "layout(location = 1) in vec2 a_texCoord;\n"
                                 "out vec2 v_texCoord;\n"
                                 "void main() {\n"
                                 "    gl_Position = a_position;\n"
                                 "    v_texCoord = a_texCoord;\n"
                                 "}\n";


/**
 * YUV420 to RGB fragment shader.
 */
const char YUV_FRAGMENT_SHADER[] = "#version 300 es\n"
                                   "precision mediump float;\n"
                                   "in vec2 v_texCoord;\n"
                                   "out vec4 fragColor;\n"
                                   "uniform sampler2D y_texture;\n"
                                   "uniform sampler2D u_texture;\n"
                                   "uniform sampler2D v_texture;\n"
                                   "void main() {\n"
                                   "    float y = texture(y_texture, v_texCoord).r;\n"
                                   "    float u = texture(u_texture, v_texCoord).r - 0.5;\n"
                                   "    float v = texture(v_texture, v_texCoord).r - 0.5;\n"
                                   "    float r = y + 1.402 * v;\n"
                                   "    float g = y - 0.344136 * u - 0.714136 * v;\n"
                                   "    float b = y + 1.772 * u;\n"
                                   "    fragColor = vec4(r, g, b, 1.0);\n"
                                   "}\n";

/**
 * Quad vertices for full screen rendering.
 * Format: x, y, u, v
 */
const GLfloat QUAD_VERTICES[] = {
    // positions    // texture coords
    -1.0f, -1.0f, 0.0f, 1.0f, // 左下
    1.0f,  -1.0f, 1.0f, 1.0f, // 右下
    1.0f,  1.0f,  1.0f, 0.0f, // 右上
    -1.0f, 1.0f,  0.0f, 0.0f  // 左上
};

const GLuint QUAD_INDICES[] = {0, 1, 2, 2, 3, 0};

/**
 * Egl red size default.
 */
const int EGL_RED_SIZE_DEFAULT = 8;

/**
 * Egl green size default.
 */
const int EGL_GREEN_SIZE_DEFAULT = 8;

/**
 * Egl blue size default.
 */
const int EGL_BLUE_SIZE_DEFAULT = 8;

/**
 * Egl alpha size default.
 */
const int EGL_ALPHA_SIZE_DEFAULT = 8;

/**
 * Default x position.
 */
const int DEFAULT_X_POSITION = 0;

/**
 * Default y position.
 */
const int DEFAULT_Y_POSITION = 0;

/**
 * Program error.
 */
const GLuint PROGRAM_ERROR = 0;

/**
 * Config attribute list.
 */
const EGLint ATTRIB_LIST[] = {
    // Key,value.
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RED_SIZE, EGL_RED_SIZE_DEFAULT, EGL_GREEN_SIZE, EGL_GREEN_SIZE_DEFAULT,
    EGL_BLUE_SIZE, EGL_BLUE_SIZE_DEFAULT, EGL_ALPHA_SIZE, EGL_ALPHA_SIZE_DEFAULT, EGL_RENDERABLE_TYPE,
    EGL_OPENGL_ES2_BIT,
    // End.
    EGL_NONE};

/**
 * Context attributes.
 */
const EGLint CONTEXT_ATTRIBS[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
} // namespace

bool EGLCore::EglContextInit(void *window) {
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "EGLCore", "EglContextInit execute");

    // 安全的类型转换
    if (window == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "EGLCore", "Input window is null");
        return false;
    }

    // 检查类型兼容性并进行转换
#ifdef __OHOS__
    // 在HarmonyOS上，如果EGLNativeWindowType是指针类型，使用static_cast
    if constexpr (std::is_pointer<EGLNativeWindowType>::value) {
        eglWindow_ = static_cast<EGLNativeWindowType>(window);
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "EGLCore", "Using pointer-type EGLNativeWindowType");
    } else {
        // 如果不是指针类型，使用reinterpret_cast（但这可能有问题）
        eglWindow_ = reinterpret_cast<EGLNativeWindowType>(window);
        OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "EGLCore",
                     "EGLNativeWindowType is not pointer type, using reinterpret_cast - this may cause issues");
    }
#else
    eglWindow_ = reinterpret_cast<EGLNativeWindowType>(window);
#endif

    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "EGLCore", "EGL window set, input pointer: %p", window);

    // Init display.
    eglDisplay_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (eglDisplay_ == EGL_NO_DISPLAY) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "EGLCore", "eglGetDisplay: unable to get EGL display");
        return false;
    }

    EGLint majorVersion;
    EGLint minorVersion;
    if (!eglInitialize(eglDisplay_, &majorVersion, &minorVersion)) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "EGLCore",
                     "eglInitialize: unable to get initialize EGL display");
        return false;
    }

    // Select configuration.
    const EGLint maxConfigSize = 1;
    EGLint numConfigs;
    if (!eglChooseConfig(eglDisplay_, ATTRIB_LIST, &eglConfig_, maxConfigSize, &numConfigs)) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "EGLCore", "eglChooseConfig: unable to choose configs");
        return false;
    }

    return CreateEnvironment();
}

bool EGLCore::CreateEnvironment() {
    // Create surface.
    if (eglWindow_ == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "EGLCore", "eglWindow_ is null/zero");
        return false;
    }

    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "EGLCore", "Creating EGL window surface...");
    eglSurface_ = eglCreateWindowSurface(eglDisplay_, eglConfig_, eglWindow_, NULL);
    if (eglSurface_ == nullptr) {
        EGLint eglError = eglGetError();
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "EGLCore", "eglCreateWindowSurface failed, error: 0x%x",
                     eglError);
        return false;
    }

    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "EGLCore", "EGL window surface created successfully");
    // Create context.
    eglContext_ = eglCreateContext(eglDisplay_, eglConfig_, EGL_NO_CONTEXT, CONTEXT_ATTRIBS);
    if (!eglMakeCurrent(eglDisplay_, eglSurface_, eglSurface_, eglContext_)) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "EGLCore", "eglMakeCurrent failed");
        return false;
    }
    // Create program.
    program_ = CreateProgram(YUV_VERTEX_SHADER, YUV_FRAGMENT_SHADER);
    // program_ = CreateProgram(YUV_VERTEX_SHADER, TEST_FRAGMENT_SHADER); // 纯红
    if (program_ == PROGRAM_ERROR) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "EGLCore", "CreateProgram: unable to create program");
        return false;
    }

    // Initialize YUV textures and buffers
    if (!InitYUVTextures()) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "EGLCore", "InitYUVTextures failed");
        return false;
    }

    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "EGLCore", "CreateEnvironment success");
    return true;
}

bool EGLCore::InitYUVTextures() {
    // Generate and bind VAO
    glGenVertexArrays(1, &VAO_);
    glBindVertexArray(VAO_);

    // Generate and bind VBO
    glGenBuffers(1, &VBO_);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(QUAD_VERTICES), QUAD_VERTICES, GL_STATIC_DRAW);

    // Generate and bind EBO
    glGenBuffers(1, &EBO_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(QUAD_INDICES), QUAD_INDICES, GL_STATIC_DRAW);

    // Position attribute (location 0): x, y
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    // Texture coordinate attribute (location 1): u, v
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Generate YUV textures
    glGenTextures(1, &yTexture_);
    glGenTextures(1, &uTexture_);
    glGenTextures(1, &vTexture_);

    // Get uniform locations
    yTextureLocation_ = glGetUniformLocation(program_, "y_texture");
    uTextureLocation_ = glGetUniformLocation(program_, "u_texture");
    vTextureLocation_ = glGetUniformLocation(program_, "v_texture");

    texturesInitialized_ = true;
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "EGLCore", "YUV textures initialized");
    return true;
}

bool EGLCore::RenderYUVFrame(const VideoFrame &frame) {
    if (!texturesInitialized_) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "EGLCore", "Textures not initialized");
        return false;
    }

    if ((eglDisplay_ == nullptr) || (eglSurface_ == nullptr) || (eglContext_ == nullptr)) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "EGLCore", "RenderYUVFrame: EGL context is null");
        return false;
    }

    // 确保EGL上下文是当前的
    if (!eglMakeCurrent(eglDisplay_, eglSurface_, eglSurface_, eglContext_)) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "EGLCore", "RenderYUVFrame: eglMakeCurrent failed");
        return false;
    }

    // Update YUV textures with frame data
    if (!UpdateYUVTextures(frame)) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "EGLCore", "UpdateYUVTextures failed");
        return false;
    }

    // Clear and prepare for rendering
    glViewport(DEFAULT_X_POSITION, DEFAULT_Y_POSITION, width_, height_);
    glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Use shader program
    glUseProgram(program_);

    // Bind textures
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, yTexture_);
    glUniform1i(yTextureLocation_, 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, uTexture_);
    glUniform1i(uTextureLocation_, 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, vTexture_);
    glUniform1i(vTextureLocation_, 2);

    // Draw the quad
    DrawQuad();

    // 检查OpenGL错误
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "EGLCore", "OpenGL error before swap: %{public}d", error);
        return false;
    }

    // Flush and swap buffers to display on surface
    glFlush();

    bool swapResult = eglSwapBuffers(eglDisplay_, eglSurface_);
    if (!swapResult) {
        EGLint eglError = eglGetError();
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "EGLCore", "eglSwapBuffers failed, error: 0x%x", eglError);
        return false;
    }

    // OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "EGLCore", "Frame rendered successfully to surface");
    return true;
}

bool EGLCore::UpdateYUVTextures(const VideoFrame &frame) {
    if (frame.data[0] == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "EGLCore", "Y plane data is null");
        return false;
    }

    // 添加数据验证
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "EGLCore",
                 "Frame data pointers: Y= %{public}p, U= %{public}p, V= %{public}p", frame.data[0], frame.data[1],
                 frame.data[2]);

    OH_LOG_Print(
        LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "EGLCore",
        "UpdateYUVTextures: %{public}dx%{public}d, Y_linesize=%{public}d, U_linesize=%{public}d, V_linesize=%{public}d",
        frame.width, frame.height, frame.linesize[0], frame.linesize[1], frame.linesize[2]);


    // 检查是否有填充
    bool yPadding = (frame.linesize[0] != frame.width);
    bool uvPadding = (frame.linesize[1] != frame.width / 2);
    // OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "EGLCore",
    //              "Padding detected: Y_padding=%{public}s, UV_padding=%{public}s", yPadding ? "YES" : "NO",
    //              uvPadding ? "YES" : "NO");

    // Update Y texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, yTexture_);
    if (yPadding) {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, frame.linesize[0]);
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, frame.width, frame.height, 0, GL_RED, GL_UNSIGNED_BYTE, frame.data[0]);
    if (yPadding) {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Update U texture
    if (frame.data[1] != nullptr) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, uTexture_);
        if (uvPadding) {
            glPixelStorei(GL_UNPACK_ROW_LENGTH, frame.linesize[1]);
        }
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, frame.width / 2, frame.height / 2, 0, GL_RED, GL_UNSIGNED_BYTE,
                     frame.data[1]);
        if (uvPadding) {
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    // Update V texture
    if (frame.data[2] != nullptr) {
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, vTexture_);
        if (uvPadding) {
            glPixelStorei(GL_UNPACK_ROW_LENGTH, frame.linesize[2]);
        }
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, frame.width / 2, frame.height / 2, 0, GL_RED, GL_UNSIGNED_BYTE,
                     frame.data[2]);
        if (uvPadding) {
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    // 检查OpenGL错误
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "EGLCore", "OpenGL error in UpdateYUVTextures: %{public}d",
                     error);
        return false;
    }

    // OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "EGLCore", "YUV textures updated successfully");
    return true;
}

void EGLCore::DrawQuad() {
    glBindVertexArray(VAO_);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    // 检查绘制错误
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "EGLCore", "OpenGL error in DrawQuad: %{public}d", error);
    }
}

GLuint EGLCore::LoadShader(GLenum type, const char *shaderSrc) {
    if ((type <= 0) || (shaderSrc == nullptr)) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "EGLCore", "glCreateShader type or shaderSrc error");
        return PROGRAM_ERROR;
    }

    GLuint shader = glCreateShader(type);
    if (shader == 0) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "EGLCore", "glCreateShader unable to load shader");
        return PROGRAM_ERROR;
    }

    // The gl function has no return value.
    glShaderSource(shader, 1, &shaderSrc, nullptr);
    glCompileShader(shader);

    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled != 0) {
        return shader;
    }

    GLint infoLen = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
    if (infoLen <= 1) {
        glDeleteShader(shader);
        return PROGRAM_ERROR;
    }

    char *infoLog = (char *)malloc(sizeof(char) * (infoLen + 1));
    if (infoLog != nullptr) {
        memset(infoLog, 0, infoLen + 1);
        glGetShaderInfoLog(shader, infoLen, nullptr, infoLog);
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "EGLCore", "glCompileShader error = %s", infoLog);
        free(infoLog);
        infoLog = nullptr;
    }
    glDeleteShader(shader);
    return PROGRAM_ERROR;
}

GLuint EGLCore::CreateProgram(const char *vertexShader, const char *fragShader) {
    if ((vertexShader == nullptr) || (fragShader == nullptr)) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "EGLCore",
                     "createProgram: vertexShader or fragShader is null");
        return PROGRAM_ERROR;
    }

    GLuint vertex = LoadShader(GL_VERTEX_SHADER, vertexShader);
    if (vertex == PROGRAM_ERROR) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "EGLCore", "createProgram vertex error");
        return PROGRAM_ERROR;
    }

    GLuint fragment = LoadShader(GL_FRAGMENT_SHADER, fragShader);
    if (fragment == PROGRAM_ERROR) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "EGLCore", "createProgram fragment error");
        return PROGRAM_ERROR;
    }

    GLuint program = glCreateProgram();
    if (program == PROGRAM_ERROR) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "EGLCore", "createProgram program error");
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        return PROGRAM_ERROR;
    }

    // The gl function has no return value.
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);

    GLint linked;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked != 0) {
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        return program;
    }

    OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "EGLCore", "createProgram linked error");
    GLint infoLen = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLen);
    if (infoLen > 1) {
        char *infoLog = (char *)malloc(sizeof(char) * (infoLen + 1));
        memset(infoLog, 0, infoLen + 1);
        glGetProgramInfoLog(program, infoLen, nullptr, infoLog);
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "EGLCore", "glLinkProgram error = %s", infoLog);
        free(infoLog);
        infoLog = nullptr;
    }
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    glDeleteProgram(program);
    return PROGRAM_ERROR;
}

void EGLCore::UpdateSize(int width, int height) {
    width_ = width;
    height_ = height;
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "EGLCore", "UpdateSize: %{public}dx%{public}d", width_, height_);
}

void EGLCore::Release() {
    // Cleanup programs
    if (program_ != 0) {
        glDeleteProgram(program_);
        program_ = 0;
    }

    // Cleanup textures and buffers
    if (texturesInitialized_) {
        glDeleteTextures(1, &yTexture_);
        glDeleteTextures(1, &uTexture_);
        glDeleteTextures(1, &vTexture_);
        glDeleteVertexArrays(1, &VAO_);
        glDeleteBuffers(1, &VBO_);
        glDeleteBuffers(1, &EBO_);
        texturesInitialized_ = false;
    }

    if ((eglDisplay_ == nullptr) || (eglSurface_ == nullptr) || (!eglDestroySurface(eglDisplay_, eglSurface_))) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "EGLCore", "Release eglDestroySurface failed");
    }

    if ((eglDisplay_ == nullptr) || (eglContext_ == nullptr) || (!eglDestroyContext(eglDisplay_, eglContext_))) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "EGLCore", "Release eglDestroyContext failed");
    }

    if ((eglDisplay_ == nullptr) || (!eglTerminate(eglDisplay_))) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "EGLCore", "Release eglTerminate failed");
    }
}

bool EGLCore::RenderSingleTestFrame(float r, float g, float b, float a) {
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "EGLCore", 
                 "RenderSingleTestFrame: clearing with color (%.2f, %.2f, %.2f, %.2f)", r, g, b, a);
    
    // 检查EGL环境是否初始化
    if (eglDisplay_ == EGL_NO_DISPLAY || eglContext_ == EGL_NO_CONTEXT || eglSurface_ == EGL_NO_SURFACE) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "EGLCore", 
                     "RenderSingleTestFrame: EGL environment not initialized");
        return false;
    }
    
    // 设置当前上下文
    if (!eglMakeCurrent(eglDisplay_, eglSurface_, eglSurface_, eglContext_)) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "EGLCore", 
                     "RenderSingleTestFrame: eglMakeCurrent failed");
        return false;
    }
    
    // 设置视口
    glViewport(0, 0, width_, height_);
    
    // 设置清除颜色并清除缓冲区
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT);
    
    // 交换缓冲区显示内容
    if (!eglSwapBuffers(eglDisplay_, eglSurface_)) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "EGLCore", 
                     "RenderSingleTestFrame: eglSwapBuffers failed");
        return false;
    }
    
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "EGLCore", 
                 "RenderSingleTestFrame: Test frame rendered successfully");
    return true;
}
} // namespace VideoStreamNS
