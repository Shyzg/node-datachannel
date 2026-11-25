#ifndef MEDIA_AUDIO_WRAPPER_H
#define MEDIA_AUDIO_WRAPPER_H

#include "media-description-base.h"

class AudioWrapper : public MediaDescriptionBase<AudioWrapper, rtc::Description::Audio>
{
public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  AudioWrapper(const Napi::CallbackInfo &info);

  rtc::Description::Audio getAudioInstance();

  void addAudioCodec(const Napi::CallbackInfo &info);
  void addOpusCodec(const Napi::CallbackInfo &info);
};

#endif // MEDIA_AUDIO_WRAPPER_H
