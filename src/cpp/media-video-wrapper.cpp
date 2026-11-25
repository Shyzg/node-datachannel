#include "media-video-wrapper.h"
#include "wrapper-utils.h"

Napi::Object VideoWrapper::Init(Napi::Env env, Napi::Object exports)
{
  Napi::HandleScope scope(env);

  std::vector<PropertyDescriptor> props = {
      InstanceMethod("addVideoCodec", &VideoWrapper::addVideoCodec),
      InstanceMethod("addH264Codec", &VideoWrapper::addH264Codec),
      InstanceMethod("addH265Codec", &VideoWrapper::addH265Codec),
      InstanceMethod("addVP8Codec", &VideoWrapper::addVP8Codec),
      InstanceMethod("addVP9Codec", &VideoWrapper::addVP9Codec),
      InstanceMethod("addAV1Codec", &VideoWrapper::addAV1Codec),
  };
  appendBaseDescriptors(props);

  Napi::Function func = DefineClass(env, "Video", props);

  exports.Set("Video", func);
  return exports;
}

VideoWrapper::VideoWrapper(const Napi::CallbackInfo &info)
    : MediaDescriptionBase<VideoWrapper, rtc::Description::Video>(info)
{
  if (initMedia(info, "video", rtc::Description::Direction::SendOnly))
    info.This().As<Napi::Object>().TypeTag(&kVideoTypeTag);
}

rtc::Description::Video VideoWrapper::getVideoInstance() { return *mMediaPtr; }

void VideoWrapper::addVideoCodec(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();
  int length = info.Length();

  if (length < 2 || !info[1].IsString())
  {
    Napi::TypeError::New(env, "We expect (Number, String, String[optional]) as param").ThrowAsJavaScriptException();
    return;
  }

  int payloadType = 0;
  if (!parsePayloadType(env, info[0], "payloadType", payloadType))
    return;
  std::string codec = info[1].As<Napi::String>().ToString();
  std::optional<std::string> profile = std::nullopt;

  if (length > 2 && !info[2].IsUndefined())
  {
    if (!info[2].IsString())
    {
      Napi::TypeError::New(env, "profile (String) expected").ThrowAsJavaScriptException();
      return;
    }
    profile = info[2].As<Napi::String>().ToString();
  }

  mMediaPtr->addVideoCodec(payloadType, codec, profile);
}

void VideoWrapper::addH264Codec(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();
  int length = info.Length();

  if (length < 1)
  {
    Napi::TypeError::New(env, "We expect (Number) as param").ThrowAsJavaScriptException();
    return;
  }

  int payloadType = 0;
  if (!parsePayloadType(env, info[0], "payloadType", payloadType))
    return;
  std::string profile = rtc::DEFAULT_H264_VIDEO_PROFILE;

  if (length > 1 && !info[1].IsUndefined())
  {
    if (!info[1].IsString())
    {
      Napi::TypeError::New(env, "profile (String) expected").ThrowAsJavaScriptException();
      return;
    }
    profile = info[1].As<Napi::String>().ToString();
  }

  mMediaPtr->addH264Codec(payloadType, profile);
}

void VideoWrapper::addH265Codec(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();
  if (info.Length() < 1)
  {
    Napi::TypeError::New(env, "We expect (Number) as param").ThrowAsJavaScriptException();
    return;
  }
  int payloadType = 0;
  if (!parsePayloadType(env, info[0], "payloadType", payloadType))
    return;
  mMediaPtr->addH265Codec(payloadType);
}

void VideoWrapper::addVP8Codec(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();
  if (info.Length() < 1)
  {
    Napi::TypeError::New(env, "We expect (Number) as param").ThrowAsJavaScriptException();
    return;
  }
  int payloadType = 0;
  if (!parsePayloadType(env, info[0], "payloadType", payloadType))
    return;
  mMediaPtr->addVP8Codec(payloadType);
}

void VideoWrapper::addVP9Codec(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();
  if (info.Length() < 1)
  {
    Napi::TypeError::New(env, "We expect (Number) as param").ThrowAsJavaScriptException();
    return;
  }
  int payloadType = 0;
  if (!parsePayloadType(env, info[0], "payloadType", payloadType))
    return;
  mMediaPtr->addVP9Codec(payloadType);
}

void VideoWrapper::addAV1Codec(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();
  if (info.Length() < 1)
  {
    Napi::TypeError::New(env, "We expect (Number) as param").ThrowAsJavaScriptException();
    return;
  }
  int payloadType = 0;
  if (!parsePayloadType(env, info[0], "payloadType", payloadType))
    return;
  mMediaPtr->addAV1Codec(payloadType);
}
