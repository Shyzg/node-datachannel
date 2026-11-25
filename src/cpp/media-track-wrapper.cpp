#include "media-track-wrapper.h"
#include "media-direction.h"
#include "media-mediahandler-helper.h"

Napi::Object TrackWrapper::Init(Napi::Env env, Napi::Object exports)
{
  Napi::HandleScope scope(env);

  std::vector<PropertyDescriptor> props = {
      InstanceMethod("direction", &TrackWrapper::direction),
      InstanceMethod("mid", &TrackWrapper::mid),
      InstanceMethod("type", &TrackWrapper::type),
      InstanceMethod("isClosed", &TrackWrapper::isClosed),
      InstanceMethod("requestBitrate", &TrackWrapper::requestBitrate),
      InstanceMethod("requestKeyframe", &TrackWrapper::requestKeyframe),
      InstanceMethod("setMediaHandler", &TrackWrapper::setMediaHandler),
  };
  appendChannelDescriptors(props, /*includeBufferedAmount=*/false);

  Napi::Function func = DefineClass(env, "Track", props);
  storeConstructor(env, func);
  exports.Set("Track", func);
  return exports;
}

TrackWrapper::TrackWrapper(const Napi::CallbackInfo &info) : ChannelWrapperBase(info)
{
  PLOG_DEBUG << "Constructor called";
  if (info.Length() < 1 || !info[0].IsExternal())
  {
    Napi::TypeError::New(info.Env(), "Track cannot be constructed directly").ThrowAsJavaScriptException();
    return;
  }

  auto *ptr = info[0].As<Napi::External<std::shared_ptr<rtc::Track>>>().Data();
  if (!ptr || !*ptr)
  {
    Napi::TypeError::New(info.Env(), "Invalid Track handle").ThrowAsJavaScriptException();
    return;
  }

  mChannelPtr = *ptr;
  PLOG_DEBUG << "Track created";
  registerInstance(info.Env());
}

TrackWrapper::~TrackWrapper() { destroyInDestructor(); }

Napi::Value TrackWrapper::direction(const Napi::CallbackInfo &info)
{
  if (!mChannelPtr)
  {
    Napi::Error::New(info.Env(), "direction() called on destroyed track").ThrowAsJavaScriptException();
    return info.Env().Null();
  }
  return Napi::String::New(info.Env(), directionToStr(mChannelPtr->direction()));
}

Napi::Value TrackWrapper::mid(const Napi::CallbackInfo &info)
{
  if (!mChannelPtr)
  {
    Napi::Error::New(info.Env(), "mid() called on destroyed track").ThrowAsJavaScriptException();
    return info.Env().Null();
  }
  return Napi::String::New(info.Env(), mChannelPtr->mid());
}

Napi::Value TrackWrapper::type(const Napi::CallbackInfo &info)
{
  if (!mChannelPtr)
  {
    Napi::Error::New(info.Env(), "type() called on destroyed track").ThrowAsJavaScriptException();
    return info.Env().Null();
  }
  return Napi::String::New(info.Env(), mChannelPtr->description().type());
}

Napi::Value TrackWrapper::isClosed(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();
  return Napi::Boolean::New(env, !mChannelPtr || mChannelPtr->isClosed());
}

Napi::Value TrackWrapper::requestBitrate(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();
  if (!mChannelPtr)
  {
    Napi::Error::New(env, "requestBitrate() called on destroyed track").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() < 1)
  {
    Napi::TypeError::New(env, "Number expected").ThrowAsJavaScriptException();
    return env.Null();
  }

  uint32_t bitrate = 0;
  if (!parseNonNegativeUint32(env, info[0], "bitRate", bitrate))
    return env.Null();
  return Napi::Boolean::New(env, mChannelPtr->requestBitrate(bitrate));
}

Napi::Value TrackWrapper::requestKeyframe(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();
  if (!mChannelPtr)
  {
    Napi::Error::New(env, "requestKeyframe() called on destroyed track").ThrowAsJavaScriptException();
    return env.Null();
  }
  return Napi::Boolean::New(env, mChannelPtr->requestKeyframe());
}

void TrackWrapper::setMediaHandler(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();
  if (!mChannelPtr)
  {
    Napi::Error::New(env, "setMediaHandler() called on destroyed track").ThrowAsJavaScriptException();
    return;
  }

  if (info.Length() < 1 || !info[0].IsObject())
  {
    Napi::TypeError::New(env, "MediaHandler class instance expected").ThrowAsJavaScriptException();
    return;
  }

  std::shared_ptr<rtc::MediaHandler> handler = asMediaHandler(info[0].As<Napi::Object>());
  if (!handler)
  {
    Napi::TypeError::New(env, "MediaHandler class instance expected. If this error is unexpected, please report a bug!")
        .ThrowAsJavaScriptException();
    return;
  }
  mChannelPtr->setMediaHandler(handler);
}
