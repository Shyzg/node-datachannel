#include "rtc-wrapper.h"
#include "peer-connection-wrapper.h"
#include "data-channel-wrapper.h"
#include "ice-udp-mux-listener-wrapper.h"
#include "wrapper-utils.h"

#if RTC_ENABLE_MEDIA == 1
#include "media-track-wrapper.h"
#endif

#if RTC_ENABLE_WEBSOCKET == 1
#include "web-socket-wrapper.h"
#include "web-socket-server-wrapper.h"
#endif

#include "plog/Log.h"

#include <chrono>
#include <future>

Napi::Object RtcWrapper::Init(Napi::Env env, Napi::Object exports)
{
  Napi::HandleScope scope(env);

  exports.Set("initLogger", Napi::Function::New(env, &RtcWrapper::initLogger));
  exports.Set("cleanup", Napi::Function::New(env, &RtcWrapper::cleanup));
  exports.Set("preload", Napi::Function::New(env, &RtcWrapper::preload));
  exports.Set("setSctpSettings", Napi::Function::New(env, &RtcWrapper::setSctpSettings));
  exports.Set("getLibraryVersion", Napi::Function::New(env, &RtcWrapper::getLibraryVersion));

  return exports;
}

void RtcWrapper::preload(const Napi::CallbackInfo &info)
{
  PLOG_DEBUG << "preload() called";
  Napi::Env env = info.Env();
  try
  {
    rtc::Preload();
  }
  catch (std::exception &ex)
  {
    Napi::Error::New(env, std::string("libdatachannel error# ") + ex.what()).ThrowAsJavaScriptException();
  }
}

void RtcWrapper::clearLogCallbackIfActive(AddonData *data, bool fromDestructor)
{
  if (!data)
    return;
  if (data->logAlive)
    data->logAlive->store(false);
  ++data->logGen;
  if (data->logCallback)
  {
    std::lock_guard<std::mutex> lock(logMutex);
    if (activeLogCallback.lock() == data->logCallback)
    {
      rtc::InitLogger(rtc::LogLevel::None, nullptr);
      activeLogCallback.reset();
    }
    resetCallback(data->logCallback);
  }
  resetFuncRef(data->logFn, fromDestructor);
}

void RtcWrapper::initLogger(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();
  AddonData *data = getAddonData(env);
  int length = info.Length();

  // We expect (String, Function?) as param
  if (length < 1 || !info[0].IsString())
  {
    Napi::TypeError::New(env, "LogLevel(String) expected").ThrowAsJavaScriptException();
    return;
  }

  std::string logLevelStr = info[0].As<Napi::String>().ToString();
  rtc::LogLevel logLevel = rtc::LogLevel::None;

  if (logLevelStr == "Verbose")
    logLevel = rtc::LogLevel::Verbose;
  else if (logLevelStr == "Debug")
    logLevel = rtc::LogLevel::Debug;
  else if (logLevelStr == "Info")
    logLevel = rtc::LogLevel::Info;
  else if (logLevelStr == "Warning")
    logLevel = rtc::LogLevel::Warning;
  else if (logLevelStr == "Error")
    logLevel = rtc::LogLevel::Error;
  else if (logLevelStr == "Fatal")
    logLevel = rtc::LogLevel::Fatal;
  else if (logLevelStr == "None")
    logLevel = rtc::LogLevel::None;
  else
  {
    Napi::TypeError::New(
        env, "Unknown LogLevel, expected \"Verbose\", \"Debug\", \"Info\", \"Warning\", \"Error\", \"Fatal\", or \"None\"")
        .ThrowAsJavaScriptException();
    return;
  }

  try
  {
    if (length < 2 || info[1].IsUndefined())
    {
      std::lock_guard<std::mutex> lock(logMutex);
      if (data->logAlive)
        data->logAlive->store(false);
      ++data->logGen;
      resetCallback(data->logCallback);
      resetFuncRef(data->logFn, false);
      activeLogCallback.reset();
      rtc::InitLogger(logLevel, nullptr);
    }
    else
    {
      if (!info[1].IsFunction())
      {
        Napi::TypeError::New(env, "Function expected").ThrowAsJavaScriptException();
        return;
      }
      auto cb = std::make_shared<ThreadSafeCallback>(env, /*unref=*/true);
      auto alive = std::make_shared<std::atomic<bool>>(true);
      Napi::FunctionReference fnRef = Napi::Persistent(info[1].As<Napi::Function>());
      std::lock_guard<std::mutex> lock(logMutex);
      if (data->logAlive)
        data->logAlive->store(false);
      resetCallback(data->logCallback);
      resetFuncRef(data->logFn, false);
      data->logCallback = cb;
      data->logAlive = alive;
      data->logFn = std::move(fnRef);
      uint64_t gen = ++data->logGen;
      activeLogCallback = cb;
      rtc::InitLogger(logLevel,
                      [data, alive, cb = std::move(cb), gen](rtc::LogLevel level, std::string message)
                      {
                        if (!alive->load())
                          return;
                        cb->dispatch(
                            [data, alive, gen, level, message = std::move(message)](Napi::Env cbEnv)
                            {
                              if (!alive->load() || data->logGen != gen || data->logFn.IsEmpty())
                                throw ThreadSafeCallback::CancelException();
                              const char *logLevelName = "";
                              switch (level)
                              {
                              case rtc::LogLevel::Verbose:
                                logLevelName = "Verbose";
                                break;
                              case rtc::LogLevel::Debug:
                                logLevelName = "Debug";
                                break;
                              case rtc::LogLevel::Info:
                                logLevelName = "Info";
                                break;
                              case rtc::LogLevel::Warning:
                                logLevelName = "Warning";
                                break;
                              case rtc::LogLevel::Error:
                                logLevelName = "Error";
                                break;
                              case rtc::LogLevel::Fatal:
                                logLevelName = "Fatal";
                                break;
                              default:
                                break;
                              }
                              data->logFn.Call(
                                  {Napi::String::New(cbEnv, logLevelName), Napi::String::New(cbEnv, message)});
                            });
                      });
    }
  }
  catch (std::exception &ex)
  {
    Napi::Error::New(env, std::string("libdatachannel error# ") + ex.what()).ThrowAsJavaScriptException();
    return;
  }
}

