#ifndef MEDIA_TRACK_WRAPPER_H
#define MEDIA_TRACK_WRAPPER_H

#include "channel-wrapper-base.h"

class TrackWrapper
    : public ChannelWrapperBase<TrackWrapper, rtc::Track, &AddonData::tracks, &AddonData::trackCtor,
                                /*BinaryOnlyMessages=*/true>
{
public:
  static constexpr const char *kChannelLabel = "track";

  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  explicit TrackWrapper(const Napi::CallbackInfo &info);
  ~TrackWrapper();

  Napi::Value direction(const Napi::CallbackInfo &info);
  Napi::Value mid(const Napi::CallbackInfo &info);
  Napi::Value type(const Napi::CallbackInfo &info);
  Napi::Value isClosed(const Napi::CallbackInfo &info);
  Napi::Value requestBitrate(const Napi::CallbackInfo &info);
  Napi::Value requestKeyframe(const Napi::CallbackInfo &info);
  void setMediaHandler(const Napi::CallbackInfo &info);
};

#endif // MEDIA_TRACK_WRAPPER_H
