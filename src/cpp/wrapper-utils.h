#ifndef WRAPPER_UTILS_H
#define WRAPPER_UTILS_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>

#include <napi.h>
#include <rtc/rtc.hpp>

// Unique 128-bit N-API v8 type tags to prevent prototype-swap type confusion on ObjectWrap::Unwrap
inline constexpr napi_type_tag kVideoTypeTag = {0x9b6d8e4a1f024a11ULL, 0xa1e5c309b4410001ULL};
inline constexpr napi_type_tag kAudioTypeTag = {0x9b6d8e4a1f024a11ULL, 0xa1e5c309b4410002ULL};
inline constexpr napi_type_tag kRtpPacketizationConfigTypeTag = {0x9b6d8e4a1f024a11ULL, 0xa1e5c309b4410003ULL};
inline constexpr napi_type_tag kRtpPacketizerTypeTag = {0x9b6d8e4a1f024a11ULL, 0xa1e5c309b4410004ULL};
inline constexpr napi_type_tag kH264RtpPacketizerTypeTag = {0x9b6d8e4a1f024a11ULL, 0xa1e5c309b4410005ULL};
inline constexpr napi_type_tag kH265RtpPacketizerTypeTag = {0x9b6d8e4a1f024a11ULL, 0xa1e5c309b4410006ULL};
inline constexpr napi_type_tag kAV1RtpPacketizerTypeTag = {0x9b6d8e4a1f024a11ULL, 0xa1e5c309b4410007ULL};
inline constexpr napi_type_tag kPacingHandlerTypeTag = {0x9b6d8e4a1f024a11ULL, 0xa1e5c309b4410008ULL};
inline constexpr napi_type_tag kRtcpNackResponderTypeTag = {0x9b6d8e4a1f024a11ULL, 0xa1e5c309b4410009ULL};
inline constexpr napi_type_tag kRtcpReceivingSessionTypeTag = {0x9b6d8e4a1f024a11ULL, 0xa1e5c309b441000aULL};
inline constexpr napi_type_tag kRtcpSrReporterTypeTag = {0x9b6d8e4a1f024a11ULL, 0xa1e5c309b441000bULL};

inline constexpr double kMaxSafeInt = 9007199254740991.0; // 2^53 - 1

template <typename T>
inline T *unwrapChecked(const Napi::Object &obj, const napi_type_tag *tag)
{
  if (!obj.CheckTypeTag(tag))
    return nullptr;
  return Napi::ObjectWrap<T>::Unwrap(obj);
}

inline void resetFuncRef(Napi::FunctionReference &ref, bool fromDestructor)
{
  if (!ref.IsEmpty())
  {
    if (fromDestructor)
      ref.SuppressDestruct();
    else
      ref.Reset();
  }
}

struct SelfRef
{
  bool active = false;

  template <typename Wrap>
  void acquire(Wrap *self)
  {
    if (!active)
    {
      self->Ref();
      active = true;
    }
  }

  template <typename Wrap>
  void release(Wrap *self, bool fromDestructor = false)
  {
    if (active)
    {
      active = false;
      if (!fromDestructor)
        self->Unref();
    }
  }
};

template <typename IntType>
inline bool parseBoundedInt(Napi::Env env, Napi::Value val, const char *name, double minVal, double maxVal,
                            const char *rangeMsg, IntType &out)
{
  if (!val.IsNumber())
  {
    Napi::TypeError::New(env, std::string(name) + " must be a number").ThrowAsJavaScriptException();
    return false;
  }
  double d = val.As<Napi::Number>().DoubleValue();
  if (!std::isfinite(d) || d < minVal || d > maxVal)
  {
    Napi::RangeError::New(env, std::string(name) + " " + rangeMsg).ThrowAsJavaScriptException();
    return false;
  }
  out = static_cast<IntType>(d);
  return true;
}

inline bool parseUint8(Napi::Env env, Napi::Value val, const char *name, uint8_t &out)
{
  return parseBoundedInt(env, val, name, 0.0, 255.0, "must be between 0 and 255", out);
}

