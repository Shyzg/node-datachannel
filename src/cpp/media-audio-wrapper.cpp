#include "media-audio-wrapper.h"
#include "wrapper-utils.h"

Napi::Object AudioWrapper::Init(Napi::Env env, Napi::Object exports)
{
  Napi::HandleScope scope(env);

  std::vector<PropertyDescriptor> props = {
      InstanceMethod("addAudioCodec", &AudioWrapper::addAudioCodec),
      InstanceMethod("addOpusCodec", &AudioWrapper::addOpusCodec),
  };
  appendBaseDescriptors(props);

  Napi::Function func = DefineClass(env, "Audio", props);

  exports.Set("Audio", func);
  return exports;
}

AudioWrapper::AudioWrapper(const Napi::CallbackInfo &info)
    : MediaDescriptionBase<AudioWrapper, rtc::Description::Audio>(info)
{
  if (initMedia(info, "audio", rtc::Description::Direction::SendOnly))
    info.This().As<Napi::Object>().TypeTag(&kAudioTypeTag);
}

rtc::Description::Audio AudioWrapper::getAudioInstance() { return *mMediaPtr; }

void AudioWrapper::addAudioCodec(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();
  int length = info.Length();

  if (length < 2 || !info[0].IsNumber() || !info[1].IsString())
  {
    Napi::TypeError::New(env, "We expect (Number, String, String[optional]) as param").ThrowAsJavaScriptException();
    return;
  }

  int payloadType = info[0].As<Napi::Number>().Int32Value();
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

  mMediaPtr->addAudioCodec(payloadType, codec, profile);
}

void AudioWrapper::addOpusCodec(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();
  int length = info.Length();

  if (length < 1 || !info[0].IsNumber())
  {
    Napi::TypeError::New(env, "We expect (Number) as param").ThrowAsJavaScriptException();
    return;
  }

  int payloadType = info[0].As<Napi::Number>().Int32Value();
  std::optional<std::string> profile = rtc::DEFAULT_OPUS_AUDIO_PROFILE;

  if (length > 1 && !info[1].IsUndefined())
  {
    if (!info[1].IsString())
    {
      Napi::TypeError::New(env, "profile (String) expected").ThrowAsJavaScriptException();
      return;
    }
    profile = info[1].As<Napi::String>().ToString();
  }

  mMediaPtr->addOpusCodec(payloadType, profile);
}
