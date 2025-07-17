//
// Created on 2025/7/17.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#ifndef ARKUI_DEMO_AUDIO_RENDER_H
#define ARKUI_DEMO_AUDIO_RENDER_H


#include "libavutil/mem.h"
#include "libavutil/samplefmt.h"
#include "napi/native_api.h"
#include <cstdint>
#include <ohaudio/native_audiostreambuilder.h>
#include <ohaudio/native_audiorenderer.h>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <vector>

namespace VideoStreamNS{

OH_AudioData_Callback_Result write_callback(OH_AudioRenderer *render, void* userData, void* audioData, int32_t audioDataSize);
extern uint8_t * Buffer;

class AudioRender {
public:
    explicit AudioRender(): audioRenderer_(nullptr), builder(nullptr){
        audioBufferSize_ = av_samples_get_buffer_size(nullptr, 2, 4096, AV_SAMPLE_FMT_S16, 1);
        Buffer = (uint8_t *)av_malloc(audioBufferSize_);
    };
    bool renderInit();
    bool renderAudioFrame(void* audioBuffer, int32_t bufferLen);
    void AudioRendererRelease();
    
    bool renderStart();
private:
    OH_AudioRenderer* audioRenderer_;
    OH_AudioStreamBuilder* builder;
    
    // uint8_t *buffer_;
    int audioBufferSize_;
    std::queue<std::vector<uint8_t>> dataQueue_;
    std::mutex queueMutex_;
    std::condition_variable queueCond_;
};
}
#endif //ARKUI_DEMO_AUDIO_RENDER_H
