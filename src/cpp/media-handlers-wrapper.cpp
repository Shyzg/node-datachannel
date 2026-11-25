#include "media-handlers-wrapper.h"
#include "wrapper-utils.h"

#include <chrono>
#include <cmath>

Napi::Object PacingHandlerWrapper::Init(Napi::Env env, Napi::Object exports)
{
  return MediaHandlerBase::Init(env, exports, "PacingHandler");
}

PacingHandlerWrapper::PacingHandlerWrapper(const Napi::CallbackInfo &info) : MediaHandlerBase(info)
{
  Napi::Env env = info.Env();
  if (info.Length() < 2)
  {
    Napi::TypeError::New(env, "Expected 2 parameters").ThrowAsJavaScriptException();
    return;
  }
  if (!info[0].IsNumber())
  {
    Napi::TypeError::New(env, "Expected bitsPerSecond to be a number").ThrowAsJavaScriptException();
    return;
  }
  double bitsPerSecond = info[0].As<Napi::Number>().DoubleValue();
  if (!std::isfinite(bitsPerSecond) || bitsPerSecond <= 0)
  {
    Napi::RangeError::New(env, "bitsPerSecond must be a positive finite number").ThrowAsJavaScriptException();
    return;
  }
  int64_t sendInterval = 0;
  if (!parsePositiveDurationMs(env, info[1], "sendInterval", sendInterval))
    return;

  mHandlerPtr = std::make_shared<rtc::PacingHandler>(bitsPerSecond, std::chrono::milliseconds(sendInterval));
  info.This().As<Napi::Object>().TypeTag(&kPacingHandlerTypeTag);
}

Napi::Object RtcpNackResponderWrapper::Init(Napi::Env env, Napi::Object exports)
{
  return MediaHandlerBase::Init(env, exports, "RtcpNackResponder");
}

RtcpNackResponderWrapper::RtcpNackResponderWrapper(const Napi::CallbackInfo &info) : MediaHandlerBase(info)
{
  Napi::Env env = info.Env();
  size_t maxSize = rtc::RtcpNackResponder::DefaultMaxSize;
  if (info.Length() >= 1 && !info[0].IsUndefined())
  {
    // RTP sequence numbers are 16-bit (max 65535 distinct cached packets), and libdatachannel asserts maxSize > 0.
    if (!parseBoundedInt(env, info[0], "maxSize", 1.0, 65535.0, "must be between 1 and 65535", maxSize))
      return;
  }
  mHandlerPtr = std::make_shared<rtc::RtcpNackResponder>(maxSize);
  info.This().As<Napi::Object>().TypeTag(&kRtcpNackResponderTypeTag);
}

Napi::Object RtcpReceivingSessionWrapper::Init(Napi::Env env, Napi::Object exports)
{
  return MediaHandlerBase::Init(env, exports, "RtcpReceivingSession");
}

RtcpReceivingSessionWrapper::RtcpReceivingSessionWrapper(const Napi::CallbackInfo &info) : MediaHandlerBase(info)
{
  mHandlerPtr = std::make_shared<rtc::RtcpReceivingSession>();
  info.This().As<Napi::Object>().TypeTag(&kRtcpReceivingSessionTypeTag);
}

Napi::Object RtcpSrReporterWrapper::Init(Napi::Env env, Napi::Object exports)
{
  return MediaHandlerBase::Init(
      env, exports, "RtcpSrReporter",
      {InstanceAccessor("rtpConfig", &RtcpSrReporterWrapper::getRtpPacketizationConfig, nullptr)});
}

RtcpSrReporterWrapper::RtcpSrReporterWrapper(const Napi::CallbackInfo &info) : MediaHandlerBase(info)
{
  auto rtpConfig = extractRtpConfig(info.Env(), info[0], mRtpConfigObject);
  if (!rtpConfig)
    return;

  mHandlerPtr = std::make_shared<rtc::RtcpSrReporter>(rtpConfig);
  info.This().As<Napi::Object>().TypeTag(&kRtcpSrReporterTypeTag);
}

RtcpSrReporterWrapper::~RtcpSrReporterWrapper()
{
  mHandlerPtr.reset();
  mRtpConfigObject.Reset();
}

Napi::Value RtcpSrReporterWrapper::getRtpPacketizationConfig(const Napi::CallbackInfo &info)
{
  return returnRtpConfig(info, mHandlerPtr.get(), mRtpConfigObject);
}
