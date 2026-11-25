#include "media-rtcpreceivingsession-wrapper.h"
#include "media-mediahandler-helper.h"
#include "wrapper-utils.h"

Napi::Object RtcpReceivingSessionWrapper::Init(Napi::Env env, Napi::Object exports)
{
  Napi::HandleScope scope(env);

  Napi::Function func = Napi::ObjectWrap<RtcpReceivingSessionWrapper>::DefineClass(
      env, "RtcpReceivingSession",
      {
          InstanceMethod("addToChain", &RtcpReceivingSessionWrapper::addToChain),
      });

  exports.Set("RtcpReceivingSession", func);
  return exports;
}

RtcpReceivingSessionWrapper::RtcpReceivingSessionWrapper(const Napi::CallbackInfo &info)
    : Napi::ObjectWrap<RtcpReceivingSessionWrapper>(info)
{
  mSessionPtr = std::make_shared<rtc::RtcpReceivingSession>();
  info.This().As<Napi::Object>().TypeTag(&kRtcpReceivingSessionTypeTag);
}

RtcpReceivingSessionWrapper::~RtcpReceivingSessionWrapper() { mSessionPtr.reset(); }

std::shared_ptr<rtc::RtcpReceivingSession> RtcpReceivingSessionWrapper::getSessionInstance() { return mSessionPtr; }

void RtcpReceivingSessionWrapper::addToChain(const Napi::CallbackInfo &info)
{
  addMediaHandlerToChain(info, mSessionPtr);
}