inline bool parseUint16(Napi::Env env, Napi::Value val, const char *name, uint16_t &out)
{
  return parseBoundedInt(env, val, name, 0.0, 65535.0, "must be between 0 and 65535", out);
}

inline bool parseUint16Port(Napi::Env env, Napi::Value val, const char *name, uint16_t &out)
{
  return parseUint16(env, val, name, out);
}

inline bool parseNonNegativeUint32(Napi::Env env, Napi::Value val, const char *name, uint32_t &out)
{
  return parseBoundedInt(env, val, name, 0.0, static_cast<double>(std::numeric_limits<uint32_t>::max()),
                         "must be between 0 and 4294967295", out);
}

inline bool parsePositiveUint32(Napi::Env env, Napi::Value val, const char *name, uint32_t &out)
{
  return parseBoundedInt(env, val, name, 1.0, static_cast<double>(std::numeric_limits<uint32_t>::max()),
                         "must be positive (<= 4294967295)", out);
}

inline bool parseNonNegativeSize(Napi::Env env, Napi::Value val, const char *name, size_t &out)
{
  double maxSize = std::min(kMaxSafeInt, static_cast<double>(std::numeric_limits<size_t>::max()));
  return parseBoundedInt(env, val, name, 0.0, maxSize, "must be non-negative", out);
}

inline bool parsePositiveSize(Napi::Env env, Napi::Value val, const char *name, size_t &out)
{
  double maxSize = std::min(kMaxSafeInt, static_cast<double>(std::numeric_limits<size_t>::max()));
  return parseBoundedInt(env, val, name, 1.0, maxSize, "must be positive", out);
}

inline bool parseNonNegativeInt64(Napi::Env env, Napi::Value val, const char *name, int64_t &out)
{
  return parseBoundedInt(env, val, name, 0.0, kMaxSafeInt, "must be non-negative", out);
}

inline bool parsePositiveInt64(Napi::Env env, Napi::Value val, const char *name, int64_t &out)
{
  return parseBoundedInt(env, val, name, 1.0, kMaxSafeInt, "must be positive", out);
}

inline bool parseProxyServer(Napi::Env env, Napi::Value val, std::optional<rtc::ProxyServer> &out)
{
  if (!val.IsObject())
  {
    Napi::TypeError::New(env, "proxyServer must be an object").ThrowAsJavaScriptException();
    return false;
  }

  Napi::Object proxyServer = val.As<Napi::Object>();
  Napi::Value ipVal = proxyServer.Get("ip");
  Napi::Value portVal = proxyServer.Get("port");
  Napi::Value typeVal = proxyServer.Get("type");
  if (!ipVal.IsString() || !portVal.IsNumber() || !typeVal.IsString())
  {
    Napi::TypeError::New(env, "proxyServer requires ip (String), port (Number), and type (String)")
        .ThrowAsJavaScriptException();
    return false;
  }

  uint16_t port = 0;
  if (!parseUint16Port(env, portVal, "proxyServer.port", port))
    return false;

  std::string strType = typeVal.As<Napi::String>().ToString();
  rtc::ProxyServer::Type type;
  if (strType == "Socks5")
    type = rtc::ProxyServer::Type::Socks5;
  else if (strType == "Http")
    type = rtc::ProxyServer::Type::Http;
  else
  {
    Napi::TypeError::New(env, "proxyServer.type must be \"Socks5\" or \"Http\"").ThrowAsJavaScriptException();
    return false;
  }

  std::string ip = ipVal.As<Napi::String>().ToString();
  std::string username;
  std::string password;
  if (proxyServer.Get("username").IsString())
    username = proxyServer.Get("username").As<Napi::String>().ToString();
  if (proxyServer.Get("password").IsString())
    password = proxyServer.Get("password").As<Napi::String>().ToString();

  out = rtc::ProxyServer(type, ip, port, username, password);
  return true;
}

#endif // WRAPPER_UTILS_H
