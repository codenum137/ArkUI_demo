//
// Created on 2025/7/10.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#include "plugin_render.h"
#include "../common/common.h"
#include <chrono>
#include <cstdint>
#include <hilog/log.h>

namespace VideoStreamNS {
VideoRenderer::VideoRenderer(int64_t surfaceId) {
    this->surfaceId_ = surfaceId;
    this->eglCore_ = new EGLCore();
    isInitialized_ = false;
    renderThreadRunning_ = false;

    // 创建帧缓冲队列
    frameQueue_ = std::make_unique<FrameBufferQueue>(10); // 最多缓存10帧

    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "VideoRenderer", "Created with frame queue, surfaceId: %lld",
                 static_cast<long long>(surfaceId));
}

// 废弃的直接渲染方法，现在只通过队列渲染
// bool VideoRenderer::RenderYUVFrame(const VideoFrame &frame) {
//     OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "VideoRenderer",
//                  "RenderYUVFrame is deprecated, use pushFrame instead");
//     return pushFrame(frame);
// }


void VideoRenderer::InitNativeWindow(OHNativeWindow *window) {
    if (eglCore_->EglContextInit(window)) {
        isInitialized_ = true;

        // 启动渲染线程
        startRenderThread();

        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "VideoRenderer", "InitNativeWindow success");
    } else {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "VideoRenderer", "InitNativeWindow failed");
    }
}

void VideoRenderer::UpdateSize(int width, int height) {
    if (eglCore_ != nullptr) {
        eglCore_->UpdateSize(width, height);
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "VideoRenderer", "UpdateSize: %{public}dx%{public}d", width,
                     height);
    }
}

bool VideoRenderer::IsInitialized() const { return isInitialized_; }

bool VideoRenderer::pushFrame(const VideoFrame &frame) {
    if (!frameQueue_) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "VideoRenderer", "Frame queue not initialized");
        return false;
    }

    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "VideoRenderer",
                 "Pushing frame to queue: %{public}dx%{public}d, queue size before: %{public}zu", frame.width,
                 frame.height, frameQueue_->size());

    bool result = frameQueue_->push(frame);

    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "VideoRenderer",
                 "Push result: %{public}s, queue size after: %{public}zu", result ? "SUCCESS" : "FAILED",
                 frameQueue_->size());

    return result;
}

bool VideoRenderer::renderSingleTestFrame(float r, float g, float b, float a) {
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "VideoRenderer", 
                 "renderSingleTestFrame: Rendering single test frame with color (%.2f, %.2f, %.2f, %.2f)", r, g, b, a);
    
    if (!isInitialized_ || !eglCore_) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "VideoRenderer", 
                     "renderSingleTestFrame: Renderer not initialized");
        return false;
    }
    
    // 直接调用EGLCore的单帧渲染方法
    bool result = eglCore_->RenderSingleTestFrame(r, g, b, a);
    
    if (result) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "VideoRenderer", 
                     "renderSingleTestFrame: Single test frame rendered successfully");
    } else {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "VideoRenderer", 
                     "renderSingleTestFrame: Failed to render single test frame");
    }
    
    return result;
}

void VideoRenderer::startRenderThread() {
    if (renderThreadRunning_.exchange(true)) {
        OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "VideoRenderer", "Render thread already running");
        return;
    }

    renderThread_ = std::thread(&VideoRenderer::renderThreadFunc, this);
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "VideoRenderer", "Render thread started");
}

void VideoRenderer::stopRenderThread() {
    if (!renderThreadRunning_.exchange(false)) {
        return; // 已经停止了
    }

    if (frameQueue_) {
        frameQueue_->stop();
    }

    if (renderThread_.joinable()) {
        renderThread_.join();
    }

    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "VideoRenderer", "Render thread stopped");
}

void VideoRenderer::renderThreadFunc() {
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "VideoRenderer", "Render thread function started");

    int frameCount = 0;
    auto lastLogTime = std::chrono::steady_clock::now();

    while (renderThreadRunning_) {
        if (!isInitialized_ || !eglCore_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        // 从队列中取出帧（带超时）
        auto copiedFrame = frameQueue_->pop(50); // 50ms超时
        if (!copiedFrame) {
            // OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_PRINT_DOMAIN, "VideoRenderer", "No frame available,
            // timeout");
            continue; // 超时或队列为空，继续等待
        }

        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "VideoRenderer",
                     "Got frame from queue: %{public}dx%{public}d, queue size: %{public}zu", copiedFrame->width,
                     copiedFrame->height, frameQueue_->size());

        // 转换为VideoFrame格式
        VideoFrame frame = copiedFrame->toVideoFrame();

        // 渲染帧
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "VideoRenderer", "Attempting to render frame...");

        if (eglCore_->RenderYUVFrame(frame)) {
            frameCount++;

            // 每30帧或每3秒输出一次日志
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastLogTime);
            if (frameCount % 30 == 0 || elapsed.count() >= 3) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "VideoRenderer",
                             "Rendered %{public}d frames, queue size: %{public}zu", frameCount, frameQueue_->size());
                lastLogTime = now;
            }
        } else {
            OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "VideoRenderer", "Failed to render frame");
        }
    }

    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "VideoRenderer",
                 "Render thread function ended, total frames rendered: %{public}d", frameCount);
}
} // namespace VideoStreamNS
