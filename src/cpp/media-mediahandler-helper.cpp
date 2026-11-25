#include "media-mediahandler-helper.h"
#include "addon-data.h"
#include "media-handlers-wrapper.h"
#include "media-rtppacketizationconfig-wrapper.h"
#include "media-rtppacketizer-wrapper.h"
#include "wrapper-utils.h"

std::shared_ptr<rtc::MediaHandler> asMediaHandler(const Napi::Object &val)
{
  if (auto *p = unwrapChecked<AV1RtpPacketizerWrapper>(val, &kAV1RtpPacketizerTypeTag))
    return p->getHandlerInstance();
  if (auto *p = unwrapChecked<H264RtpPacketizerWrapper>(val, &kH264RtpPacketizerTypeTag))
    return p->getHandlerInstance();
  if (auto *p = unwrapChecked<H265RtpPacketizerWrapper>(val, &kH265RtpPacketizerTypeTag))
    return p->getHandlerInstance();
  if (auto *p = unwrapChecked<PacingHandlerWrapper>(val, &kPacingHandlerTypeTag))
    return p->getHandlerInstance();
  if (auto *p = unwrapChecked<RtcpNackResponderWrapper>(val, &kRtcpNackResponderTypeTag))
    return p->getHandlerInstance();
  if (auto *p = unwrapChecked<RtcpReceivingSessionWrapper>(val, &kRtcpReceivingSessionTypeTag))
    return p->getHandlerInstance();
  if (auto *p = unwrapChecked<RtcpSrReporterWrapper>(val, &kRtcpSrReporterTypeTag))
    return p->getHandlerInstance();
  if (auto *p = unwrapChecked<RtpPacketizerWrapper>(val, &kRtpPacketizerTypeTag))
    return p->getHandlerInstance();
  return nullptr;
}

void addMediaHandlerToChain(const Napi::CallbackInfo &info, const std::shared_ptr<rtc::MediaHandler> &handler)
{
  auto env = info.Env();
  if (!handler)
  {
    Napi::Error::New(env, "addToChain() called on uninitialized MediaHandler").ThrowAsJavaScriptException();
    return;
  }
  if (info.Length() < 1 || !info[0].IsObject())
  {
    Napi::TypeError::New(env, "Expected a MediaHandler instance").ThrowAsJavaScriptException();
    return;
  }
  auto mediaHandler = asMediaHandler(info[0].As<Napi::Object>());
  if (!mediaHandler)
  {
    Napi::TypeError::New(env, "Expected a MediaHandler instance. If this error is unexpected, please report a bug!")
        .ThrowAsJavaScriptException();
    return;
  }

  // Prevent cycles (h.addToChain(h), a -> b -> a, diamonds) which cause infinite recursion in libdatachannel
  auto tail = handler->last();
  for (auto cur = mediaHandler; cur; cur = cur->next())
  {
    if (cur == tail)
    {
      Napi::Error::New(env, "Cannot add MediaHandler to chain (cycle detected)").ThrowAsJavaScriptException();
      return;
    }
  }
  handler->addToChain(mediaHandler);
}

std::shared_ptr<rtc::RtpPacketizationConfig> extractRtpConfig(Napi::Env env, Napi::Value val,
                                                              Napi::ObjectReference &outRef)
{
  if (!val.IsObject())
  {
    Napi::TypeError::New(env, "rtpConfig must be a RtpPacketizationConfig instance").ThrowAsJavaScriptException();
    return nullptr;
  }
  auto obj = val.As<Napi::Object>();
  auto *cfg = unwrapChecked<RtpPacketizationConfigWrapper>(obj, &kRtpPacketizationConfigTypeTag);
  if (!cfg)
  {
    Napi::TypeError::New(env, "rtpConfig must be a RtpPacketizationConfig instance").ThrowAsJavaScriptException();
    return nullptr;
  }
  outRef = Napi::Persistent(obj);
  return cfg->getConfigInstance();
}

Napi::Value returnRtpConfig(const Napi::CallbackInfo &info, const void *ptr, const Napi::ObjectReference &ref)
{
  Napi::Env env = info.Env();
  if (!ptr)
  {
    Napi::Error::New(env, "getRtpPacketizationConfig() called on destroyed handler").ThrowAsJavaScriptException();
    return env.Null();
  }
  return ref.IsEmpty() ? env.Null() : Napi::Value(ref.Value());
}

bool parseOptionalMaxFragmentSize(const Napi::CallbackInfo &info, size_t argIndex, size_t &outSize, size_t minSize)
{
  outSize = rtc::RtpPacketizer::DefaultMaxFragmentSize;
  if (info.Length() <= argIndex || info[argIndex].IsUndefined())
    return true;
  double maxSize = std::min(kMaxSafeInt, static_cast<double>(std::numeric_limits<size_t>::max()));
  std::string msg = "must be at least " + std::to_string(minSize);
  return parseBoundedInt(info.Env(), info[argIndex], "maxFragmentSize", static_cast<double>(minSize), maxSize,
                         msg.c_str(), outSize);
}
