#ifndef VIDEO_STREAM_HANDLER_H
#define VIDEO_STREAM_HANDLER_H

#include "hilog/log.h"
#include "libavutil/channel_layout.h"
#include <ohaudio/native_audiostreambuilder.h>
#include <ohaudio/native_audiorenderer.h>
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
#include <libswresample/swresample.h>
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
    using AudioCallback = std::function<OH_AudioData_Callback_Result(OH_AudioRenderer *, void *, void *, int32_t)>;

    VideoStreamHandler();
    ~VideoStreamHandler();

    void setFrameCallback(FrameCallback callback);
    void setErrorCallback(ErrorCallback callback);

    bool startStream(const std::string &url);
    void stopStream();

    bool isStreaming() const;
    std::string getStreamInfo() const;
    int getFrameCount() const;
    double getCurrentFrameRate() const;

private:
    void streamThread();
    void cleanup();
    bool initializeFFmpeg();
    bool openInputStream(const std::string &url);
    bool setupVideoDecoder();
    bool setupAudioDecoder();
    bool processVideoFrame(AVFrame *frame);
    bool processAudioFrame(AVFrame *frame);

    // FFmpeg - General
    AVFormatContext *formatContext_;
    AVFrame *frame_;
    AVPacket *packet_;

    // FFmpeg - Video
    AVCodecContext *videoCodecContext_;
    const AVCodec *videoCodec_;
    int videoStreamIndex_;

    // FFmpeg - Audio
    AVCodecContext *audioCodecContext_;
    const AVChannelLayout out_ch_layout = AV_CHANNEL_LAYOUT_2_2;
    const AVCodec *audioCodec_;
    SwrContext *swrContext_;
    int audioStreamIndex_;
    uint8_t *audioBuffer_;
    uint8_t *buffer;
    int audioBufferSize_;
    
    OH_AudioRenderer *audioRenderer_;

    // Thread and State Management
    std::thread streamThread_;
    std::atomic<bool> isStreaming_;
    std::atomic<bool> shouldStop_;
    std::mutex callbackMutex_;
    std::mutex streamMutex_;

    // Callbacks
    FrameCallback frameCallback_;
    ErrorCallback errorCallback_;
    AudioCallback audioCallback_;
     static OH_AudioData_Callback_Result write_callback(OH_AudioRenderer *render, void* userData, void *buffer, int32_t bufferLen){
        memcpy(buffer, userData, bufferLen);
        return AUDIO_DATA_CALLBACK_RESULT_VALID;
    }
    static int32_t error_callback(OH_AudioRenderer* renderer, void* userData, OH_AudioStream_Result error){
    // 根据error表示的音频异常信息，做出相应的处理
    OH_LOG_ERROR(LOG_APP, "Audio process error : %d ", error);
    return 0;
    }

    // Stream Info
    std::string streamUrl_;
    int frameWidth_;
    int frameHeight_;
    double frameRate_;

    // Frame Stats
    std::atomic<int> frameCount_;
    std::atomic<double> currentFrameRate_;
};

#endif // VIDEO_STREAM_HANDLER_H
