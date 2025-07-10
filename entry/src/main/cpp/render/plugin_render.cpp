//
// Created on 2025/7/10.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#include <cstdint>
#include <js_native_api_types.h>
#include "plugin_render.h"


PluginRender::PluginRender(int64_t& id)
{
    this->id_ = id;
    this->eglCore_ = new EGLCore();
    hasDraw_ = 0;
    hasChangeColor_ = 0;
}

void PluginRender::DrawPattern()
{
    eglCore_->Draw(hasDraw_);
}

void PluginRender::InitNativeWindow(OHNativeWindow *window)
{
    eglCore_->EglContextInit(window);
}

void PluginRender::UpdateNativeWindowSize(int width, int height)
{
    eglCore_->UpdateSize(width, height);
    if (!hasChangeColor_ && !hasDraw_) {
        eglCore_->Background();
    }
}

int32_t PluginRender::HasDraw()
{
    return hasDraw_;
}

int32_t PluginRender::HasChangedColor()
{
    return hasChangeColor_;
}
