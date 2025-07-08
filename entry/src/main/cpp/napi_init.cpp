#include "hilog/log.h" // 添加日志头文件
#include "napi/native_api.h"
#include "video_stream_handler.h"
#include <ace/xcomponent/native_interface_xcomponent.h>
#include <cstring> // 添加memset支持
#include <map>
#include <memory>
#include <string>

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0x3200         // 自定义domain
#define LOG_TAG "VideoStreamNAPI" // 日志标签

// 全局视频流处理器映射
static std::map<std::string, std::shared_ptr<VideoStreamHandler>> g_streamHandlers;

// 帧数据回调到JS
static napi_value CreateFrameData(napi_env env, const VideoFrame &frame) {
    napi_value frameObj;
    napi_create_object(env, &frameObj);

    // 创建width属性
    napi_value width;
    napi_create_int32(env, frame.width, &width);
    napi_set_named_property(env, frameObj, "width", width);

    // 创建height属性
    napi_value height;
    napi_create_int32(env, frame.height, &height);
    napi_set_named_property(env, frameObj, "height", height);

    // 创建pts属性
    napi_value pts;
    napi_create_int64(env, frame.pts, &pts);
    napi_set_named_property(env, frameObj, "pts", pts);

    // 创建ArrayBuffer存储帧数据
    void *buffer_data;
    size_t buffer_size = frame.linesize * frame.height;
    napi_value arrayBuffer;
    napi_create_arraybuffer(env, buffer_size, &buffer_data, &arrayBuffer);

    // 复制帧数据
    memcpy(buffer_data, frame.data, buffer_size);
    napi_set_named_property(env, frameObj, "data", arrayBuffer);

    return frameObj;
}

// 开始视频流
static napi_value StartVideoStream(napi_env env, napi_callback_info info) {
    OH_LOG_INFO(LOG_APP, "=== StartVideoStream called ===");

    size_t argc = 1;
    napi_value args[1];

    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    OH_LOG_INFO(LOG_APP, "Got callback info, argc = %{public}zu", argc);

    if (argc < 1) {
        OH_LOG_ERROR(LOG_APP, "Missing stream URL parameter");
        napi_throw_error(env, nullptr, "Missing stream URL parameter");
        return nullptr;
    }

    // 获取URL参数
    size_t url_length;
    napi_get_value_string_utf8(env, args[0], nullptr, 0, &url_length);
    OH_LOG_INFO(LOG_APP, "URL length: %{public}zu", url_length);

    std::string url(url_length, '\0');
    napi_get_value_string_utf8(env, args[0], &url[0], url_length + 1, &url_length);
    OH_LOG_INFO(LOG_APP, "Starting video stream with URL: %{public}s", url.c_str());

    // 创建视频流处理器
    OH_LOG_INFO(LOG_APP, "Creating VideoStreamHandler...");
    auto handler = std::make_shared<VideoStreamHandler>();
    OH_LOG_INFO(LOG_APP, "VideoStreamHandler created successfully");

    // 设置回调函数
    handler->setFrameCallback([env](const VideoFrame &frame) {
        OH_LOG_INFO(LOG_APP, "Frame received: %{public}dx%{public}d, pts=%{public}ld", frame.width, frame.height,
                    static_cast<long>(frame.pts));
    });

    handler->setErrorCallback(
        [env](const std::string &error) { OH_LOG_ERROR(LOG_APP, "Stream error: %{public}s", error.c_str()); });

    OH_LOG_INFO(LOG_APP, "Callbacks set, starting stream...");

    // 开始流
    bool success = handler->startStream(url);
    OH_LOG_INFO(LOG_APP, "Stream start result: %{public}s", success ? "SUCCESS" : "FAILED");

    if (success) {
        g_streamHandlers[url] = handler;
        OH_LOG_INFO(LOG_APP, "Added handler to global map, total handlers: %{public}zu", g_streamHandlers.size());
    } else {
        OH_LOG_ERROR(LOG_APP, "Failed to start stream for URL: %{public}s", url.c_str());
    }

    napi_value result;
    napi_create_object(env, &result);

    napi_value successValue;
    napi_get_boolean(env, success, &successValue);
    napi_set_named_property(env, result, "success", successValue);

    napi_value urlValue;
    napi_create_string_utf8(env, url.c_str(), NAPI_AUTO_LENGTH, &urlValue);
    napi_set_named_property(env, result, "url", urlValue);

    OH_LOG_INFO(LOG_APP, "=== StartVideoStream completed ===");
    return result;
}