void RtcWrapper::cleanup(const Napi::CallbackInfo &info)
{
  PLOG_DEBUG << "cleanup() called";
  Napi::Env env = info.Env();
  AddonData *data = getAddonData(env);
  try
  {
    PeerConnectionWrapper::CloseAll(data);
    DataChannelWrapper::CloseAll(data);
    IceUdpMuxListenerWrapper::StopAll(data);

#if RTC_ENABLE_MEDIA == 1
    TrackWrapper::CloseAll(data);
#endif

#if RTC_ENABLE_WEBSOCKET == 1
    WebSocketWrapper::CloseAll(data);
    WebSocketServerWrapper::StopAll(data);
#endif

    // Clean up wrappers and cancel callback queues BEFORE rtc::Cleanup() so queued native handles
    // (such as accepted rtc::WebSocket/DataChannel/Track objects) are released synchronously.
    PeerConnectionWrapper::CleanupAll(data);
    DataChannelWrapper::CleanupAll(data);

#if RTC_ENABLE_MEDIA == 1
    TrackWrapper::CleanupAll(data);
#endif

#if RTC_ENABLE_WEBSOCKET == 1
    WebSocketWrapper::CleanupAll(data);
#endif

    clearLogCallbackIfActive(data);

    // Only run process-global rtc::Cleanup() when no other Node workers still hold active instances.
    // Unique lock blocks concurrent constructors until cleanup finishes, preventing a race where
    // another worker pins libdatachannel's init_token and causes rtc::Cleanup() to time out.
    bool timedOut = false;
    {
      std::unique_lock<std::shared_mutex> lock(gLifecycleMutex);
      if (gActiveNativeInstances.load() == 0)
      {
        const auto timeout = std::chrono::seconds(10);
        timedOut = rtc::Cleanup().wait_for(timeout) == std::future_status::timeout;
      }
    }

    if (timedOut)
      throw std::runtime_error("cleanup timeout (possible deadlock)");
  }
  catch (std::exception &ex)
  {
    Napi::Error::New(env, std::string("libdatachannel error# ") + ex.what()).ThrowAsJavaScriptException();
    return;
  }
}

void RtcWrapper::setSctpSettings(const Napi::CallbackInfo &info)
{
  PLOG_DEBUG << "setSctpSettings() called";
  Napi::Env env = info.Env();
  int length = info.Length();

  // We expect (Object) as param
  if (length < 1 || !info[0].IsObject())
  {
    Napi::TypeError::New(env, "Configuration (Object) expected").ThrowAsJavaScriptException();
    return;
  }

  rtc::SctpSettings settings;
  Napi::Object config = info[0].As<Napi::Object>();

  auto parseSetting = [&](const char *key, auto setter) -> bool
  {
    Napi::Value val = config.Get(key);
    if (val.IsUndefined())
      return true;
    uint32_t parsed = 0;
    if (!parseNonNegativeUint32(env, val, key, parsed))
      return false;
    setter(parsed);
    return true;
  };

  if (!parseSetting("recvBufferSize", [&](uint32_t v) { settings.recvBufferSize = v; }))
    return;
  if (!parseSetting("sendBufferSize", [&](uint32_t v) { settings.sendBufferSize = v; }))
    return;
  if (!parseSetting("maxChunksOnQueue", [&](uint32_t v) { settings.maxChunksOnQueue = v; }))
    return;
  if (!parseSetting("initialCongestionWindow", [&](uint32_t v) { settings.initialCongestionWindow = v; }))
    return;
  if (!parseSetting("congestionControlModule", [&](uint32_t v) { settings.congestionControlModule = v; }))
    return;
  if (!parseSetting("delayedSackTime", [&](uint32_t v) { settings.delayedSackTime = std::chrono::milliseconds(v); }))
    return;

  rtc::SetSctpSettings(settings);
}

Napi::Value RtcWrapper::getLibraryVersion(const Napi::CallbackInfo &info)
{
  PLOG_DEBUG << "getLibraryVersion() called";
  return Napi::String::New(info.Env(), RTC_VERSION);
}
