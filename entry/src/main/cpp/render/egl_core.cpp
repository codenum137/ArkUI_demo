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
#include <string>
#include <vector>


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
 * Get detailed OpenGL error description
 */
const char *GetGLErrorString(GLenum error) {
    switch (error) {
    case GL_NO_ERROR:
        return "GL_NO_ERROR";
    case GL_INVALID_ENUM:
        return "GL_INVALID_ENUM - An unacceptable value is specified for an enumerated argument";
    case GL_INVALID_VALUE:
        return "GL_INVALID_VALUE - A numeric argument is out of range";
    case GL_INVALID_OPERATION:
        return "GL_INVALID_OPERATION - The specified operation is not allowed in the current state";
    case GL_OUT_OF_MEMORY:
        return "GL_OUT_OF_MEMORY - There is not enough memory left to execute the command";
    case GL_INVALID_FRAMEBUFFER_OPERATION:
        return "GL_INVALID_FRAMEBUFFER_OPERATION - The framebuffer object is not complete";
    default:
        return "Unknown OpenGL error";
    }
}

/**
 * Check and report OpenGL errors with detailed information
 */
bool CheckGLError(const char *operation) {
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "EGLCore",
                     "OpenGL error in %{public}s: 0x%{public}x (%{public}d) - %{public}s", operation, error, error,
                     GetGLErrorString(error));
        return false;
    }
    return true;
}

/**
 * Print complete first frame YUV data and terminate program
 */
void PrintFirstFrameDataAndExit(const VideoFrame &frame) {
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "EGLCore", "=== FIRST FRAME DATA ANALYSIS ===");
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "EGLCore",
                 "Frame: %{public}dx%{public}d, linesize[Y=%{public}d, U=%{public}d, V=%{public}d]", frame.width,
                 frame.height, frame.linesize[0], frame.linesize[1], frame.linesize[2]);

    // 打印Y平面完整数据 - 每行一个字符串
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "EGLCore", "=== Y PLANE FULL DATA ===");
    for (int row = 0; row < frame.height; row++) {
        std::string line_data = "";
        for (int col = 0; col < frame.linesize[0]; col++) {
            if (col > 0)
                line_data += " ";
            line_data += std::to_string(frame.data[0][row * frame.linesize[0] + col]);
        }
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "EGLCore", "Y_row_%{public}d: %{public}s", row,
                     line_data.c_str());
    }

    // 打印U平面完整数据
    if (frame.data[1] != nullptr) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "EGLCore", "=== U PLANE FULL DATA ===");
        for (int row = 0; row < frame.height / 2; row++) {
            std::string line_data = "";
            for (int col = 0; col < frame.linesize[1]; col++) {
                if (col > 0)
                    line_data += " ";
                line_data += std::to_string(frame.data[1][row * frame.linesize[1] + col]);
            }
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "EGLCore", "U_row_%{public}d: %{public}s", row,
                         line_data.c_str());
        }
    }

    // 打印V平面完整数据
    if (frame.data[2] != nullptr) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "EGLCore", "=== V PLANE FULL DATA ===");
        for (int row = 0; row < frame.height / 2; row++) {
            std::string line_data = "";
            for (int col = 0; col < frame.linesize[2]; col++) {
                if (col > 0)
                    line_data += " ";
                line_data += std::to_string(frame.data[2][row * frame.linesize[2] + col]);
            }
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "EGLCore", "V_row_%{public}d: %{public}s", row,
                         line_data.c_str());
        }
    }

    // 数据指针地址
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "EGLCore",
                 "Data pointers: Y=%{public}p, U=%{public}p, V=%{public}p", frame.data[0], frame.data[1],
                 frame.data[2]);

    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "EGLCore", "=== END FIRST FRAME DATA ANALYSIS ===");
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "EGLCore",
                 "First frame analysis complete. Terminating program to ensure complete output.");

    // 强制刷新日志输出
    fflush(stdout);
    fflush(stderr);

    // 程序终止，确保第一帧数据完整输出
    exit(0);
}