// 停止视频流
static napi_value StopVideoStream(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];

    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 1) {
        napi_throw_error(env, nullptr, "Missing stream URL parameter");
        return nullptr;
    }

    // 获取URL参数
    size_t url_length;
    napi_get_value_string_utf8(env, args[0], nullptr, 0, &url_length);

    std::string url(url_length, '\0');
    napi_get_value_string_utf8(env, args[0], &url[0], url_length + 1, &url_length);

    bool success = false;
    auto it = g_streamHandlers.find(url);
    if (it != g_streamHandlers.end()) {
        it->second->stopStream();
        g_streamHandlers.erase(it);
        success = true;
    }

    napi_value result;
    napi_get_boolean(env, success, &result);

    return result;
}

// 获取流状态
static napi_value GetStreamStatus(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];

    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 1) {
        napi_throw_error(env, nullptr, "Missing stream URL parameter");
        return nullptr;
    }

    // 获取URL参数
    size_t url_length;
    napi_get_value_string_utf8(env, args[0], nullptr, 0, &url_length);

    std::string url(url_length, '\0');
    napi_get_value_string_utf8(env, args[0], &url[0], url_length + 1, &url_length);

    napi_value result;
    napi_create_object(env, &result);

    auto it = g_streamHandlers.find(url);
    if (it != g_streamHandlers.end()) {
        napi_value isStreaming;
        napi_get_boolean(env, it->second->isStreaming(), &isStreaming);
        napi_set_named_property(env, result, "isStreaming", isStreaming);

        napi_value info;
        std::string streamInfo = it->second->getStreamInfo();
        napi_create_string_utf8(env, streamInfo.c_str(), NAPI_AUTO_LENGTH, &info);
        napi_set_named_property(env, result, "info", info);
    } else {
        napi_value isStreaming;
        napi_get_boolean(env, false, &isStreaming);
        napi_set_named_property(env, result, "isStreaming", isStreaming);

        napi_value info;
        napi_create_string_utf8(env, "Stream not found", NAPI_AUTO_LENGTH, &info);
        napi_set_named_property(env, result, "info", info);
    }

    return result;
}

// 获取视频帧统计信息
static napi_value GetFrameStats(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc != 1) {
        napi_throw_error(env, nullptr, "Expected 1 argument");
        return nullptr;
    }

    // 获取URL
    size_t url_length;
    napi_get_value_string_utf8(env, args[0], nullptr, 0, &url_length);
    std::string url(url_length, '\0');
    napi_get_value_string_utf8(env, args[0], &url[0], url_length + 1, &url_length);

    auto it = g_streamHandlers.find(url);
    if (it == g_streamHandlers.end()) {
        napi_value result;
        napi_create_object(env, &result);

        napi_value frameCount;
        napi_create_int32(env, 0, &frameCount);
        napi_set_named_property(env, result, "frameCount", frameCount);

        napi_value frameRate;
        napi_create_double(env, 0.0, &frameRate);
        napi_set_named_property(env, result, "frameRate", frameRate);

        return result;
    }

    auto handler = it->second;
    napi_value result;
    napi_create_object(env, &result);

    napi_value frameCount;
    napi_create_int32(env, handler->getFrameCount(), &frameCount);
    napi_set_named_property(env, result, "frameCount", frameCount);

    napi_value frameRate;
    napi_create_double(env, handler->getCurrentFrameRate(), &frameRate);
    napi_set_named_property(env, result, "frameRate", frameRate);

    return result;
}

EXTERN_C_START
static napi_value Init(napi_env env, napi_value exports) {
    napi_property_descriptor desc[] = {
        {"startVideoStream", nullptr, StartVideoStream, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"stopVideoStream", nullptr, StopVideoStream, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"getStreamStatus", nullptr, GetStreamStatus, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"getFrameStats", nullptr, GetFrameStats, nullptr, nullptr, nullptr, napi_default, nullptr}};
    napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);

    // 注册XComponent回调
    napi_value exportInstance = nullptr;
    OH_NativeXComponent *nativeXComponent = nullptr;
    int32_t ret;
    char idStr[OH_XCOMPONENT_ID_LEN_MAX + 1] = {};
    uint64_t idSize = OH_XCOMPONENT_ID_LEN_MAX + 1;
    

    return exports;
}
EXTERN_C_END

static napi_module demoModule = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = nullptr,
    .nm_register_func = Init,
    .nm_modname = "entry",
    .nm_priv = ((void *)0),
    .reserved = {0},
};

extern "C" __attribute__((constructor)) void RegisterEntryModule(void) { napi_module_register(&demoModule); }
