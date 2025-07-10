#include "video_stream_handler.h"
#include "hilog/log.h"
#include <chrono>
#include <thread>

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0x3200
#define LOG_TAG "VideoStreamHandler"

// 外部渲染函数声明
extern "C" bool RenderVideoFrame(const VideoFrame &frame);


VideoStreamHandler::VideoStreamHandler()
    : formatContext_(nullptr), codecContext_(nullptr), codec_(nullptr), frame_(nullptr),
      swsContext_(nullptr), packet_(nullptr), videoStreamIndex_(-1), isStreaming_(false), shouldStop_(false),
      frameWidth_(0), frameHeight_(0), frameRate_(0.0), frameCount_(0), currentFrameRate_(0.0) {
    initializeFFmpeg();
}

VideoStreamHandler::~VideoStreamHandler() {
    stopStream();
    cleanup();
}

bool VideoStreamHandler::initializeFFmpeg() {
    // FFmpeg 4.0+ 不再需要 av_register_all()
    avformat_network_init();
    return true;
}

void VideoStreamHandler::setFrameCallback(FrameCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    frameCallback_ = callback;
}

void VideoStreamHandler::setErrorCallback(ErrorCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    errorCallback_ = callback;
}

bool VideoStreamHandler::startStream(const std::string &url) {
    OH_LOG_INFO(LOG_APP, "VideoStreamHandler::startStream called with URL: %{public}s", url.c_str());

    if (isStreaming_) {
        OH_LOG_WARN(LOG_APP, "Stream already running");
        return false;
    }

    streamUrl_ = url;
    shouldStop_ = false;
    frameCount_ = 0;
    currentFrameRate_ = 0.0;

    // 在新线程中开始流处理
    try {
        streamThread_ = std::thread(&VideoStreamHandler::streamThread, this);
        OH_LOG_INFO(LOG_APP, "Stream thread started successfully");
        return true;
    } catch (const std::exception &e) {
        OH_LOG_ERROR(LOG_APP, "Failed to start stream thread: %{public}s", e.what());
        return false;
    }
}

void VideoStreamHandler::stopStream() {
    if (!isStreaming_) {
        return;
    }

    shouldStop_ = true;

    if (streamThread_.joinable()) {
        streamThread_.join();
    }

    cleanup();
    isStreaming_ = false;
}

bool VideoStreamHandler::isStreaming() const { return isStreaming_; }

std::string VideoStreamHandler::getStreamInfo() const {
    if (!isStreaming_) {
        return "Not streaming";
    }

    return "URL: " + streamUrl_ + ", Resolution: " + std::to_string(frameWidth_) + "x" + std::to_string(frameHeight_) +
           ", FPS: " + std::to_string(frameRate_);
}

