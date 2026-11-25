#include "web-socket-server-wrapper.h"
#include "web-socket-wrapper.h"

#include "plog/Log.h"

#include <iostream>

void WebSocketServerWrapper::StopAll(AddonData *data)
{
  if (!data)
    return;
  PLOG_DEBUG << "StopAll() called";
  auto copy = data->webSocketServers;
  for (auto *inst : copy)
    inst->doStop();
}

Napi::Object WebSocketServerWrapper::Init(Napi::Env env, Napi::Object exports)
{
  Napi::HandleScope scope(env);

  Napi::Function func = DefineClass(env, "WebSocketServer",
                                    {InstanceMethod("stop", &WebSocketServerWrapper::stop),
                                     InstanceMethod("port", &WebSocketServerWrapper::port),
                                     InstanceMethod("onClient", &WebSocketServerWrapper::onClient)});

  exports.Set("WebSocketServer", func);
  return exports;
}

WebSocketServerWrapper::WebSocketServerWrapper(const Napi::CallbackInfo &info)
    : Napi::ObjectWrap<WebSocketServerWrapper>(info)
{
  PLOG_DEBUG << "Constructor called";
  Napi::Env env = info.Env();

  if (info.Length() == 0 || info[0].IsUndefined())
  {
    ActiveInstanceGuard guard;
    try
    {
      mWebSocketServerPtr = std::make_unique<rtc::WebSocketServer>();
    }
    catch (std::exception &ex)
    {
      Napi::Error::New(env,
                       std::string("libdatachannel error while creating WebSocketServer without config: ") + ex.what())
          .ThrowAsJavaScriptException();
      return;
    }

    guard.commit(mAddonData, env, &AddonData::webSocketServers, this);
    return;
  }

  if (!info[0].IsObject())
  {
    Napi::TypeError::New(env, "Configuration (Object) expected").ThrowAsJavaScriptException();
    return;
  }

  Napi::Object config = info[0].As<Napi::Object>();
  rtc::WebSocketServerConfiguration webSocketServerConfig;

  auto getDefined = [&](const char *key) -> Napi::Value
  {
    Napi::Value val = config.Get(key);
    return val.IsUndefined() ? Napi::Value() : val;
  };

  if (auto val = getDefined("port"))
  {
    uint16_t port = 0;
    if (!parseUint16(env, val, "port", port))
      return;
    webSocketServerConfig.port = port;
  }

  if (!parseOptionalBool(env, config, "enableTls", webSocketServerConfig.enableTls))
    return;

  if (!parseOptionalString(env, config, "certificatePemFile", webSocketServerConfig.certificatePemFile) ||
      !parseOptionalString(env, config, "keyPemFile", webSocketServerConfig.keyPemFile) ||
      !parseOptionalString(env, config, "keyPemPass", webSocketServerConfig.keyPemPass) ||
      !parseOptionalString(env, config, "bindAddress", webSocketServerConfig.bindAddress))
    return;

  if (auto val = getDefined("connectionTimeout"))
  {
    int64_t timeoutMs = 0;
    if (!parseNonNegativeDurationMs(env, val, "connectionTimeout", timeoutMs))
      return;
    webSocketServerConfig.connectionTimeout = std::chrono::milliseconds(timeoutMs);
  }

  if (auto val = getDefined("maxMessageSize"))
  {
    size_t sizeVal = 0;
    if (!parsePositiveSize(env, val, "maxMessageSize", sizeVal))
      return;
    webSocketServerConfig.maxMessageSize = sizeVal;
  }

  ActiveInstanceGuard guard;
  try
  {
    mWebSocketServerPtr = std::make_unique<rtc::WebSocketServer>(webSocketServerConfig);
  }
  catch (std::exception &ex)
  {
    Napi::Error::New(env, std::string("libdatachannel error while creating WebSocketServer: ") + ex.what())
        .ThrowAsJavaScriptException();
    return;
  }

  guard.commit(mAddonData, env, &AddonData::webSocketServers, this);
}

WebSocketServerWrapper::~WebSocketServerWrapper()
{
  PLOG_DEBUG << "Destructor called";
  doStop(/*fromDestructor=*/true);
}

void WebSocketServerWrapper::doStop(bool fromDestructor)
{
  PLOG_DEBUG << "doStop() called";
  mAlive->store(false);
  ++mOnClientGen;
  resetCallback(mOnClientCallback);
  resetFuncRef(mOnClientFn, fromDestructor);

  if (mWebSocketServerPtr)
  {
    try
    {
      mWebSocketServerPtr->onClient(nullptr);
      mWebSocketServerPtr->stop();
    }
    catch (std::exception &ex)
    {
      std::cerr << std::string("libdatachannel error while closing WebSocketServer: ") + ex.what() << std::endl;
    }
    mWebSocketServerPtr.reset();
  }

  if (mAddonData)
  {
    if (mAddonData->webSocketServers.erase(this) > 0)
      --gActiveNativeInstances;
    mAddonData = nullptr;
  }

  mSelfRef.release(this, fromDestructor);
}

void WebSocketServerWrapper::stop(const Napi::CallbackInfo &) { doStop(); }

Napi::Value WebSocketServerWrapper::port(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();
  if (!mWebSocketServerPtr)
    return Napi::Number::New(env, 0);

  try
  {
    return Napi::Number::New(env, mWebSocketServerPtr->port());
  }
  catch (std::exception &ex)
  {
    Napi::Error::New(env, std::string("WebSocketServer error: ") + ex.what()).ThrowAsJavaScriptException();
    return Napi::Number::New(env, 0);
  }
}

void WebSocketServerWrapper::onClient(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();
  if (!mWebSocketServerPtr)
  {
    Napi::Error::New(env, "onClient() called on destroyed WebSocketServer").ThrowAsJavaScriptException();
    return;
  }

  if (info.Length() < 1 || !info[0].IsFunction())
  {
    Napi::TypeError::New(env, "Function expected as onClient callback").ThrowAsJavaScriptException();
    return;
  }

  mSelfRef.acquire(this);
  if (!mOnClientCallback)
    mOnClientCallback = std::make_shared<ThreadSafeCallback>(env, /*unref=*/false);
  auto cb = mOnClientCallback;
  mOnClientFn = Napi::Persistent(info[0].As<Napi::Function>());
  uint64_t gen = ++mOnClientGen;

  mWebSocketServerPtr->onClient(
      [this, alive = mAlive, cb = std::move(cb), gen](std::shared_ptr<rtc::WebSocket> ws)
      {
        if (!alive->load())
          return;
        cb->dispatch(
            [this, alive, gen, ws = std::move(ws)](Napi::Env cbEnv) mutable
            {
              if (!alive->load() || mOnClientGen != gen || mOnClientFn.IsEmpty())
                throw ThreadSafeCallback::CancelException();

              auto *data = getAddonData(cbEnv);
              auto instance = data->webSocketCtor.New(
                  {Napi::Boolean::New(cbEnv, false), Napi::External<std::shared_ptr<rtc::WebSocket>>::New(cbEnv, &ws)});
              mOnClientFn.Call({instance});
            });
      });
}
