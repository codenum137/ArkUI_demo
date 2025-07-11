//
// Created on 2025/7/10.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#include "plugin_render.h"
#include "../common/common.h"
#include <cstdint>
#include <hilog/log.h>

namespace VideoStreamNS {
VideoRenderer::VideoRenderer(int64_t surfaceId) {
    this->surfaceId_ = surfaceId;
    this->eglCore_ = new EGLCore();
    isInitialized_ = false;
}

bool VideoRenderer::RenderYUVFrame(const VideoFrame &frame) {
    if (!isInitialized_ || eglCore_ == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "VideoRenderer", "RenderYUVFrame: not initialized");
        return false;
    }

    return eglCore_->RenderYUVFrame(frame);
}

void VideoRenderer::InitNativeWindow(OHNativeWindow *window) {
    if (eglCore_->EglContextInit(window)) {
        isInitialized_ = true;
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
} // namespace VideoStreamNS
