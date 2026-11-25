#ifndef MEDIA_DESCRIPTION_BASE_H
#define MEDIA_DESCRIPTION_BASE_H

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <napi.h>
#include <rtc/rtc.hpp>

#include "media-direction.h"
#include "wrapper-utils.h"

template <typename Derived, typename RtcMedia>
class MediaDescriptionBase : public Napi::ObjectWrap<Derived>
{
public:
  explicit MediaDescriptionBase(const Napi::CallbackInfo &info) : Napi::ObjectWrap<Derived>(info) {}

  Napi::Value direction(const Napi::CallbackInfo &info)
  {
    return Napi::String::New(info.Env(), directionToStr(mMediaPtr->direction()));
  }

  Napi::Value generateSdp(const Napi::CallbackInfo &info)
  {
    Napi::Env env = info.Env();
    if (info.Length() < 3 || !info[0].IsString() || !info[1].IsString() || !info[2].IsNumber())
    {
      Napi::TypeError::New(env, "We expect (String, String, Number) as param").ThrowAsJavaScriptException();
      return Napi::String::New(env, "");
    }

    std::string eol = info[0].As<Napi::String>().ToString();
    std::string addr = info[1].As<Napi::String>().ToString();
    uint16_t port = 0;
    if (!parseUint16(env, info[2], "port", port))
      return Napi::String::New(env, "");
    return Napi::String::New(env, mMediaPtr->generateSdp(eol, addr, port));
  }

  Napi::Value mid(const Napi::CallbackInfo &info) { return Napi::String::New(info.Env(), mMediaPtr->mid()); }

  void setDirection(const Napi::CallbackInfo &info)
  {
    Napi::Env env = info.Env();
    if (info.Length() < 1 || !info[0].IsString())
    {
      Napi::TypeError::New(env, "We expect (String) as param").ThrowAsJavaScriptException();
      return;
    }

    mMediaPtr->setDirection(strToDirection(info[0].As<Napi::String>().ToString()));
  }

  Napi::Value description(const Napi::CallbackInfo &info)
  {
    return Napi::String::New(info.Env(), mMediaPtr->description());
  }

  void removeFormat(const Napi::CallbackInfo &info)
  {
    Napi::Env env = info.Env();
    if (info.Length() < 1 || !info[0].IsString())
    {
      Napi::TypeError::New(env, "We expect (String) as param").ThrowAsJavaScriptException();
      return;
    }

    mMediaPtr->removeFormat(info[0].As<Napi::String>().ToString());
  }

  void addSSRC(const Napi::CallbackInfo &info)
  {
    Napi::Env env = info.Env();
    int length = info.Length();
    if (length < 1 || !info[0].IsNumber())
    {
      Napi::TypeError::New(env, "We expect (Number, String[optional], String[optional], String[optional]) as param")
          .ThrowAsJavaScriptException();
      return;
    }

    uint32_t ssrc = 0;
    if (!parseNonNegativeUint32(env, info[0], "ssrc", ssrc))
      return;
    std::optional<std::string> name;
    std::optional<std::string> msid;
    std::optional<std::string> trackID;
    if (!parseOptionalSsrcStrings(info, 1, name, msid, trackID))
      return;

    mMediaPtr->addSSRC(ssrc, name, msid, trackID);
  }

  void removeSSRC(const Napi::CallbackInfo &info)
  {
    Napi::Env env = info.Env();
    if (info.Length() < 1 || !info[0].IsNumber())
    {
      Napi::TypeError::New(env, "We expect (Number) as param").ThrowAsJavaScriptException();
      return;
    }

    uint32_t ssrc = 0;
    if (!parseNonNegativeUint32(env, info[0], "ssrc", ssrc))
      return;
    mMediaPtr->removeSSRC(ssrc);
  }

