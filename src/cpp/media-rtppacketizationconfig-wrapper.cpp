#include "media-rtppacketizationconfig-wrapper.h"
#include "wrapper-utils.h"

Napi::Object RtpPacketizationConfigWrapper::Init(Napi::Env env, Napi::Object exports)
{
  Napi::HandleScope scope(env);

  Napi::Function func = Napi::ObjectWrap<RtpPacketizationConfigWrapper>::DefineClass(
      env, "RtpPacketizationConfig",
      {
          InstanceAccessor("playoutDelayId", &RtpPacketizationConfigWrapper::getPlayoutDelayId,
                           &RtpPacketizationConfigWrapper::setPlayoutDelayId),
          InstanceAccessor("playoutDelayMin", &RtpPacketizationConfigWrapper::getPlayoutDelayMin,
                           &RtpPacketizationConfigWrapper::setPlayoutDelayMin),
          InstanceAccessor("playoutDelayMax", &RtpPacketizationConfigWrapper::getPlayoutDelayMax,
                           &RtpPacketizationConfigWrapper::setPlayoutDelayMax),
          InstanceAccessor("timestamp", &RtpPacketizationConfigWrapper::getTimestamp,
                           &RtpPacketizationConfigWrapper::setTimestamp),
          InstanceAccessor("clockRate", &RtpPacketizationConfigWrapper::getClockRate, nullptr),
      });

  exports.Set("RtpPacketizationConfig", func);
  return exports;
}

RtpPacketizationConfigWrapper::RtpPacketizationConfigWrapper(const Napi::CallbackInfo &info)
    : Napi::ObjectWrap<RtpPacketizationConfigWrapper>(info)
{
  Napi::Env env = info.Env();

  if (info.Length() < 4)
  {
    Napi::Error::New(env, "RtpPacketizationConfig constructor requires 4 or 5 parameters").ThrowAsJavaScriptException();
    return;
  }

  rtc::SSRC ssrc = 0;
  if (!parseNonNegativeUint32(env, info[0], "ssrc", ssrc))
    return;

  if (!info[1].IsString())
  {
    Napi::TypeError::New(env, "cname must be a string").ThrowAsJavaScriptException();
    return;
  }
  std::string cname = info[1].As<Napi::String>().Utf8Value();

  uint8_t payloadType = 0;
  if (!parseUint8(env, info[2], "payloadType", payloadType))
    return;

  uint32_t clockRate = 0;
  if (!parsePositiveUint32(env, info[3], "clockRate", clockRate))
    return;

  uint8_t videoOrientationId = 0;
  if (info.Length() >= 5 && !info[4].IsUndefined())
  {
    if (!parseUint8(env, info[4], "videoOrientationId", videoOrientationId))
      return;
  }
  mConfigPtr = std::make_shared<rtc::RtpPacketizationConfig>(ssrc, cname, payloadType, clockRate, videoOrientationId);
  info.This().As<Napi::Object>().TypeTag(&kRtpPacketizationConfigTypeTag);
}

RtpPacketizationConfigWrapper::~RtpPacketizationConfigWrapper() { mConfigPtr.reset(); }

std::shared_ptr<rtc::RtpPacketizationConfig> RtpPacketizationConfigWrapper::getConfigInstance() { return mConfigPtr; }

Napi::Value RtpPacketizationConfigWrapper::getPlayoutDelayId(const Napi::CallbackInfo &info)
{
  return Napi::Number::New(info.Env(), mConfigPtr->playoutDelayId);
}
void RtpPacketizationConfigWrapper::setPlayoutDelayId(const Napi::CallbackInfo &info, const Napi::Value &val)
{
  uint8_t parsed = 0;
  if (parseUint8(info.Env(), val, "playoutDelayId", parsed))
    mConfigPtr->playoutDelayId = parsed;
}
Napi::Value RtpPacketizationConfigWrapper::getPlayoutDelayMin(const Napi::CallbackInfo &info)
{
  return Napi::Number::New(info.Env(), mConfigPtr->playoutDelayMin);
}
void RtpPacketizationConfigWrapper::setPlayoutDelayMin(const Napi::CallbackInfo &info, const Napi::Value &val)
{
  uint16_t parsed = 0;
  if (parseUint16(info.Env(), val, "playoutDelayMin", parsed))
    mConfigPtr->playoutDelayMin = parsed;
}
Napi::Value RtpPacketizationConfigWrapper::getPlayoutDelayMax(const Napi::CallbackInfo &info)
{
  return Napi::Number::New(info.Env(), mConfigPtr->playoutDelayMax);
}
void RtpPacketizationConfigWrapper::setPlayoutDelayMax(const Napi::CallbackInfo &info, const Napi::Value &val)
{
  uint16_t parsed = 0;
  if (parseUint16(info.Env(), val, "playoutDelayMax", parsed))
    mConfigPtr->playoutDelayMax = parsed;
}
Napi::Value RtpPacketizationConfigWrapper::getTimestamp(const Napi::CallbackInfo &info)
{
  return Napi::Number::New(info.Env(), mConfigPtr->timestamp);
}
void RtpPacketizationConfigWrapper::setTimestamp(const Napi::CallbackInfo &info, const Napi::Value &val)
{
  uint32_t parsed = 0;
  if (parseNonNegativeUint32(info.Env(), val, "timestamp", parsed))
    mConfigPtr->timestamp = parsed;
}
Napi::Value RtpPacketizationConfigWrapper::getClockRate(const Napi::CallbackInfo &info)
{
  return Napi::Number::New(info.Env(), mConfigPtr->clockRate);
}
