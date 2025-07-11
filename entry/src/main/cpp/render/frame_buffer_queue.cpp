//
// Created on 2025/7/11.
//

#include "frame_buffer_queue.h"
#include "../common/common.h"
#include <chrono>
#include <cstring>
#include <hilog/log.h>

namespace VideoStreamNS {

std::shared_ptr<CopiedVideoFrame> CopiedVideoFrame::copyFromVideoFrame(const VideoFrame &frame) {
    auto copiedFrame = std::make_shared<CopiedVideoFrame>();
    
    copiedFrame->width = frame.width;
    copiedFrame->height = frame.height;
    copiedFrame->yLinesize = frame.linesize[0];
    copiedFrame->uLinesize = frame.linesize[1];
    copiedFrame->vLinesize = frame.linesize[2];
    copiedFrame->pts = frame.pts;

    // 计算各平面数据大小
    int ySize = frame.linesize[0] * frame.height;
    int uSize = frame.linesize[1] * (frame.height / 2);
    int vSize = frame.linesize[2] * (frame.height / 2);

    // 分配内存并拷贝数据
    if (frame.data[0] && ySize > 0) {
        copiedFrame->yData = std::make_unique<uint8_t[]>(ySize);
        memcpy(copiedFrame->yData.get(), frame.data[0], ySize);
    }

    if (frame.data[1] && uSize > 0) {
        copiedFrame->uData = std::make_unique<uint8_t[]>(uSize);
        memcpy(copiedFrame->uData.get(), frame.data[1], uSize);
    }

    if (frame.data[2] && vSize > 0) {
        copiedFrame->vData = std::make_unique<uint8_t[]>(vSize);
        memcpy(copiedFrame->vData.get(), frame.data[2], vSize);
    }

    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_PRINT_DOMAIN, "CopiedVideoFrame", 
                 "Copied frame: %{public}dx%{public}d, Y_size=%{public}d, U_size=%{public}d, V_size=%{public}d", 
                 frame.width, frame.height, ySize, uSize, vSize);

    return copiedFrame;
}

VideoFrame CopiedVideoFrame::toVideoFrame() const {
    VideoFrame frame;
    frame.width = width;
    frame.height = height;
    frame.pts = pts;
    
    frame.data[0] = yData.get();
    frame.data[1] = uData.get();
    frame.data[2] = vData.get();
    
    frame.linesize[0] = yLinesize;
    frame.linesize[1] = uLinesize;
    frame.linesize[2] = vLinesize;
    
    return frame;
}

FrameBufferQueue::FrameBufferQueue(size_t maxSize) : maxSize_(maxSize), stopped_(false) {
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "FrameBufferQueue", 
                 "Created with max size: %{public}zu", maxSize_);
}

FrameBufferQueue::~FrameBufferQueue() {
    stop();
    clear();
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "FrameBufferQueue", "Destroyed");
}

bool FrameBufferQueue::push(const VideoFrame &frame) {
    if (stopped_) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    
    // 如果队列满了，丢弃最老的帧
    if (queue_.size() >= maxSize_) {
        queue_.pop();
        OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "FrameBufferQueue", 
                     "Queue full, dropped oldest frame");
    }

    // 深拷贝帧数据
    auto copiedFrame = CopiedVideoFrame::copyFromVideoFrame(frame);
    if (copiedFrame) {
        queue_.push(copiedFrame);
        condition_.notify_one();
        
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "FrameBufferQueue", 
                     "Pushed frame, queue size: %zu", queue_.size());
        return true;
    }

    return false;
}

std::shared_ptr<CopiedVideoFrame> FrameBufferQueue::pop(int timeoutMs) {
    std::unique_lock<std::mutex> lock(mutex_);
    
    if (condition_.wait_for(lock, std::chrono::milliseconds(timeoutMs), 
                           [this] { return !queue_.empty() || stopped_; })) {
        if (!queue_.empty()) {
            auto frame = queue_.front();
            queue_.pop();
            // OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_PRINT_DOMAIN, "FrameBufferQueue", 
            //              "Popped frame, queue size: %zu", queue_.size());
            return frame;
        }
    }
    
    return nullptr;
}

std::shared_ptr<CopiedVideoFrame> FrameBufferQueue::tryPop() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!queue_.empty()) {
        auto frame = queue_.front();
        queue_.pop();
        return frame;
    }
    
    return nullptr;
}

void FrameBufferQueue::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    size_t clearedCount = queue_.size();
    while (!queue_.empty()) {
        queue_.pop();
    }
    
    if (clearedCount > 0) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "FrameBufferQueue", 
                     "Cleared %{public}zu frames", clearedCount);
    }
}

size_t FrameBufferQueue::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

bool FrameBufferQueue::empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty();
}

void FrameBufferQueue::stop() {
    stopped_ = true;
    condition_.notify_all();
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "FrameBufferQueue", "Stopped");
}

} // namespace VideoStreamNS
