#include "video_stream_handler.h"

#include <chrono>
#include <cstddef>
#include <thread>

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0x3200
#define LOG_TAG "VideoStreamHandler"

// Constructor: Initialize all member variables to a safe default state.
VideoStreamHandler::VideoStreamHandler()
    : formatContext_(nullptr), frame_(nullptr), packet_(nullptr), videoCodecContext_(nullptr), videoCodec_(nullptr),
      videoStreamIndex_(-1), audioCodecContext_(nullptr), audioCodec_(nullptr), swrContext_(nullptr),
      audioStreamIndex_(-1), audioBuffer_(nullptr), audioBufferSize_(0), audioRenderer_(nullptr), isStreaming_(false),
      shouldStop_(false), frameWidth_(0), frameHeight_(0), frameRate_(0.0), frameCount_(0), currentFrameRate_(0.0) {
    initializeFFmpeg();
}

// Destructor: Ensure stopStream is called to clean up resources.
VideoStreamHandler::~VideoStreamHandler() { stopStream(); }

// Initialize FFmpeg network capabilities.
bool VideoStreamHandler::initializeFFmpeg() {
    avformat_network_init();
    return true;
}

// Set the callback function for video frames.
void VideoStreamHandler::setFrameCallback(FrameCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    frameCallback_ = callback;
}

// Set the callback function for errors.
void VideoStreamHandler::setErrorCallback(ErrorCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    errorCallback_ = callback;
}



// Start the stream processing in a new thread.
bool VideoStreamHandler::startStream(const std::string &url) {
    std::lock_guard<std::mutex> lock(streamMutex_);
    if (isStreaming_) {
        OH_LOG_WARN(LOG_APP, "Stream is already running, start request ignored.");
        return false;
    }
    if (streamThread_.joinable()) {
        streamThread_.join();
    }
    streamUrl_ = url;
    shouldStop_ = false;
    frameCount_ = 0;
    currentFrameRate_ = 0.0;
    try {
        streamThread_ = std::thread(&VideoStreamHandler::streamThread, this);
        return true;
    } catch (const std::exception &e) {
        OH_LOG_ERROR(LOG_APP, "Failed to create stream thread: %{public}s", e.what());
        return false;
    }
}

// Stop the stream processing.
void VideoStreamHandler::stopStream() {
    std::lock_guard<std::mutex> lock(streamMutex_);
    if (!isStreaming_ && !streamThread_.joinable()) {
        return;
    }
    shouldStop_ = true;
    if (streamThread_.joinable()) {
        streamThread_.join();
    }
    cleanup();
    isStreaming_ = false;
}

bool VideoStreamHandler::isStreaming() const { return isStreaming_.load(); }

std::string VideoStreamHandler::getStreamInfo() const {
    if (!isStreaming_) {
        return "Not streaming";
    }
    return "URL: " + streamUrl_ + ", Resolution: " + std::to_string(frameWidth_) + "x" + std::to_string(frameHeight_) +
           ", FPS: " + std::to_string(frameRate_);
}

int VideoStreamHandler::getFrameCount() const { return frameCount_.load(); }

double VideoStreamHandler::getCurrentFrameRate() const { return currentFrameRate_.load(); }

// The main thread for stream processing.
void VideoStreamHandler::streamThread() {
    if (!openInputStream(streamUrl_)) {
        OH_LOG_ERROR(LOG_APP, "Failed to open input stream: %{public}s", streamUrl_.c_str());
        return;
    }
    if (videoStreamIndex_ != -1 && !setupVideoDecoder()) {
        OH_LOG_ERROR(LOG_APP, "Failed to setup video decoder");
        cleanup();
        return;
    }
    if (audioStreamIndex_ != -1 && !setupAudioDecoder()) {
        OH_LOG_ERROR(LOG_APP, "Failed to setup audio decoder");
        cleanup();
        return;
    }

    isStreaming_ = true;
    frame_ = av_frame_alloc();
    packet_ = av_packet_alloc();
    if (!frame_ || !packet_) {
        OH_LOG_ERROR(LOG_APP, "Failed to allocate frame or packet");
        isStreaming_ = false;
        cleanup();
        return;
    }

    while (!shouldStop_) {
        if (av_read_frame(formatContext_, packet_) >= 0) {
            if (packet_->stream_index == videoStreamIndex_) {
                if (avcodec_send_packet(videoCodecContext_, packet_) >= 0) {
                    while (avcodec_receive_frame(videoCodecContext_, frame_) >= 0) {
                        processVideoFrame(frame_);
                        frameCount_++;
                    }
                }
            } else if (packet_->stream_index == audioStreamIndex_) {
                if (avcodec_send_packet(audioCodecContext_, packet_) >= 0) {
                    while (avcodec_receive_frame(audioCodecContext_, frame_) >= 0) {
                        processAudioFrame(frame_);
                    }
                }
            }
            av_packet_unref(packet_);
        } else {
            OH_LOG_INFO(LOG_APP, "End of stream or read error, exiting loop.");
            break;
        }
    }
    isStreaming_ = false; // Set streaming to false before cleanup
}

