#ifndef MEDIA_HANDLERS_WRAPPER_H
#define MEDIA_HANDLERS_WRAPPER_H

#include <memory>
#include <vector>

#include <napi.h>
#include <rtc/rtc.hpp>

#include "media-mediahandler-helper.h"

template <typename Derived, typename RtcHandler>
class MediaHandlerBase : public Napi::ObjectWrap<Derived>
{
public:
  using PropertyDescriptor = typename Napi::ObjectWrap<Derived>::PropertyDescriptor;

  static Napi::Object Init(Napi::Env env, Napi::Object exports, const char *name,
                           std::vector<PropertyDescriptor> extraProps = {})
  {
    Napi::HandleScope scope(env);
    extraProps.push_back(Derived::InstanceMethod("addToChain", &MediaHandlerBase::addToChain));
    Napi::Function func = Derived::DefineClass(env, name, extraProps);
    exports.Set(name, func);
    return exports;
  }

  explicit MediaHandlerBase(const Napi::CallbackInfo &info) : Napi::ObjectWrap<Derived>(info) {}

  ~MediaHandlerBase() { mHandlerPtr.reset(); }

  std::shared_ptr<RtcHandler> getHandlerInstance() { return mHandlerPtr; }

  void addToChain(const Napi::CallbackInfo &info) { addMediaHandlerToChain(info, mHandlerPtr); }

protected:
  std::shared_ptr<RtcHandler> mHandlerPtr = nullptr;
};

template <typename Derived, typename RtcHandler>
class RtpConfigHandlerBase : public MediaHandlerBase<Derived, RtcHandler>
{
public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports, const char *name)
  {
    return MediaHandlerBase<Derived, RtcHandler>::Init(
        env, exports, name,
        {Derived::InstanceAccessor("rtpConfig", &RtpConfigHandlerBase::getRtpPacketizationConfig, nullptr)});
  }

  explicit RtpConfigHandlerBase(const Napi::CallbackInfo &info) : MediaHandlerBase<Derived, RtcHandler>(info) {}

  ~RtpConfigHandlerBase()
  {
    this->mHandlerPtr.reset();
    mRtpConfigObject.Reset();
  }

  Napi::Value getRtpPacketizationConfig(const Napi::CallbackInfo &info)
  {
    return returnRtpConfig(info, this->mHandlerPtr.get(), mRtpConfigObject);
  }

protected:
  Napi::ObjectReference mRtpConfigObject;
};

class PacingHandlerWrapper : public MediaHandlerBase<PacingHandlerWrapper, rtc::PacingHandler>
{
public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  explicit PacingHandlerWrapper(const Napi::CallbackInfo &info);
};

class RtcpNackResponderWrapper : public MediaHandlerBase<RtcpNackResponderWrapper, rtc::RtcpNackResponder>
{
public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  explicit RtcpNackResponderWrapper(const Napi::CallbackInfo &info);
};

class RtcpReceivingSessionWrapper : public MediaHandlerBase<RtcpReceivingSessionWrapper, rtc::RtcpReceivingSession>
{
public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  explicit RtcpReceivingSessionWrapper(const Napi::CallbackInfo &info);
};

class RtcpSrReporterWrapper : public RtpConfigHandlerBase<RtcpSrReporterWrapper, rtc::RtcpSrReporter>
{
public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  explicit RtcpSrReporterWrapper(const Napi::CallbackInfo &info);
};

#endif // MEDIA_HANDLERS_WRAPPER_H