  void replaceSSRC(const Napi::CallbackInfo &info)
  {
    Napi::Env env = info.Env();
    int length = info.Length();
    if (length < 2 || !info[0].IsNumber() || !info[1].IsNumber())
    {
      Napi::TypeError::New(env,
                           "We expect (Number, Number, String[optional], String[optional], String[optional]) as param")
          .ThrowAsJavaScriptException();
      return;
    }

    uint32_t oldSsrc = 0;
    uint32_t ssrc = 0;
    if (!parseNonNegativeUint32(env, info[0], "oldSsrc", oldSsrc) || !parseNonNegativeUint32(env, info[1], "ssrc", ssrc))
      return;
    std::optional<std::string> name;
    std::optional<std::string> msid;
    std::optional<std::string> trackID;
    if (!parseOptionalSsrcStrings(info, 2, name, msid, trackID))
      return;

    mMediaPtr->replaceSSRC(oldSsrc, ssrc, name, msid, trackID);
  }

  Napi::Value hasSSRC(const Napi::CallbackInfo &info)
  {
    Napi::Env env = info.Env();
    if (info.Length() < 1 || !info[0].IsNumber())
    {
      Napi::TypeError::New(env, "We expect (Number) as param").ThrowAsJavaScriptException();
      return env.Null();
    }

    uint32_t ssrc = 0;
    if (!parseNonNegativeUint32(env, info[0], "ssrc", ssrc))
      return env.Null();
    return Napi::Boolean::New(env, mMediaPtr->hasSSRC(ssrc));
  }

  Napi::Value getSSRCs(const Napi::CallbackInfo &info)
  {
    Napi::Env env = info.Env();
    auto list = mMediaPtr->getSSRCs();
    Napi::Uint32Array napiArr = Napi::Uint32Array::New(env, list.size());
    for (size_t i = 0; i < list.size(); i++)
      napiArr[i] = list[i];
    return napiArr;
  }

  Napi::Value getCNameForSsrc(const Napi::CallbackInfo &info)
  {
    Napi::Env env = info.Env();
    if (info.Length() < 1 || !info[0].IsNumber())
    {
      Napi::TypeError::New(env, "We expect (Number) as param").ThrowAsJavaScriptException();
      return env.Null();
    }

    uint32_t ssrc = 0;
    if (!parseNonNegativeUint32(env, info[0], "ssrc", ssrc))
      return env.Null();
    auto name = mMediaPtr->getCNameForSsrc(ssrc);
    return name.has_value() ? Napi::Value(Napi::String::New(env, name.value())) : env.Null();
  }

  void setBitrate(const Napi::CallbackInfo &info)
  {
    Napi::Env env = info.Env();
    if (info.Length() < 1 || !info[0].IsNumber())
    {
      Napi::TypeError::New(env, "We expect (Number) as param").ThrowAsJavaScriptException();
      return;
    }

    uint32_t bitrate = 0;
    if (!parseNonNegativeUint32(env, info[0], "bitRate", bitrate))
      return;
    mMediaPtr->setBitrate(static_cast<int>(std::min<uint32_t>(bitrate, std::numeric_limits<int>::max())));
  }

  Napi::Value getBitrate(const Napi::CallbackInfo &info)
  {
    return Napi::Number::New(info.Env(), mMediaPtr->bitrate());
  }

  Napi::Value hasPayloadType(const Napi::CallbackInfo &info)
  {
    Napi::Env env = info.Env();
    if (info.Length() < 1 || !info[0].IsNumber())
    {
      Napi::TypeError::New(env, "We expect (Number) as param").ThrowAsJavaScriptException();
      return env.Null();
    }

    return Napi::Boolean::New(env, mMediaPtr->hasPayloadType(info[0].As<Napi::Number>().Int32Value()));
  }

  void addRTXCodec(const Napi::CallbackInfo &info)
  {
    Napi::Env env = info.Env();
    if (info.Length() < 3 || !info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsNumber())
    {
      Napi::TypeError::New(env, "We expect (Number,Number,Number) as param").ThrowAsJavaScriptException();
      return;
    }

    int payloadType = info[0].As<Napi::Number>().Int32Value();
    int originalPayloadType = info[1].As<Napi::Number>().Int32Value();
    uint32_t clockRate = 0;
    if (!parseNonNegativeUint32(env, info[2], "clockRate", clockRate))
      return;
    mMediaPtr->addRtxCodec(payloadType, originalPayloadType, clockRate);
  }