// Open the input stream and find video/audio stream indices.
bool VideoStreamHandler::openInputStream(const std::string &url) {
    formatContext_ = avformat_alloc_context();
    AVDictionary *options = nullptr;
    av_dict_set(&options, "rtsp_transport", "tcp", 0);
    av_dict_set(&options, "stimeout", "5000000", 0);
    if (avformat_open_input(&formatContext_, url.c_str(), nullptr, &options) != 0) {
        OH_LOG_ERROR(LOG_APP, "avformat_open_input failed");
        av_dict_free(&options);
        return false;
    }
    av_dict_free(&options);
    if (avformat_find_stream_info(formatContext_, nullptr) < 0) {
        OH_LOG_ERROR(LOG_APP, "avformat_find_stream_info failed");
        return false;
    }
    videoStreamIndex_ = -1;
    audioStreamIndex_ = -1;
    // 遍历所有轨道
    for (unsigned int i = 0; i < formatContext_->nb_streams; i++) {
        if (formatContext_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex_ = i;
        } else if (formatContext_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioStreamIndex_ = i;
        }
    }
    if (videoStreamIndex_ == -1) {
        OH_LOG_ERROR(LOG_APP, "No video stream found");
        return false;
    }
    if (audioStreamIndex_ == -1) {
        OH_LOG_WARN(LOG_APP, "No audio stream found");
    }
    return true;
}

// Setup the video decoder.
bool VideoStreamHandler::setupVideoDecoder() {
    AVCodecParameters *codecpar = formatContext_->streams[videoStreamIndex_]->codecpar;
    videoCodec_ = avcodec_find_decoder(codecpar->codec_id);
    if (!videoCodec_) return false;
    videoCodecContext_ = avcodec_alloc_context3(videoCodec_);
    if (!videoCodecContext_ || avcodec_parameters_to_context(videoCodecContext_, codecpar) < 0) return false;
    if (avcodec_open2(videoCodecContext_, videoCodec_, nullptr) < 0) return false;
    frameWidth_ = videoCodecContext_->width;
    frameHeight_ = videoCodecContext_->height;
    frameRate_ = av_q2d(formatContext_->streams[videoStreamIndex_]->r_frame_rate);
    return true;
}

static OH_AudioData_Callback_Result NewAudioRendererOnWriteData(
    OH_AudioRenderer* renderer,
    void* userData,
    void* audioData,
    int32_t audioDataSize)
{
    
    
    
    return AUDIO_DATA_CALLBACK_RESULT_VALID;
}

