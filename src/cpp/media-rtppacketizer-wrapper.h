#ifndef MEDIA_RTPPACKETIZER_WRAPPER_H
#define MEDIA_RTPPACKETIZER_WRAPPER_H

#include "media-handlers-wrapper.h"

class RtpPacketizerWrapper : public RtpConfigHandlerBase<RtpPacketizerWrapper, rtc::RtpPacketizer>
{
public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  explicit RtpPacketizerWrapper(const Napi::CallbackInfo &info);
};

class H264RtpPacketizerWrapper : public RtpConfigHandlerBase<H264RtpPacketizerWrapper, rtc::H264RtpPacketizer>
{
public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  explicit H264RtpPacketizerWrapper(const Napi::CallbackInfo &info);
};

class H265RtpPacketizerWrapper : public RtpConfigHandlerBase<H265RtpPacketizerWrapper, rtc::H265RtpPacketizer>
{
public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  explicit H265RtpPacketizerWrapper(const Napi::CallbackInfo &info);
};

class AV1RtpPacketizerWrapper : public RtpConfigHandlerBase<AV1RtpPacketizerWrapper, rtc::AV1RtpPacketizer>
{
public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  explicit AV1RtpPacketizerWrapper(const Napi::CallbackInfo &info);
};

#endif // MEDIA_RTPPACKETIZER_WRAPPER_H
