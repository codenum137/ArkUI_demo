//
// Created on 2025/7/10.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#ifndef ARKUI_DEMO_PLUGIN_MANAGER_H
#define ARKUI_DEMO_PLUGIN_MANAGER_H

#include <ace/xcomponent/native_interface_xcomponent.h>
#include <js_native_api.h>
#include <js_native_api_types.h>
#include <native_window/external_window.h>
#include <unordered_map>

namespace VideoStreamNS {
class VideoRenderer;
}

class PluginManager {
public:
    ~PluginManager();
    static VideoStreamNS::VideoRenderer *GetVideoRenderer(int64_t surfaceId);
    static napi_value SetSurfaceId(napi_env env, napi_callback_info info);
    static napi_value ChangeSurface(napi_env env, napi_callback_info info);
    static napi_value DestroySurface(napi_env env, napi_callback_info info);
    static napi_value GetXComponentStatus(napi_env env, napi_callback_info info);

    static std::unordered_map<int64_t, VideoStreamNS::VideoRenderer *> videoRendererMap_;
    static std::unordered_map<int64_t, OHNativeWindow *> windowMap_;
};

#endif // ARKUI_DEMO_PLUGIN_MANAGER_H
