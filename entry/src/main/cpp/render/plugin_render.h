//
// Created on 2025/7/10.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#ifndef ARKUI_DEMO_VIDEO_RENDERER_H
#define ARKUI_DEMO_VIDEO_RENDERER_H

#include "../video_stream_handler.h"
#include "egl_core.h"
#include <ace/xcomponent/native_interface_xcomponent.h>
#include <native_window/external_window.h>

namespace VideoStreamNS {
class VideoRenderer {
public:
    explicit VideoRenderer(int64_t surfaceId);
    ~VideoRenderer() {
        if (eglCore_ != nullptr) {
            eglCore_->Release();
            delete eglCore_;
            eglCore_ = nullptr;
        }
    }

    bool RenderYUVFrame(const VideoFrame &frame);
    bool IsInitialized() const;
    void InitNativeWindow(OHNativeWindow *window);
    void UpdateSize(int width, int height);

private:
    EGLCore *eglCore_;
    int64_t surfaceId_;
    bool isInitialized_;
};
} // namespace VideoStreamNS

#endif // ARKUI_DEMO_VIDEO_RENDERER_H
