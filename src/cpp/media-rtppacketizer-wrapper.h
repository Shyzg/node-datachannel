#ifndef MEDIA_RTPPACKETIZER_WRAPPER_H
#define MEDIA_RTPPACKETIZER_WRAPPER_H

#include <memory>

#include <napi.h>
#include <rtc/rtc.hpp>

#include "media-mediahandler-helper.h"

template <typename Derived, typename RtcPacketizer>
class RtpPacketizerBase : public Napi::ObjectWrap<Derived>
{
public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports, const char *name)
  {
    Napi::HandleScope scope(env);
    Napi::Function func = Derived::DefineClass(
        env, name,
        {
            Derived::InstanceMethod("addToChain", &RtpPacketizerBase::addToChain),
            Derived::InstanceAccessor("rtpConfig", &RtpPacketizerBase::getRtpPacketizationConfig, nullptr),
        });

    exports.Set(name, func);
    return exports;
  }

  explicit RtpPacketizerBase(const Napi::CallbackInfo &info) : Napi::ObjectWrap<Derived>(info) {}

  ~RtpPacketizerBase()
  {
    mPacketizerPtr.reset();
    mRtpConfigObject.Reset();
  }

  std::shared_ptr<RtcPacketizer> getPacketizerInstance() { return mPacketizerPtr; }

  void addToChain(const Napi::CallbackInfo &info) { addMediaHandlerToChain(info, mPacketizerPtr); }

  Napi::Value getRtpPacketizationConfig(const Napi::CallbackInfo &info)
  {
    return returnRtpConfig(info, mPacketizerPtr.get(), mRtpConfigObject);
  }

protected:
  std::shared_ptr<RtcPacketizer> mPacketizerPtr = nullptr;
  Napi::ObjectReference mRtpConfigObject;
};

class RtpPacketizerWrapper : public RtpPacketizerBase<RtpPacketizerWrapper, rtc::RtpPacketizer>
{
public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  explicit RtpPacketizerWrapper(const Napi::CallbackInfo &info);
};

class H264RtpPacketizerWrapper : public RtpPacketizerBase<H264RtpPacketizerWrapper, rtc::H264RtpPacketizer>
{
public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  explicit H264RtpPacketizerWrapper(const Napi::CallbackInfo &info);
};

class H265RtpPacketizerWrapper : public RtpPacketizerBase<H265RtpPacketizerWrapper, rtc::H265RtpPacketizer>
{
public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  explicit H265RtpPacketizerWrapper(const Napi::CallbackInfo &info);
};

class AV1RtpPacketizerWrapper : public RtpPacketizerBase<AV1RtpPacketizerWrapper, rtc::AV1RtpPacketizer>
{
public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  explicit AV1RtpPacketizerWrapper(const Napi::CallbackInfo &info);
};

#endif // MEDIA_RTPPACKETIZER_WRAPPER_H
