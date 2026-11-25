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

class RtcpSrReporterWrapper : public MediaHandlerBase<RtcpSrReporterWrapper, rtc::RtcpSrReporter>
{
public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  explicit RtcpSrReporterWrapper(const Napi::CallbackInfo &info);
  ~RtcpSrReporterWrapper();

  Napi::Value getRtpPacketizationConfig(const Napi::CallbackInfo &info);

private:
  Napi::ObjectReference mRtpConfigObject;
};

#endif // MEDIA_HANDLERS_WRAPPER_H