void VideoStreamHandler::streamThread() {
    OH_LOG_INFO(LOG_APP, "Stream thread started for URL: %{public}s", streamUrl_.c_str());

    // 打开流
    if (!openInputStream(streamUrl_)) {
        OH_LOG_ERROR(LOG_APP, "Failed to open input stream: %{public}s", streamUrl_.c_str());
        std::lock_guard<std::mutex> lock(callbackMutex_);
        if (errorCallback_) {
            errorCallback_("Failed to open input stream: " + streamUrl_);
        }
        return;
    }

    OH_LOG_INFO(LOG_APP, "Input stream opened successfully");

    // 设置解码器
    if (!setupDecoder()) {
        OH_LOG_ERROR(LOG_APP, "Failed to setup decoder");
        std::lock_guard<std::mutex> lock(callbackMutex_);
        if (errorCallback_) {
            errorCallback_("Failed to setup decoder");
        }
        return;
    }

    OH_LOG_INFO(LOG_APP, "Decoder setup successfully");
    isStreaming_ = true;

    // 分配帧内存
    frame_ = av_frame_alloc();
    packet_ = av_packet_alloc();

    if (!frame_ || !packet_) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        if (errorCallback_) {
            errorCallback_("Failed to allocate frame memory");
        }
        return;
    }
    
    OH_LOG_INFO(LOG_APP, "Starting main decode loop...");
    int frameCount = 0;

    // 主循环
    while (!shouldStop_) {
        int ret = av_read_frame(formatContext_, packet_);
        if (ret >= 0) {
            if (packet_->stream_index == videoStreamIndex_) {
                // 发送数据包到解码器
                if (avcodec_send_packet(codecContext_, packet_) >= 0) {
                    // 接收解码后的帧
                    while (avcodec_receive_frame(codecContext_, frame_) >= 0) {
                        processFrame(frame_);
                        frameCount_++;
                        if (frameCount_ % 30 == 0) { // 每30帧输出一次日志
                            OH_LOG_INFO(LOG_APP, "Processed %{public}d frames", frameCount_.load());
                        }
                    }
                }
            }
            av_packet_unref(packet_);
        } else {
            // 读取失败，可能是流结束或网络错误
            if (ret == AVERROR_EOF) {
                OH_LOG_INFO(LOG_APP, "End of stream reached");
                break;
            } else {
                char error_str[AV_ERROR_MAX_STRING_SIZE];
                av_strerror(ret, error_str, AV_ERROR_MAX_STRING_SIZE);
                OH_LOG_WARN(LOG_APP, "av_read_frame failed: %{public}s", error_str);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    OH_LOG_INFO(LOG_APP, "Main loop ended, processed %{public}d frames total", frameCount_.load());

    // 更新当前帧率
    currentFrameRate_ = frameRate_;
    
    cleanup();
    isStreaming_ = false;
}

bool VideoStreamHandler::openInputStream(const std::string &url) {
    OH_LOG_INFO(LOG_APP, "Opening input stream: %{public}s", url.c_str());

    formatContext_ = avformat_alloc_context();
    if (!formatContext_) {
        OH_LOG_ERROR(LOG_APP, "Failed to allocate format context");
        return false;
    }

    // 设置选项用于RTSP/RTP
    AVDictionary *options = nullptr;
    av_dict_set(&options, "rtsp_transport", "tcp", 0);
    av_dict_set(&options, "stimeout", "5000000", 0); // 5秒超时
    av_dict_set(&options, "user_agent", "FFmpeg/VideoStream", 0);
    av_dict_set(&options, "max_delay", "500000", 0); // 最大延迟500ms

    // 打开流
    OH_LOG_INFO(LOG_APP, "Attempting to open input with avformat_open_input...");
    int ret = avformat_open_input(&formatContext_, url.c_str(), nullptr, &options);
    if (ret != 0) {
        char error_str[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, error_str, AV_ERROR_MAX_STRING_SIZE);
        OH_LOG_ERROR(LOG_APP, "avformat_open_input failed with error code %{public}d: %{public}s", ret, error_str);
        av_dict_free(&options);
        avformat_free_context(formatContext_);
        formatContext_ = nullptr;
        return false;
    }

    av_dict_free(&options);
    OH_LOG_INFO(LOG_APP, "avformat_open_input succeeded");

    // 寻找流信息
    OH_LOG_INFO(LOG_APP, "Finding stream info...");
    ret = avformat_find_stream_info(formatContext_, nullptr);
    if (ret < 0) {
        char error_str[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, error_str, AV_ERROR_MAX_STRING_SIZE);
        OH_LOG_ERROR(LOG_APP, "avformat_find_stream_info failed with error code %{public}d: %{public}s", ret,
                     error_str);
        return false;
    }

    OH_LOG_INFO(LOG_APP, "Stream info found, total streams: %{public}u", formatContext_->nb_streams);

    // 查找视频流
    videoStreamIndex_ = -1;
    for (unsigned int i = 0; i < formatContext_->nb_streams; i++) {
        OH_LOG_INFO(LOG_APP, "Stream %{public}u: codec_type = %{public}d", i,
                    formatContext_->streams[i]->codecpar->codec_type);
        if (formatContext_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex_ = i;
            OH_LOG_INFO(LOG_APP, "Found video stream at index %{public}d", videoStreamIndex_);
            break;
        }
    }

    if (videoStreamIndex_ == -1) {
        OH_LOG_ERROR(LOG_APP, "No video stream found in the input");
        return false;
    }

    OH_LOG_INFO(LOG_APP, "Input stream opened successfully, video stream index: %{public}d", videoStreamIndex_);
    return true;
}

bool VideoStreamHandler::setupDecoder() {
    OH_LOG_INFO(LOG_APP, "Setting up decoder for video stream %{public}d", videoStreamIndex_);

    AVCodecParameters *codecpar = formatContext_->streams[videoStreamIndex_]->codecpar;
    OH_LOG_INFO(LOG_APP, "Codec ID: %{public}d, width: %{public}d, height: %{public}d", codecpar->codec_id,
                codecpar->width, codecpar->height);

    // 查找解码器
    codec_ = avcodec_find_decoder(codecpar->codec_id);
    if (!codec_) {
        OH_LOG_ERROR(LOG_APP, "Decoder not found for codec ID: %{public}d", codecpar->codec_id);
        return false;
    }

    OH_LOG_INFO(LOG_APP, "Found decoder: %{public}s", codec_->name);

    // 分配解码器上下文
    codecContext_ = avcodec_alloc_context3(codec_);
    if (!codecContext_) {
        OH_LOG_ERROR(LOG_APP, "Failed to allocate codec context");
        return false;
    }

    // 从流参数复制到解码器上下文
    int ret = avcodec_parameters_to_context(codecContext_, codecpar);
    if (ret < 0) {
        char error_str[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, error_str, AV_ERROR_MAX_STRING_SIZE);
        OH_LOG_ERROR(LOG_APP, "Failed to copy codec parameters: %{public}s", error_str);
        return false;
    }

    // 打开解码器
    ret = avcodec_open2(codecContext_, codec_, nullptr);
    if (ret < 0) {
        char error_str[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, error_str, AV_ERROR_MAX_STRING_SIZE);
        OH_LOG_ERROR(LOG_APP, "Failed to open codec: %{public}s", error_str);
        return false;
    }

    frameWidth_ = codecContext_->width;
    frameHeight_ = codecContext_->height;
    OH_LOG_INFO(LOG_APP, "Decoder opened successfully, frame size: %{public}dx%{public}d", frameWidth_, frameHeight_);

    // 计算帧率
    AVRational timeBase = formatContext_->streams[videoStreamIndex_]->time_base;
    AVRational frameRate = formatContext_->streams[videoStreamIndex_]->r_frame_rate;
    if (frameRate.num > 0 && frameRate.den > 0) {
        frameRate_ = av_q2d(frameRate);
        OH_LOG_INFO(LOG_APP, "Frame rate: %{public}f fps", frameRate_);
    } else {
        OH_LOG_WARN(LOG_APP, "Frame rate information not available");
    }

    return true;
}

bool VideoStreamHandler::processFrame(AVFrame *frame) {
    if (!swsContext_) {
        return false;
    }

    // 创建VideoFrame结构
    VideoFrame videoFrame;
    videoFrame.width = frameWidth_;
    videoFrame.height = frameHeight_;
    videoFrame.pts = frame->pts;

    // 调用回调函数
    std::lock_guard<std::mutex> lock(callbackMutex_);
    if (frameCallback_) {
        frameCallback_(videoFrame);
    }

    return true;
}

void VideoStreamHandler::cleanup() {
    if (swsContext_) {
        sws_freeContext(swsContext_);
        swsContext_ = nullptr;
    }

    if (packet_) {
        av_packet_free(&packet_);
    }

    if (frame_) {
        av_frame_free(&frame_);
    }

    if (codecContext_) {
        avcodec_free_context(&codecContext_);
    }

    if (formatContext_) {
        avformat_close_input(&formatContext_);
    }

    videoStreamIndex_ = -1;
}

int VideoStreamHandler::getFrameCount() const { return frameCount_.load(); }

double VideoStreamHandler::getCurrentFrameRate() const { return currentFrameRate_.load(); }
