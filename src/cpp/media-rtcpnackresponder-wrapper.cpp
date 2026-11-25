#include "media-rtcpnackresponder-wrapper.h"
#include "media-mediahandler-helper.h"
#include "wrapper-utils.h"

Napi::Object RtcpNackResponderWrapper::Init(Napi::Env env, Napi::Object exports)
{
  Napi::HandleScope scope(env);

  Napi::Function func = Napi::ObjectWrap<RtcpNackResponderWrapper>::DefineClass(
      env, "RtcpNackResponder",
      {
          InstanceMethod("addToChain", &RtcpNackResponderWrapper::addToChain),
      });

  exports.Set("RtcpNackResponder", func);
  return exports;
}

RtcpNackResponderWrapper::RtcpNackResponderWrapper(const Napi::CallbackInfo &info)
    : Napi::ObjectWrap<RtcpNackResponderWrapper>(info)
{
  Napi::Env env = info.Env();

  size_t maxSize = rtc::RtcpNackResponder::DefaultMaxSize;
  if (info.Length() >= 1 && !info[0].IsUndefined())
  {
    if (!parseNonNegativeSize(env, info[0], "maxSize", maxSize))
      return;
  }
  mResponderPtr = std::make_shared<rtc::RtcpNackResponder>(maxSize);
  info.This().As<Napi::Object>().TypeTag(&kRtcpNackResponderTypeTag);
}

RtcpNackResponderWrapper::~RtcpNackResponderWrapper() { mResponderPtr.reset(); }

std::shared_ptr<rtc::RtcpNackResponder> RtcpNackResponderWrapper::getResponderInstance() { return mResponderPtr; }

void RtcpNackResponderWrapper::addToChain(const Napi::CallbackInfo &info)
{
  addMediaHandlerToChain(info, mResponderPtr);
}
