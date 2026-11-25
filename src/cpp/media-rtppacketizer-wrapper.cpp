#include "media-rtppacketizer-wrapper.h"
#include "wrapper-utils.h"

#include <optional>
#include <string>

namespace
{
std::optional<rtc::NalUnit::Separator> strToSeparator(const std::string &sepAsStr)
{
  if (sepAsStr == "Length")
    return rtc::NalUnit::Separator::Length;
  if (sepAsStr == "ShortStartSequence")
    return rtc::NalUnit::Separator::ShortStartSequence;
  if (sepAsStr == "LongStartSequence")
    return rtc::NalUnit::Separator::LongStartSequence;
  if (sepAsStr == "StartSequence")
    return rtc::NalUnit::Separator::StartSequence;
  return std::nullopt;
}

std::optional<rtc::AV1RtpPacketizer::Packetization> strToPacketization(const std::string &packAsStr)
{
  if (packAsStr == "Obu")
    return rtc::AV1RtpPacketizer::Packetization::Obu;
  if (packAsStr == "TemporalUnit")
    return rtc::AV1RtpPacketizer::Packetization::TemporalUnit;
  return std::nullopt;
}

template <typename RtcPacketizer>
std::shared_ptr<RtcPacketizer> createH26xPacketizer(const Napi::CallbackInfo &info, Napi::ObjectReference &outRef)
{
  Napi::Env env = info.Env();
  if (info.Length() < 2)
  {
    Napi::Error::New(env, "Expected 2 or 3 parameters").ThrowAsJavaScriptException();
    return nullptr;
  }

  auto separatorOpt = info[0].IsString() ? strToSeparator(info[0].As<Napi::String>().Utf8Value()) : std::nullopt;
  if (!separatorOpt.has_value())
  {
    Napi::TypeError::New(
        env, "separator must be \"Length\", \"ShortStartSequence\", \"LongStartSequence\" or \"StartSequence\"")
        .ThrowAsJavaScriptException();
    return nullptr;
  }

  auto rtpConfig = extractRtpConfig(env, info[1], outRef);
  if (!rtpConfig)
    return nullptr;

  size_t maxFragmentSize = 0;
  if (!parseOptionalMaxFragmentSize(info, 2, maxFragmentSize))
    return nullptr;

  return std::make_shared<RtcPacketizer>(separatorOpt.value(), rtpConfig, maxFragmentSize);
}
} // namespace

Napi::Object RtpPacketizerWrapper::Init(Napi::Env env, Napi::Object exports)
{
  return RtpPacketizerBase::Init(env, exports, "RtpPacketizer");
}

RtpPacketizerWrapper::RtpPacketizerWrapper(const Napi::CallbackInfo &info) : RtpPacketizerBase(info)
{
  auto rtpConfig = extractRtpConfig(info.Env(), info[0], mRtpConfigObject);
  if (!rtpConfig)
    return;
  mPacketizerPtr = std::make_shared<rtc::RtpPacketizer>(rtpConfig);
  info.This().As<Napi::Object>().TypeTag(&kRtpPacketizerTypeTag);
}

Napi::Object H264RtpPacketizerWrapper::Init(Napi::Env env, Napi::Object exports)
{
  return RtpPacketizerBase::Init(env, exports, "H264RtpPacketizer");
}

H264RtpPacketizerWrapper::H264RtpPacketizerWrapper(const Napi::CallbackInfo &info) : RtpPacketizerBase(info)
{
  mPacketizerPtr = createH26xPacketizer<rtc::H264RtpPacketizer>(info, mRtpConfigObject);
  if (mPacketizerPtr)
    info.This().As<Napi::Object>().TypeTag(&kH264RtpPacketizerTypeTag);
}

Napi::Object H265RtpPacketizerWrapper::Init(Napi::Env env, Napi::Object exports)
{
  return RtpPacketizerBase::Init(env, exports, "H265RtpPacketizer");
}

H265RtpPacketizerWrapper::H265RtpPacketizerWrapper(const Napi::CallbackInfo &info) : RtpPacketizerBase(info)
{
  mPacketizerPtr = createH26xPacketizer<rtc::H265RtpPacketizer>(info, mRtpConfigObject);
  if (mPacketizerPtr)
    info.This().As<Napi::Object>().TypeTag(&kH265RtpPacketizerTypeTag);
}

Napi::Object AV1RtpPacketizerWrapper::Init(Napi::Env env, Napi::Object exports)
{
  return RtpPacketizerBase::Init(env, exports, "AV1RtpPacketizer");
}

AV1RtpPacketizerWrapper::AV1RtpPacketizerWrapper(const Napi::CallbackInfo &info) : RtpPacketizerBase(info)
{
  Napi::Env env = info.Env();
  if (info.Length() < 2)
  {
    Napi::Error::New(env, "Expected 2 or 3 parameters").ThrowAsJavaScriptException();
    return;
  }

  auto packetizationOpt =
      info[0].IsString() ? strToPacketization(info[0].As<Napi::String>().Utf8Value()) : std::nullopt;
  if (!packetizationOpt.has_value())
  {
    Napi::TypeError::New(env, "packetization must be \"Obu\" or \"TemporalUnit\"").ThrowAsJavaScriptException();
    return;
  }

  auto rtpConfig = extractRtpConfig(env, info[1], mRtpConfigObject);
  if (!rtpConfig)
    return;

  size_t maxFragmentSize = 0;
  if (!parseOptionalMaxFragmentSize(info, 2, maxFragmentSize))
    return;

  mPacketizerPtr = std::make_shared<rtc::AV1RtpPacketizer>(packetizationOpt.value(), rtpConfig, maxFragmentSize);
  info.This().As<Napi::Object>().TypeTag(&kAV1RtpPacketizerTypeTag);
}