/**
 * Create a test YUV frame with gradient pattern and FFmpeg-style padding
 */
bool LoadYUVFromRawfile(VideoFrame &frame) {
    // 创建320x176的YUV420测试帧，模拟FFmpeg的填充格式
    const int width = 320;
    const int height = 176;

    // 模拟FFmpeg的linesize（包含填充）
    const int y_linesize = 384;  // 320 + 64 填充
    const int uv_linesize = 192; // 160 + 32 填充

    const int y_total_size = y_linesize * height;
    const int uv_total_size = uv_linesize * (height / 2);

    // 使用静态存储确保数据在函数返回后仍然有效
    static std::vector<uint8_t> y_data(y_total_size);
    static std::vector<uint8_t> u_data(uv_total_size);
    static std::vector<uint8_t> v_data(uv_total_size);

    // 初始化所有数据为0（模拟填充区域）
    std::fill(y_data.begin(), y_data.end(), 0);
    std::fill(u_data.begin(), u_data.end(), 128); // UV默认中性值
    std::fill(v_data.begin(), v_data.end(), 128);

    // 创建Y平面渐变图案 - 只填充前320列，后64列保持为填充
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) { // 只填充实际宽度
            // 创建对角线渐变效果
            int value = ((x + y) * 255) / (width + height);
            y_data[y * y_linesize + x] = static_cast<uint8_t>(value);
        }
        // 填充区域保持为0（已经初始化）
    }

    // 创建U平面图案 - 只填充前160列，后32列保持为填充
    for (int y = 0; y < height / 2; y++) {
        for (int x = 0; x < width / 2; x++) {         // 只填充实际宽度
            int value = 128 + (x * 64) / (width / 2); // 128-192 范围
            u_data[y * uv_linesize + x] = static_cast<uint8_t>(std::min(255, value));
        }
        // 填充区域保持为128（已经初始化）
    }

    // 创建V平面图案 - 只填充前160列，后32列保持为填充
    for (int y = 0; y < height / 2; y++) {
        for (int x = 0; x < width / 2; x++) {          // 只填充实际宽度
            int value = 128 + (y * 64) / (height / 2); // 128-192 范围
            v_data[y * uv_linesize + x] = static_cast<uint8_t>(std::min(255, value));
        }
        // 填充区域保持为128（已经初始化）
    }

    // 填充VideoFrame结构 - 使用带填充的linesize
    frame.width = width;
    frame.height = height;
    frame.data[0] = y_data.data();
    frame.data[1] = u_data.data();
    frame.data[2] = v_data.data();
    frame.linesize[0] = y_linesize;  // 384（包含64字节填充）
    frame.linesize[1] = uv_linesize; // 192（包含32字节填充）
    frame.linesize[2] = uv_linesize; // 192（包含32字节填充）

    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "EGLCore",
                 "Created test YUV pattern with padding: %{public}dx%{public}d, linesize[Y=%{public}d, U=%{public}d, "
                 "V=%{public}d]",
                 width, height, frame.linesize[0], frame.linesize[1], frame.linesize[2]);

    // 验证填充数据
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "EGLCore",
                 "Y padding: %{public}d bytes, UV padding: %{public}d bytes per row", y_linesize - width,
                 uv_linesize - (width / 2));

    return true;
}

/**
 * YUV420 to RGB vertex shader.
 */
const char YUV_VERTEX_SHADER[] = "#version 300 es\n"
                                 "layout(location = 0) in vec2 a_position;\n"
                                 "layout(location = 1) in vec2 a_texCoord;\n"
                                 "out vec2 v_texCoord;\n"
                                 "void main() {\n"
                                 "    gl_Position = vec4(a_position,0.0,1.0);\n"
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
    EGL_OPENGL_ES3_BIT, // 修复：添加ES3支持
    // End.
    EGL_NONE};

/**
 * Context attributes.
 */
