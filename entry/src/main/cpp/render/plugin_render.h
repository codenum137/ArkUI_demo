//
// Created on 2025/7/10.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#ifndef ARKUI_DEMO_VIDEO_RENDERER_H
#define ARKUI_DEMO_VIDEO_RENDERER_H

#include "../video_stream_handler.h"
#include "egl_core.h"
#include "frame_buffer_queue.h"
#include <ace/xcomponent/native_interface_xcomponent.h>
#include <atomic>
#include <memory>
#include <native_window/external_window.h>
#include <thread>

namespace VideoStreamNS {
class VideoRenderer {
public:
    explicit VideoRenderer(int64_t surfaceId);
    ~VideoRenderer() {
        stopRenderThread();
        if (eglCore_ != nullptr) {
            eglCore_->Release();
            delete eglCore_;
            eglCore_ = nullptr;
        }
    }

    // 注意：不再提供直接渲染接口，所有渲染都通过队列进行
    // bool RenderYUVFrame(const VideoFrame &frame); // 已废弃，使用pushFrame代替
    bool IsInitialized() const;
    void InitNativeWindow(OHNativeWindow *window);
    void UpdateSize(int width, int height);

    // 生产者接口：将帧放入渲染队列
    bool pushFrame(const VideoFrame &frame);
    
    // 渲染单个测试帧（只渲染一次，不循环）
    bool renderSingleTestFrame(float r = 0.2f, float g = 0.6f, float b = 0.9f, float a = 1.0f);
    
    // 启动/停止渲染线程
    void startRenderThread();
    void stopRenderThread();

private:
    // 消费者线程函数
    void renderThreadFunc();

private:
    EGLCore *eglCore_;
    int64_t surfaceId_;
    bool isInitialized_;
    
    // 帧缓冲队列
    std::unique_ptr<FrameBufferQueue> frameQueue_;
    
    // 渲染线程
    std::thread renderThread_;
    std::atomic<bool> renderThreadRunning_;
};
} // namespace VideoStreamNS

#endif // ARKUI_DEMO_VIDEO_RENDERER_H
