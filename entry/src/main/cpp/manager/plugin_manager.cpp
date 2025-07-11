//
// Created on 2025/7/10.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#include "plugin_manager.h"
#include "../common/common.h"
#include "../render/plugin_render.h"
#include <ace/xcomponent/native_interface_xcomponent.h>
#include <cstdint>
#include <hilog/log.h>
#include <native_drawing/drawing_text_typography.h>
#include <native_window/external_window.h>
#include <string>


namespace {
int64_t ParseId(napi_env env, napi_callback_info info) {
    if ((env == nullptr) || (info == nullptr)) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "ParseId", "env or info is null");
        return -1;
    }
    size_t argc = 1;
    napi_value args[1] = {nullptr};
    if (napi_ok != napi_get_cb_info(env, info, &argc, args, nullptr, nullptr)) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "ParseId", "GetContext napi_get_cb_info failed");
        return -1;
    }
    int64_t value = 0;
    bool lossless = true;
    if (napi_ok != napi_get_value_bigint_int64(env, args[0], &value, &lossless)) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "ParseId", "Get value failed");
        return -1;
    }
    return value;
}
} // namespace

std::unordered_map<int64_t, VideoStreamNS::VideoRenderer *> PluginManager::videoRendererMap_;
std::unordered_map<int64_t, OHNativeWindow *> PluginManager::windowMap_;

PluginManager::~PluginManager() {
    OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "PluginManager", "~PluginManager");
    for (auto iter = videoRendererMap_.begin(); iter != videoRendererMap_.end(); ++iter) {
        if (iter->second != nullptr) {
            delete iter->second;
            iter->second = nullptr;
        }
    }
    videoRendererMap_.clear();
    for (auto iter = windowMap_.begin(); iter != windowMap_.end(); ++iter) {
        if (iter->second != nullptr) {
            delete iter->second;
            iter->second = nullptr;
        }
    }
    windowMap_.clear();
}

VideoStreamNS::VideoRenderer *PluginManager::GetVideoRenderer(int64_t surfaceId) {
    if (videoRendererMap_.find(surfaceId) != videoRendererMap_.end()) {
        return videoRendererMap_[surfaceId];
    }
    return nullptr;
}

napi_value PluginManager::SetSurfaceId(napi_env env, napi_callback_info info) {
    int64_t surfaceId = ParseId(env, info);
    OHNativeWindow *nativeWindow;
    VideoStreamNS::VideoRenderer *videoRenderer = nullptr;

    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "PluginManager", "SetSurfaceId: %{public}lld",
                 static_cast<long long>(surfaceId));

    if (windowMap_.find(surfaceId) == windowMap_.end()) {
        OH_NativeWindow_CreateNativeWindowFromSurfaceId(surfaceId, &nativeWindow);
        windowMap_[surfaceId] = nativeWindow;
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "PluginManager", "Created new native window");
    } else {
        nativeWindow = windowMap_[surfaceId];
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "PluginManager", "Using existing native window");
    }

    if (videoRendererMap_.find(surfaceId) == videoRendererMap_.end()) {
        videoRenderer = new VideoStreamNS::VideoRenderer(surfaceId);
        videoRendererMap_[surfaceId] = videoRenderer;
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "PluginManager", "Created new VideoRenderer");
    } else {
        videoRenderer = videoRendererMap_[surfaceId];
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "PluginManager", "Using existing VideoRenderer");
    }

    videoRenderer->InitNativeWindow(nativeWindow);

    // 设置默认尺寸，后续会通过ChangeSurface更新
    videoRenderer->UpdateSize(640, 480);
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "PluginManager",
                 "VideoRenderer initialized with default size 640x480");

    return nullptr;
}

napi_value PluginManager::DestroySurface(napi_env env, napi_callback_info info) {
    int64_t surfaceId = ParseId(env, info);
    auto videoRendererMapIter = videoRendererMap_.find(surfaceId);
    if (videoRendererMapIter != videoRendererMap_.end()) {
        delete videoRendererMapIter->second;
        videoRendererMap_.erase(videoRendererMapIter);
    }
    auto windowMapIter = windowMap_.find(surfaceId);
    if (windowMapIter != windowMap_.end()) {
        OH_NativeWindow_DestroyNativeWindow(windowMapIter->second);
        windowMap_.erase(windowMapIter);
    }
    return nullptr;
}

napi_value PluginManager::ChangeSurface(napi_env env, napi_callback_info info) {
    if ((env == nullptr) || (info == nullptr)) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "PluginManager",
                     "ChangeSurface: OnLoad env or info is null");
        return nullptr;
    }
    int64_t surfaceId = 0;
    size_t argc = 3;
    napi_value args[3] = {nullptr};

    if (napi_ok != napi_get_cb_info(env, info, &argc, args, nullptr, nullptr)) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "PluginManager",
                     "ChangeSurface: GetContext napi_get_cb_info failed");
    }
    bool lossless = true;
    int index = 0;
    if (napi_ok != napi_get_value_bigint_int64(env, args[index++], &surfaceId, &lossless)) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "PluginManager", "ChangeSurface: Get value failed");
    }
    double width;
    if (napi_ok != napi_get_value_double(env, args[index++], &width)) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "PluginManager", "ChangeSurface: Get width failed");
    }
    double height;
    if (napi_ok != napi_get_value_double(env, args[index++], &height)) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "PluginManager", "ChangeSurface: Get height failed");
    }
    auto videoRenderer = GetVideoRenderer(surfaceId);
    if (videoRenderer == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "PluginManager", "ChangeSurface: Get videoRenderer failed");
        return nullptr;
    }
    videoRenderer->UpdateSize(static_cast<int>(width), static_cast<int>(height));
    return nullptr;
}

napi_value PluginManager::GetXComponentStatus(napi_env env, napi_callback_info info) {
    int64_t surfaceId = ParseId(env, info);
    auto videoRenderer = GetVideoRenderer(surfaceId);
    if (videoRenderer == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "PluginManager",
                     "GetXComponentStatus: Get videoRenderer failed");
        return nullptr;
    }
    napi_value isInitialized;
    napi_status ret = napi_create_int32(env, videoRenderer->IsInitialized(), &(isInitialized));
    if (ret != napi_ok) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "PluginManager",
                     "GetXComponentStatus: napi_create_int32 isInitialized error");
        return nullptr;
    }
    napi_value obj;
    ret = napi_create_object(env, &obj);
    if (ret != napi_ok) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "PluginManager",
                     "GetXComponentStatus: napi_create_object error");
        return nullptr;
    }
    ret = napi_set_named_property(env, obj, "isInitialized", isInitialized);
    if (ret != napi_ok) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "PluginManager",
                     "GetXComponentStatus: napi_set_named_property isInitialized error");
        return nullptr;
    }
    return obj;
}