const EGLint CONTEXT_ATTRIBS[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
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
    const GLubyte *version = glGetString(GL_VERSION);   // 获取OpenGL版本（如 "OpenGL ES 2.0"）
    const GLubyte *renderer = glGetString(GL_RENDERER); // 获取渲染器名称（如GPU型号）
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "EGLCore", "GL_VERSION: %{public}s, GL_RENDERER:%{public}s",
                 version, renderer);
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

    // Configure Y texture parameters (only once during initialization)
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, yTexture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Configure U texture parameters (only once during initialization)
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, uTexture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Configure V texture parameters (only once during initialization)
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, vTexture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Get uniform locations
    yTextureLocation_ = glGetUniformLocation(program_, "y_texture");
    uTextureLocation_ = glGetUniformLocation(program_, "u_texture");
    vTextureLocation_ = glGetUniformLocation(program_, "v_texture");

    texturesInitialized_ = true;

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

    // 检查是否已经渲染了第一帧，如果是则跳过后续帧的渲染
    if (firstFrameRendered_) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "EGLCore",
                     "First frame already rendered, skipping subsequent frames");
        return true; // 返回true表示"成功"，但实际上跳过了渲染
    }

    // 打印第一帧的详细数据
    // if (frame.data[0] != nullptr) {
    //     PrintFirstFrameDataAndExit(frame);
    // }

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
    if (!CheckGLError("after DrawQuad")) {
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

    // 标记第一帧已经渲染完成
    if (!firstFrameRendered_) {
        firstFrameRendered_ = true;
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "EGLCore",
                     "First frame rendered successfully. Subsequent frames will be skipped.");
    }

    // OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "EGLCore", "Frame rendered successfully to surface");
    return true;
}

bool EGLCore::UpdateYUVTextures(const VideoFrame &frame) {
    if (frame.data[0] == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "EGLCore", "Y plane data is null");
        return false;
    }

    // 验证帧数据有效性
    if (frame.width <= 0 || frame.height <= 0) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "EGLCore", "Invalid frame dimensions: %{public}dx%{public}d",
                     frame.width, frame.height);
        return false;
    }

    if (frame.linesize[0] <= 0) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "EGLCore", "Invalid Y linesize: %{public}d",
                     frame.linesize[0]);
        return false;
    }

    // 添加数据验证
    OH_LOG_Print(
        LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "EGLCore",
        "UpdateYUVTextures: %{public}dx%{public}d, Y_linesize=%{public}d, U_linesize=%{public}d, V_linesize=%{public}d",
        frame.width, frame.height, frame.linesize[0], frame.linesize[1], frame.linesize[2]);

    // Clear any existing OpenGL errors
    while (glGetError() != GL_NO_ERROR) {
    }

    // 保存当前的像素存储参数
    GLint oldUnpackAlignment;
    GLint oldUnpackRowLength;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &oldUnpackAlignment);
    glGetIntegerv(GL_UNPACK_ROW_LENGTH, &oldUnpackRowLength);

    // Update Y texture with linesize padding handling
    glActiveTexture(GL_TEXTURE0);
    if (!CheckGLError("glActiveTexture(GL_TEXTURE0)"))
        return false;

    glBindTexture(GL_TEXTURE_2D, yTexture_);
    if (!CheckGLError("glBindTexture Y texture"))
        return false;

    // 设置像素存储参数来处理linesize填充
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);                  // 字节对齐
    glPixelStorei(GL_UNPACK_ROW_LENGTH, frame.linesize[0]); // 设置行长度为linesize

    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, frame.width, frame.height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE,
                 frame.data[0]);
    if (!CheckGLError("Y texture glTexImage2D with linesize"))
        return false;

    // Update U texture with linesize padding handling
    if (frame.data[1] != nullptr) {
        glActiveTexture(GL_TEXTURE1);
        if (!CheckGLError("glActiveTexture(GL_TEXTURE1)"))
            return false;

        glBindTexture(GL_TEXTURE_2D, uTexture_);
        if (!CheckGLError("glBindTexture U texture"))
            return false;

        // 设置U平面的行长度
        glPixelStorei(GL_UNPACK_ROW_LENGTH, frame.linesize[1]);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, frame.width / 2, frame.height / 2, 0, GL_LUMINANCE,
                     GL_UNSIGNED_BYTE, frame.data[1]);
        if (!CheckGLError("U texture glTexImage2D with linesize"))
            return false;
    }

    // Update V texture with linesize padding handling
    if (frame.data[2] != nullptr) {
        glActiveTexture(GL_TEXTURE2);
        if (!CheckGLError("glActiveTexture(GL_TEXTURE2)"))
            return false;

        glBindTexture(GL_TEXTURE_2D, vTexture_);
        if (!CheckGLError("glBindTexture V texture"))
            return false;

        // 设置V平面的行长度
        glPixelStorei(GL_UNPACK_ROW_LENGTH, frame.linesize[2]);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, frame.width / 2, frame.height / 2, 0, GL_LUMINANCE,
                     GL_UNSIGNED_BYTE, frame.data[2]);
        if (!CheckGLError("V texture glTexImage2D with linesize"))
            return false;
    }

    // 恢复原来的像素存储参数
    glPixelStorei(GL_UNPACK_ALIGNMENT, oldUnpackAlignment);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, oldUnpackRowLength);

    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "EGLCore",
                 "YUV textures updated successfully with linesize padding handled");

    return true;
}

