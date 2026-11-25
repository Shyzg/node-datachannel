#ifndef MEDIA_VIDEO_WRAPPER_H
#define MEDIA_VIDEO_WRAPPER_H

#include "media-description-base.h"

class VideoWrapper : public MediaDescriptionBase<VideoWrapper, rtc::Description::Video>
{
public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  VideoWrapper(const Napi::CallbackInfo &info);

  rtc::Description::Video getVideoInstance();

  void addVideoCodec(const Napi::CallbackInfo &info);
  void addH264Codec(const Napi::CallbackInfo &info);
  void addH265Codec(const Napi::CallbackInfo &info);
  void addVP8Codec(const Napi::CallbackInfo &info);
  void addVP9Codec(const Napi::CallbackInfo &info);
  void addAV1Codec(const Napi::CallbackInfo &info);
};

#endif // MEDIA_VIDEO_WRAPPER_H
