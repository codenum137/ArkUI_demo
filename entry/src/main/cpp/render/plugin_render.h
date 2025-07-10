//
// Created on 2025/7/10.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#ifndef ARKUI_DEMO_PLUGIN_RENDER_H
#define ARKUI_DEMO_PLUGIN_RENDER_H

#include <ace/xcomponent/native_interface_xcomponent.h>
#include <native_window/external_window.h>
#include "egl_core.h"

class PluginRender {
public:
    explicit PluginRender(int64_t& id);
    ~PluginRender()
    {
        if (eglCore_ != nullptr) {
            eglCore_->Release();
            delete eglCore_;
            eglCore_ = nullptr;
        }
    }

    void DrawPattern();
    int32_t HasDraw();
    int32_t HasChangedColor();
    void InitNativeWindow(OHNativeWindow* window);
    void UpdateNativeWindowSize(int width, int height);
private:
    EGLCore* eglCore_;
    int64_t id_;
    int32_t hasDraw_;
    int32_t hasChangeColor_;
};

#endif //ARKUI_DEMO_PLUGIN_RENDER_H