void EGLCore::DrawQuad() {
    glBindVertexArray(VAO_);
    if (!CheckGLError("glBindVertexArray"))
        return;

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    if (!CheckGLError("glDrawElements"))
        return;

    glBindVertexArray(0);
    CheckGLError("glBindVertexArray(0)");
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
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "EGLCore", "glCompileShader error = %{public}s", infoLog);
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
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "EGLCore", "glLinkProgram error = %{public}s", infoLog);
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

    // 如果纹理已经初始化并且窗口尺寸有效，渲染测试帧
    if (texturesInitialized_ && width_ > 0 && height_ > 0) {
        VideoFrame testFrame;
        if (LoadYUVFromRawfile(testFrame)) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "EGLCore",
                         "Rendering test frame with padding after size update");

            // 确保EGL上下文是当前的
            if (eglMakeCurrent(eglDisplay_, eglSurface_, eglSurface_, eglContext_)) {
                // 清除屏幕
                glViewport(DEFAULT_X_POSITION, DEFAULT_Y_POSITION, width_, height_);
                glClearColor(0.0f, 0.0f, 1.0f, 1.0f); // 蓝色背景
                glClear(GL_COLOR_BUFFER_BIT);

                // 更新纹理并渲染
                if (UpdateYUVTextures(testFrame)) {
                    glUseProgram(program_);

                    // 绑定纹理
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, yTexture_);
                    glUniform1i(yTextureLocation_, 0);

                    glActiveTexture(GL_TEXTURE1);
                    glBindTexture(GL_TEXTURE_2D, uTexture_);
                    glUniform1i(uTextureLocation_, 1);

                    glActiveTexture(GL_TEXTURE2);
                    glBindTexture(GL_TEXTURE_2D, vTexture_);
                    glUniform1i(vTextureLocation_, 2);

                    // 绘制
                    glBindVertexArray(VAO_);
                    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
                    glBindVertexArray(0);

                    // 交换缓冲区显示
                    glFlush();
                    eglSwapBuffers(eglDisplay_, eglSurface_);

                    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "EGLCore",
                                 "Test YUV frame with padding displayed after UpdateSize");
                } else {
                    OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "EGLCore",
                                 "Failed to update YUV textures in UpdateSize");
                }
            } else {
                OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "EGLCore",
                             "Failed to make EGL context current in UpdateSize");
            }
        } else {
            OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "EGLCore",
                         "Failed to load test YUV frame in UpdateSize");
        }
    }
}

void EGLCore::Release() {
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
} // namespace VideoStreamNS
