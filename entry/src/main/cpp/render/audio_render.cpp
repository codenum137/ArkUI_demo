//
// Created on 2025/7/17.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#include "audio_render.h"
#include "hilog/log.h"
//#include "render/plugin_render.h"

namespace VideoStreamNS{
OH_AudioData_Callback_Result write_callback(OH_AudioRenderer *render, void* userData, void* audioData, int32_t audioDataSize){
    OH_LOG_INFO(LOG_APP, "On audio data write");
    if(Buffer == nullptr){
        return AUDIO_DATA_CALLBACK_RESULT_INVALID;
    }
    memcpy(audioData, Buffer, audioDataSize);
    return AUDIO_DATA_CALLBACK_RESULT_VALID;
}
int32_t error_callback(OH_AudioRenderer* renderer, void* userData, OH_AudioStream_Result error){
    // 根据error表示的音频异常信息，做出相应的处理
    OH_LOG_ERROR(LOG_APP, "Audio process error : %{public}d ", error);
    return 0;
}
    
int32_t streamEvent_callback(OH_AudioRenderer* renderer, void* userData,OH_AudioStream_Event event){
    OH_LOG_INFO(LOG_APP, "On audio stream event ");
    return 0;
}
int32_t interruptEvent_callback(OH_AudioRenderer* renderer, void* userData, OH_AudioInterrupt_ForceType type, OH_AudioInterrupt_Hint hint){
    OH_LOG_INFO(LOG_APP, "On audio interrupt event");
    return 0;
}


bool AudioRender::renderInit(){
    // Setup Audio Renderer
    OH_AudioStream_Result result;
    // 设置renderer信息
    OH_AudioStreamBuilder_Create(&builder, AUDIOSTREAM_TYPE_RENDERER);
    OH_AudioStreamBuilder_SetSamplingRate(builder, 48000);                     // 设置采样率
    OH_AudioStreamBuilder_SetChannelCount(builder, 2);                         // 设置声道
    // OH_AudioStreamBuilder_SetChannelLayout(builder, CH_LAYOUT_STEREO);
    OH_AudioStreamBuilder_SetSampleFormat(builder, AUDIOSTREAM_SAMPLE_S16LE);  // 设置采样格式
    OH_AudioStreamBuilder_SetEncodingType(builder, AUDIOSTREAM_ENCODING_TYPE_RAW); // 设置音频流编码类型

    OH_AudioStreamBuilder_SetRendererInfo(builder, AUDIOSTREAM_USAGE_MOVIE);  //设置使用场景
    
    OH_AudioStreamBuilder_SetFrameSizeInCallback(builder, audioBufferSize_);  
    // 设置回调函数
    OH_AudioRenderer_Callbacks callbacks;
    callbacks.OH_AudioRenderer_OnError = error_callback;
    callbacks.OH_AudioRenderer_OnInterruptEvent = interruptEvent_callback;
    callbacks.OH_AudioRenderer_OnStreamEvent = streamEvent_callback;

    OH_AudioStreamBuilder_SetRendererCallback(builder, callbacks, nullptr);
    OH_AudioRenderer_OnWriteDataCallback writeDataCb = write_callback;
    OH_AudioStreamBuilder_SetRendererWriteDataCallback(builder, writeDataCb, this);

    OH_AudioStreamBuilder_GenerateRenderer(builder, &audioRenderer_);
    
    if(!renderStart()){
        return false;
    }
    // OH_AudioRenderer_Start(audioRenderer_);

    // OH_AudioStreamBuilder_Destroy(builder);
    return true;
}

// todo 将传入的帧放入Buffer, 供回调函数使用
bool AudioRender::renderAudioFrame(void* audioBuffer, int32_t bufferLen){
    OH_LOG_INFO(LOG_APP, "Render audio frame, len: %d", bufferLen);
    memcpy(Buffer, audioBuffer, bufferLen);
    return true;
}

bool AudioRender::renderStart(){
    OH_AudioStream_Result result = OH_AudioRenderer_Start(audioRenderer_);
    if(result){
        OH_LOG_ERROR(LOG_APP, "audio renderer start error %{public}d", result);
        return false;
    }
    return true;
}

void AudioRender::AudioRendererRelease() {
    if (audioRenderer_) {
        OH_AudioRenderer_Release(audioRenderer_);
        OH_AudioStreamBuilder_Destroy(builder);
        audioRenderer_ = nullptr;
        builder = nullptr;
    }
   OH_LOG_INFO(LOG_APP, "audio renderer released");
}
}