//
// Created on 2025/7/11.
//

#ifndef ARKUI_DEMO_FRAME_BUFFER_QUEUE_H
#define ARKUI_DEMO_FRAME_BUFFER_QUEUE_H

#include "../video_stream_handler.h"
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>

namespace VideoStreamNS {

/**
 * 视频帧数据，包含深拷贝的数据
 */
struct CopiedVideoFrame {
    std::unique_ptr<uint8_t[]> yData;
    std::unique_ptr<uint8_t[]> uData;
    std::unique_ptr<uint8_t[]> vData;
    int width;
    int height;
    int yLinesize;
    int uLinesize;
    int vLinesize;
    int64_t pts;

    // 从FFmpeg帧拷贝数据
    static std::shared_ptr<CopiedVideoFrame> copyFromVideoFrame(const VideoFrame &frame);
    
    // 转换回VideoFrame格式供OpenGL使用
    VideoFrame toVideoFrame() const;
};

/**
 * 线程安全的帧缓冲队列
 * FFmpeg生产者线程将解码后的帧放入队列
 * OpenGL消费者线程从队列取出帧进行渲染
 */
class FrameBufferQueue {
public:
    explicit FrameBufferQueue(size_t maxSize = 10);
    ~FrameBufferQueue();

    // 生产者接口：将帧放入队列（非阻塞）
    bool push(const VideoFrame &frame);

    // 消费者接口：从队列取出帧（阻塞）
    std::shared_ptr<CopiedVideoFrame> pop(int timeoutMs = 100);

    // 消费者接口：尝试从队列取出帧（非阻塞）
    std::shared_ptr<CopiedVideoFrame> tryPop();

    // 清空队列
    void clear();

    // 获取当前队列大小
    size_t size() const;

    // 检查队列是否为空
    bool empty() const;

    // 停止队列操作
    void stop();

private:
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::queue<std::shared_ptr<CopiedVideoFrame>> queue_;
    size_t maxSize_;
    std::atomic<bool> stopped_;
};

} // namespace VideoStreamNS

#endif // ARKUI_DEMO_FRAME_BUFFER_QUEUE_H