  void parseSdpLine(const Napi::CallbackInfo &info)
  {
    Napi::Env env = info.Env();
    if (info.Length() < 1 || !info[0].IsString())
    {
      Napi::TypeError::New(env, "We expect (String) as param").ThrowAsJavaScriptException();
      return;
    }

    mMediaPtr->parseSdpLine(info[0].As<Napi::String>().Utf8Value());
  }

protected:
  using PropertyDescriptor = typename Napi::ObjectWrap<Derived>::PropertyDescriptor;

  bool initMedia(const Napi::CallbackInfo &info, std::string defaultMid, rtc::Description::Direction defaultDir)
  {
    Napi::Env env = info.Env();
    int length = info.Length();

    if (length > 0 && !info[0].IsUndefined())
    {
      if (!info[0].IsString())
      {
        Napi::TypeError::New(env, "mid (String) expected").ThrowAsJavaScriptException();
        return false;
      }
      defaultMid = info[0].As<Napi::String>().ToString();
    }

    if (length > 1 && !info[1].IsUndefined())
    {
      if (!info[1].IsString())
      {
        Napi::TypeError::New(env, "direction (String) expected").ThrowAsJavaScriptException();
        return false;
      }
      defaultDir = strToDirection(info[1].As<Napi::String>().ToString());
    }

    mMediaPtr = std::make_unique<RtcMedia>(defaultMid, defaultDir);
    return true;
  }

  static void appendBaseDescriptors(std::vector<PropertyDescriptor> &props)
  {
    using Base = MediaDescriptionBase<Derived, RtcMedia>;
    props.push_back(Derived::InstanceMethod("direction", &Base::direction));
    props.push_back(Derived::InstanceMethod("generateSdp", &Base::generateSdp));
    props.push_back(Derived::InstanceMethod("mid", &Base::mid));
    props.push_back(Derived::InstanceMethod("setDirection", &Base::setDirection));
    props.push_back(Derived::InstanceMethod("description", &Base::description));
    props.push_back(Derived::InstanceMethod("removeFormat", &Base::removeFormat));
    props.push_back(Derived::InstanceMethod("addSSRC", &Base::addSSRC));
    props.push_back(Derived::InstanceMethod("removeSSRC", &Base::removeSSRC));
    props.push_back(Derived::InstanceMethod("replaceSSRC", &Base::replaceSSRC));
    props.push_back(Derived::InstanceMethod("hasSSRC", &Base::hasSSRC));
    props.push_back(Derived::InstanceMethod("getSSRCs", &Base::getSSRCs));
    props.push_back(Derived::InstanceMethod("getCNameForSsrc", &Base::getCNameForSsrc));
    props.push_back(Derived::InstanceMethod("setBitrate", &Base::setBitrate));
    props.push_back(Derived::InstanceMethod("getBitrate", &Base::getBitrate));
    props.push_back(Derived::InstanceMethod("hasPayloadType", &Base::hasPayloadType));
    props.push_back(Derived::InstanceMethod("addRTXCodec", &Base::addRTXCodec));
    props.push_back(Derived::InstanceMethod("parseSdpLine", &Base::parseSdpLine));
  }

  std::unique_ptr<RtcMedia> mMediaPtr = nullptr;

private:
  static bool parseOptionalSsrcStrings(const Napi::CallbackInfo &info, int startIdx, std::optional<std::string> &name,
                                       std::optional<std::string> &msid, std::optional<std::string> &trackID)
  {
    Napi::Env env = info.Env();
    int length = info.Length();
    const char *labels[3] = {"name as String expected", "msid as String expected", "trackID as String expected"};
    std::optional<std::string> *targets[3] = {&name, &msid, &trackID};

    for (int i = 0; i < 3; i++)
    {
      int idx = startIdx + i;
      if (length > idx && !info[idx].IsUndefined())
      {
        if (!info[idx].IsString())
        {
          Napi::TypeError::New(env, labels[i]).ThrowAsJavaScriptException();
          return false;
        }
        *targets[i] = info[idx].As<Napi::String>().ToString();
      }
    }
    return true;
  }
};

#endif // MEDIA_DESCRIPTION_BASE_H
