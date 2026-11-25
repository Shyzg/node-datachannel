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
std::shared_ptr<RtcPacketizer> createH26xPacketizer(const Napi::CallbackInfo &info, Napi::ObjectReference &outRef,
                                                    size_t minFragmentSize)
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
  if (!parseOptionalMaxFragmentSize(info, 2, maxFragmentSize, minFragmentSize))
    return nullptr;

  return std::make_shared<RtcPacketizer>(separatorOpt.value(), rtpConfig, maxFragmentSize);
}
} // namespace

Napi::Object RtpPacketizerWrapper::Init(Napi::Env env, Napi::Object exports)
{
  return RtpConfigHandlerBase::Init(env, exports, "RtpPacketizer");
}

RtpPacketizerWrapper::RtpPacketizerWrapper(const Napi::CallbackInfo &info) : RtpConfigHandlerBase(info)
{
  auto rtpConfig = extractRtpConfig(info.Env(), info[0], mRtpConfigObject);
  if (!rtpConfig)
    return;
  mHandlerPtr = std::make_shared<rtc::RtpPacketizer>(rtpConfig);
  info.This().As<Napi::Object>().TypeTag(&kRtpPacketizerTypeTag);
}

Napi::Object H264RtpPacketizerWrapper::Init(Napi::Env env, Napi::Object exports)
{
  return RtpConfigHandlerBase::Init(env, exports, "H264RtpPacketizer");
}

H264RtpPacketizerWrapper::H264RtpPacketizerWrapper(const Napi::CallbackInfo &info) : RtpConfigHandlerBase(info)
{
  // H.264 FU-A header is 2 bytes, so fragments must be >= 3 bytes to make forward progress.
  mHandlerPtr = createH26xPacketizer<rtc::H264RtpPacketizer>(info, mRtpConfigObject, /*minFragmentSize=*/3);
  if (mHandlerPtr)
    info.This().As<Napi::Object>().TypeTag(&kH264RtpPacketizerTypeTag);
}

Napi::Object H265RtpPacketizerWrapper::Init(Napi::Env env, Napi::Object exports)
{
  return RtpConfigHandlerBase::Init(env, exports, "H265RtpPacketizer");
}

H265RtpPacketizerWrapper::H265RtpPacketizerWrapper(const Napi::CallbackInfo &info) : RtpConfigHandlerBase(info)
{
  // H.265 FU header is 3 bytes, so fragments must be >= 4 bytes to make forward progress.
  mHandlerPtr = createH26xPacketizer<rtc::H265RtpPacketizer>(info, mRtpConfigObject, /*minFragmentSize=*/4);
  if (mHandlerPtr)
    info.This().As<Napi::Object>().TypeTag(&kH265RtpPacketizerTypeTag);
}

Napi::Object AV1RtpPacketizerWrapper::Init(Napi::Env env, Napi::Object exports)
{
  return RtpConfigHandlerBase::Init(env, exports, "AV1RtpPacketizer");
}

AV1RtpPacketizerWrapper::AV1RtpPacketizerWrapper(const Napi::CallbackInfo &info) : RtpConfigHandlerBase(info)
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

  // AV1 aggregation header is 1 byte, so fragments must be >= 2 bytes to make forward progress.
  size_t maxFragmentSize = 0;
  if (!parseOptionalMaxFragmentSize(info, 2, maxFragmentSize, /*minFragmentSize=*/2))
    return;

  mHandlerPtr = std::make_shared<rtc::AV1RtpPacketizer>(packetizationOpt.value(), rtpConfig, maxFragmentSize);
  info.This().As<Napi::Object>().TypeTag(&kAV1RtpPacketizerTypeTag);
}
