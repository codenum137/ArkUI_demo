#include "common/common.h"
#include "hilog/log.h" // 添加日志头文件
#include "manager/plugin_manager.h"
#include "napi/native_api.h"
#include "render/plugin_render.h" // 需要VideoRenderer的完整定义
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

// 开始视频流
static napi_value StartVideoStream(napi_env env, napi_callback_info info) {
    OH_LOG_INFO(LOG_APP, "=== StartVideoStream called ===");

    size_t argc = 2;
    napi_value args[2];

    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    OH_LOG_INFO(LOG_APP, "Got callback info, argc = %{public}zu", argc);

    if (argc < 2) {
        OH_LOG_ERROR(LOG_APP, "Missing parameters: expected URL and surfaceId");
        napi_throw_error(env, nullptr, "Expected 2 arguments: url and surfaceId");
        return nullptr;
    }

    // 获取URL参数
    size_t url_length;
    napi_get_value_string_utf8(env, args[0], nullptr, 0, &url_length);
    OH_LOG_INFO(LOG_APP, "URL length: %{public}zu", url_length);

    std::string url(url_length, '\0');
    napi_get_value_string_utf8(env, args[0], &url[0], url_length + 1, &url_length);
    OH_LOG_INFO(LOG_APP, "Starting video stream with URL: %{public}s", url.c_str());

    // 获取surfaceId参数
    int64_t surfaceId = 0;
    bool lossless = true;
    if (napi_ok != napi_get_value_bigint_int64(env, args[1], &surfaceId, &lossless)) {
        napi_throw_error(env, nullptr, "Failed to get surfaceId");
        return nullptr;
    }

    OH_LOG_INFO(LOG_APP, "StartVideoStream: surfaceId=%{public}lld", static_cast<long long>(surfaceId));

    // 获取视频渲染器
    auto videoRenderer = PluginManager::GetVideoRenderer(surfaceId);
    if (!videoRenderer) {
        OH_LOG_ERROR(LOG_APP, "VideoRenderer not found for surfaceId: %{public}lld", static_cast<long long>(surfaceId));
        napi_throw_error(env, nullptr, "VideoRenderer not found. Call setSurfaceId first.");
        return nullptr;
    }

    // 创建视频流处理器
    OH_LOG_INFO(LOG_APP, "Creating VideoStreamHandler...");
    auto handler = std::make_shared<VideoStreamHandler>();
    OH_LOG_INFO(LOG_APP, "VideoStreamHandler created successfully");

    // 设置帧回调，将帧推送到渲染队列而不是直接渲染
    handler->setFrameCallback([videoRenderer](const VideoFrame &frame) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "NAPI",
                     "Frame received in callback: %{public}dx%{public}d, pts=%{public}ld", frame.width, frame.height,
                     static_cast<long>(frame.pts));

        // 将帧推送到渲染队列（生产者）
        if (!videoRenderer->pushFrame(frame)) {
            OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "NAPI", "Failed to push frame to render queue");
        } else {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "NAPI", "Frame pushed to render queue successfully");
        }
    });

    handler->setErrorCallback(
        [](const std::string &error) { OH_LOG_ERROR(LOG_APP, "Stream error: %{public}s", error.c_str()); });

    OH_LOG_INFO(LOG_APP, "Callbacks set, starting stream...");

    // 开始流
    bool success = handler->startStream(url);
    OH_LOG_INFO(LOG_APP, "Stream start result: %{public}s", success ? "SUCCESS" : "FAILED");

    if (success) {
        g_streamHandlers[url] = handler;
        OH_LOG_INFO(LOG_APP, "Added handler to global map, total handlers: %{public}zu", g_streamHandlers.size());
        OH_LOG_INFO(LOG_APP, "Video stream connected to renderer successfully");
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

    OH_LOG_INFO(LOG_APP, "GetStreamStatus called for URL: %{public}s", url.c_str());
    OH_LOG_INFO(LOG_APP, "Total handlers in map: %{public}zu", g_streamHandlers.size());

    napi_value result;
    napi_create_object(env, &result);

    auto it = g_streamHandlers.find(url);
    if (it != g_streamHandlers.end()) {
        bool streaming = it->second->isStreaming();
        OH_LOG_INFO(LOG_APP, "Handler found, isStreaming: %{public}s", streaming ? "true" : "false");

        napi_value isStreaming;
        napi_get_boolean(env, streaming, &isStreaming);
        napi_set_named_property(env, result, "isStreaming", isStreaming);

        napi_value info;
        std::string streamInfo = it->second->getStreamInfo();
        OH_LOG_INFO(LOG_APP, "Stream info: %{public}s", streamInfo.c_str());
        napi_create_string_utf8(env, streamInfo.c_str(), NAPI_AUTO_LENGTH, &info);
        napi_set_named_property(env, result, "info", info);
    } else {
        OH_LOG_WARN(LOG_APP, "Handler not found for URL: %{public}s", url.c_str());
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

// 更新视频surface大小
static napi_value UpdateVideoSurfaceSize(napi_env env, napi_callback_info info) {
    size_t argc = 3;
    napi_value args[3];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc != 3) {
        napi_throw_error(env, nullptr, "Expected 3 arguments: surfaceId, width, height");
        return nullptr;
    }

    // 获取surfaceId
    int64_t surfaceId = 0;
    bool lossless = true;
    if (napi_ok != napi_get_value_bigint_int64(env, args[0], &surfaceId, &lossless)) {
        napi_throw_error(env, nullptr, "Failed to get surfaceId");
        return nullptr;
    }

    // 获取宽高
    double width, height;
    if (napi_ok != napi_get_value_double(env, args[1], &width)) {
        napi_throw_error(env, nullptr, "Failed to get width");
        return nullptr;
    }
    if (napi_ok != napi_get_value_double(env, args[2], &height)) {
        napi_throw_error(env, nullptr, "Failed to get height");
        return nullptr;
    }

    // 获取视频渲染器并更新大小
    auto videoRenderer = PluginManager::GetVideoRenderer(surfaceId);
    if (videoRenderer) {
        videoRenderer->UpdateSize(static_cast<int>(width), static_cast<int>(height));
    }

    napi_value result;
    napi_get_boolean(env, true, &result);
    return result;
}

// 渲染单个测试帧
static napi_value RenderSingleTestFrame(napi_env env, napi_callback_info info) {
    OH_LOG_INFO(LOG_APP, "=== RenderSingleTestFrame called ===");

    size_t argc = 5; // surfaceId + 4个颜色值
    napi_value args[5];

    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    OH_LOG_INFO(LOG_APP, "Got callback info, argc = %{public}zu", argc);

    if (argc < 1) {
        OH_LOG_ERROR(LOG_APP, "Missing parameters: at least surfaceId is required");
        napi_throw_error(env, nullptr, "At least surfaceId parameter is required");
        return nullptr;
    }

    // 获取surfaceId参数
    int64_t surfaceId = 0;
    bool lossless = true;
    if (napi_ok != napi_get_value_bigint_int64(env, args[0], &surfaceId, &lossless)) {
        napi_throw_error(env, nullptr, "Failed to get surfaceId");
        return nullptr;
    }

    // 获取颜色参数，如果没有提供则使用默认值
    float r = 0.2f, g = 0.6f, b = 0.9f, a = 1.0f; // 默认蓝色
    
    if (argc >= 2) {
        double value;
        napi_get_value_double(env, args[1], &value);
        r = static_cast<float>(value);
    }
    if (argc >= 3) {
        double value;
        napi_get_value_double(env, args[2], &value);
        g = static_cast<float>(value);
    }
    if (argc >= 4) {
        double value;
        napi_get_value_double(env, args[3], &value);
        b = static_cast<float>(value);
    }
    if (argc >= 5) {
        double value;
        napi_get_value_double(env, args[4], &value);
        a = static_cast<float>(value);
    }

    OH_LOG_INFO(LOG_APP, "RenderSingleTestFrame: surfaceId=%{public}lld, color(%.2f, %.2f, %.2f, %.2f)", 
                static_cast<long long>(surfaceId), r, g, b, a);

    // 获取视频渲染器
    auto videoRenderer = PluginManager::GetVideoRenderer(surfaceId);
    if (!videoRenderer) {
        OH_LOG_ERROR(LOG_APP, "VideoRenderer not found for surfaceId: %{public}lld", static_cast<long long>(surfaceId));
        napi_throw_error(env, nullptr, "VideoRenderer not found. Call setSurfaceId first.");
        return nullptr;
    }

    // 渲染单个测试帧
    bool success = videoRenderer->renderSingleTestFrame(r, g, b, a);
    OH_LOG_INFO(LOG_APP, "RenderSingleTestFrame result: %{public}s", success ? "SUCCESS" : "FAILED");

    napi_value result;
    napi_get_boolean(env, success, &result);
    return result;
}

EXTERN_C_START
static napi_value Init(napi_env env, napi_value exports) {
    napi_property_descriptor desc[] = {
        {"startVideoStream", nullptr, StartVideoStream, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"stopVideoStream", nullptr, StopVideoStream, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"getStreamStatus", nullptr, GetStreamStatus, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"getFrameStats", nullptr, GetFrameStats, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"updateVideoSurfaceSize", nullptr, UpdateVideoSurfaceSize, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"renderSingleTestFrame", nullptr, RenderSingleTestFrame, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"setSurfaceId", nullptr, PluginManager::SetSurfaceId, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"changeSurface", nullptr, PluginManager::ChangeSurface, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"getXComponentStatus", nullptr, PluginManager::GetXComponentStatus, nullptr, nullptr, nullptr, napi_default,
         nullptr},
        {"destroySurface", nullptr, PluginManager::DestroySurface, nullptr, nullptr, nullptr, napi_default, nullptr}};
    napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
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