bool VideoStreamHandler::setupAudioDecoder() {
    AVCodecParameters *codecpar = formatContext_->streams[audioStreamIndex_]->codecpar;
    audioCodec_ = avcodec_find_decoder(codecpar->codec_id);
    if (!audioCodec_) return false;
    audioCodecContext_ = avcodec_alloc_context3(audioCodec_);
    if (!audioCodecContext_ || avcodec_parameters_to_context(audioCodecContext_, codecpar) < 0) return false;
    if (avcodec_open2(audioCodecContext_, audioCodec_, nullptr) < 0) return false;
    
     // Setup SwrContext for resampling
    swr_alloc_set_opts2(&swrContext_, &out_ch_layout, AV_SAMPLE_FMT_S16, 48000,
                        &audioCodecContext_->ch_layout, audioCodecContext_->sample_fmt, audioCodecContext_->sample_rate, 0,
                        nullptr);
    swr_init(swrContext_);

    audioBufferSize_ = av_samples_get_buffer_size(nullptr, 2, 4096, AV_SAMPLE_FMT_S16, 1);
    audioBuffer_ = (uint8_t *)av_malloc(audioBufferSize_);
    buffer = (uint8_t *)av_malloc(audioBufferSize_);

    // Setup Audio Renderer
    OH_AudioStreamBuilder *builder;
    OH_AudioStream_Result result;
    // 设置renderer信息
    OH_AudioStreamBuilder_Create(&builder, AUDIOSTREAM_TYPE_RENDERER);
    OH_AudioStreamBuilder_SetSamplingRate(builder, 48000);                     // 设置采样率
    OH_AudioStreamBuilder_SetChannelCount(builder, 2);
    // OH_AudioStreamBuilder_SetChannelLayout(builder, CH_LAYOUT_STEREO);
    OH_AudioStreamBuilder_SetSampleFormat(builder, AUDIOSTREAM_SAMPLE_S16LE);  // 设置采样格式
    OH_AudioStreamBuilder_SetEncodingType(builder, AUDIOSTREAM_ENCODING_TYPE_RAW);

    OH_AudioStreamBuilder_SetRendererInfo(builder, AUDIOSTREAM_USAGE_MOVIE);
    
    OH_AudioStreamBuilder_SetFrameSizeInCallback(builder, audioBufferSize_);
    // 设置回调函数
    OH_AudioRenderer_Callbacks callbacks;
    callbacks.OH_AudioRenderer_OnError = error_callback;
    callbacks.OH_AudioRenderer_OnInterruptEvent = nullptr;
    callbacks.OH_AudioRenderer_OnStreamEvent = nullptr;

    OH_AudioStreamBuilder_SetRendererCallback(builder, callbacks, nullptr);
    OH_AudioRenderer_OnWriteDataCallback writeDataCb = write_callback;
    OH_AudioStreamBuilder_SetRendererWriteDataCallback(builder, writeDataCb, nullptr);

    OH_AudioStreamBuilder_GenerateRenderer(builder, &audioRenderer_);

    OH_AudioStreamBuilder_Destroy(builder);

   
    return true;
}



// Process a single video frame.
bool VideoStreamHandler::processVideoFrame(AVFrame *frame) {
    if (!frame->data[0]) return false;
    VideoFrame videoFrame;
    videoFrame.width = frameWidth_;
    videoFrame.height = frameHeight_;
    videoFrame.pts = frame->pts;
    videoFrame.data[0] = frame->data[0];
    videoFrame.data[1] = frame->data[1];
    videoFrame.data[2] = frame->data[2];
    videoFrame.linesize[0] = frame->linesize[0];
    videoFrame.linesize[1] = frame->linesize[1];
    videoFrame.linesize[2] = frame->linesize[2];
    std::lock_guard<std::mutex> lock(callbackMutex_);
    if (frameCallback_) {
        frameCallback_(videoFrame);
    }
    return true;
}

// Process a single audio frame.
bool VideoStreamHandler::processAudioFrame(AVFrame *frame) {
    // 将音频数据转换为设备可以播放的格式
    // OH_LOG_INFO(LOG_APP, "received audio frame");
    int out_samples = swr_convert(swrContext_, &audioBuffer_, audioBufferSize_,
                                (const uint8_t **)frame->data, frame->nb_samples);
    
    if (out_samples > 0) {
        int buffer_size = av_samples_get_buffer_size(nullptr, 2, out_samples, AV_SAMPLE_FMT_S16, 1);
        // todo 输入要播放的音频数据
        write_callback(audioRenderer_, audioBuffer_, buffer, audioBufferSize_);
//        OH_AudioRenderer_Write(audioRenderer_, audioBuffer_, buffer_size);
//        OH_AudioRenderer_OnWriteDataCallback(audioRenderer_, audioBuffer_, )
    }
    return true;
}

// Clean up all resources.
void VideoStreamHandler::cleanup() {
    if (packet_) av_packet_free(&packet_);
    if (frame_) av_frame_free(&frame_);
    if (videoCodecContext_) avcodec_free_context(&videoCodecContext_);
    if (audioCodecContext_) avcodec_free_context(&audioCodecContext_);
    if (formatContext_) avformat_close_input(&formatContext_);
    if (swrContext_) swr_free(&swrContext_);
    if (audioBuffer_) av_freep(&audioBuffer_);
    if (audioRenderer_) {
        OH_AudioRenderer_Stop(audioRenderer_);
        OH_AudioRenderer_Release(audioRenderer_);
        audioRenderer_ = nullptr;
    }
    videoStreamIndex_ = -1;
    audioStreamIndex_ = -1;
    packet_ = nullptr;
    frame_ = nullptr;
    videoCodecContext_ = nullptr;
    audioCodecContext_ = nullptr;
    formatContext_ = nullptr;
    swrContext_ = nullptr;
    audioBuffer_ = nullptr;
}