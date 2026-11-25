#include "media-rtcpsrreporter-wrapper.h"
#include "media-mediahandler-helper.h"
#include "wrapper-utils.h"

Napi::Object RtcpSrReporterWrapper::Init(Napi::Env env, Napi::Object exports)
{
  Napi::HandleScope scope(env);

  Napi::Function func = Napi::ObjectWrap<RtcpSrReporterWrapper>::DefineClass(
      env, "RtcpSrReporter",
      {
          InstanceMethod("addToChain", &RtcpSrReporterWrapper::addToChain),
          InstanceAccessor("rtpConfig", &RtcpSrReporterWrapper::getRtpPacketizationConfig, nullptr),
      });

  exports.Set("RtcpSrReporter", func);
  return exports;
}

RtcpSrReporterWrapper::RtcpSrReporterWrapper(const Napi::CallbackInfo &info)
    : Napi::ObjectWrap<RtcpSrReporterWrapper>(info)
{
  auto rtpConfig = extractRtpConfig(info.Env(), info[0], mRtpConfigObject);
  if (!rtpConfig)
    return;

  mReporterPtr = std::make_shared<rtc::RtcpSrReporter>(rtpConfig);
  info.This().As<Napi::Object>().TypeTag(&kRtcpSrReporterTypeTag);
}

RtcpSrReporterWrapper::~RtcpSrReporterWrapper()
{
  mReporterPtr.reset();
  mRtpConfigObject.Reset();
}

std::shared_ptr<rtc::RtcpSrReporter> RtcpSrReporterWrapper::getReporterInstance() { return mReporterPtr; }

void RtcpSrReporterWrapper::addToChain(const Napi::CallbackInfo &info) { addMediaHandlerToChain(info, mReporterPtr); }

Napi::Value RtcpSrReporterWrapper::getRtpPacketizationConfig(const Napi::CallbackInfo &info)
{
  return returnRtpConfig(info, mReporterPtr.get(), mRtpConfigObject);
}
