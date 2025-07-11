#ifndef VIDEO_STREAM_HANDLER_H
#define VIDEO_STREAM_HANDLER_H

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
}

struct VideoFrame {
    uint8_t *data[3]; // Y, U, V平面数据指针
    int linesize[3];  // Y, U, V平面的行大小
    int width;
    int height;
    int64_t pts;
};

class VideoStreamHandler {
public:
    using FrameCallback = std::function<void(const VideoFrame &)>;
    using ErrorCallback = std::function<void(const std::string &)>;

    VideoStreamHandler();
    ~VideoStreamHandler();

    // 设置回调函数
    void setFrameCallback(FrameCallback callback);
    void setErrorCallback(ErrorCallback callback);

    // 开始播放流
    bool startStream(const std::string &url);

    // 停止播放流
    void stopStream();

    // 获取流状态
    bool isStreaming() const;

    // 获取流信息
    std::string getStreamInfo() const;

    // 获取帧统计信息
    int getFrameCount() const;
    double getCurrentFrameRate() const;

private:
    void streamThread();
    void cleanup();
    bool initializeFFmpeg();
    bool openInputStream(const std::string &url);
    bool setupDecoder();
    bool processFrame(AVFrame *frame);

    // FFmpeg 相关
    AVFormatContext *formatContext_;
    AVCodecContext *codecContext_;
    const AVCodec *codec_;
    AVFrame *frame_;
    AVPacket *packet_;

    int videoStreamIndex_;

    // 线程和状态管理
    std::thread streamThread_;
    std::atomic<bool> isStreaming_;
    std::atomic<bool> shouldStop_;
    std::mutex callbackMutex_;

    // 回调函数
    FrameCallback frameCallback_;
    ErrorCallback errorCallback_;

    // 流信息
    std::string streamUrl_;
    int frameWidth_;
    int frameHeight_;
    double frameRate_;

    // 帧统计
    std::atomic<int> frameCount_;
    std::atomic<double> currentFrameRate_;
};

#endif // VIDEO_STREAM_HANDLER_H
