#include "web-socket-wrapper.h"

#include <cctype>

Napi::Object WebSocketWrapper::Init(Napi::Env env, Napi::Object exports)
{
  Napi::HandleScope scope(env);

  std::vector<PropertyDescriptor> props = {
      InstanceMethod("open", &WebSocketWrapper::open),
      InstanceMethod("forceClose", &WebSocketWrapper::forceClose),
      InstanceMethod("remoteAddress", &WebSocketWrapper::remoteAddress),
      InstanceMethod("path", &WebSocketWrapper::path),
  };
  appendChannelDescriptors(props);

  Napi::Function func = DefineClass(env, "WebSocket", props);
  storeConstructor(env, func);
  exports.Set("WebSocket", func);
  return exports;
}

WebSocketWrapper::WebSocketWrapper(const Napi::CallbackInfo &info) : ChannelWrapperBase(info)
{
  PLOG_DEBUG << "Constructor called";
  Napi::Env env = info.Env();

  // Create WebSocket using rtc::WebSocket provided by WebSocketServer
  if (info.Length() > 1)
  {
    if (!info[1].IsExternal())
    {
      Napi::TypeError::New(env, "Invalid internal WebSocket constructor call").ThrowAsJavaScriptException();
      return;
    }
    auto *ptr = info[1].As<Napi::External<std::shared_ptr<rtc::WebSocket>>>().Data();
    if (!ptr || !*ptr)
    {
      Napi::TypeError::New(env, "Invalid WebSocket handle").ThrowAsJavaScriptException();
      return;
    }
    mChannelPtr = *ptr;
    registerInstance(env);
    return;
  }

  // Create WebSocket without config
  if (info.Length() == 0 || info[0].IsUndefined())
  {
    mCanDetectPrematureClose = false;
    ActiveInstanceGuard guard;
    try
    {
      mChannelPtr = std::make_shared<rtc::WebSocket>();
    }
    catch (std::exception &ex)
    {
      Napi::Error::New(env, std::string("libdatachannel error while creating WebSocket without config: ") + ex.what())
          .ThrowAsJavaScriptException();
      return;
    }
    guard.commit(mAddonData, env, &AddonData::webSockets, this);
    return;
  }

  // Create WebSocket with config
  if (!info[0].IsObject())
  {
    Napi::TypeError::New(env, "Configuration (Object) expected").ThrowAsJavaScriptException();
    return;
  }

  mCanDetectPrematureClose = false;
  Napi::Object config = info[0].As<Napi::Object>();
  rtc::WebSocketConfiguration webSocketConfig;

  auto getDefined = [&](const char *key) -> Napi::Value
  {
    Napi::Value val = config.Get(key);
    return val.IsUndefined() ? Napi::Value() : val;
  };

  if (auto val = getDefined("disableTlsVerification"))
  {
    if (!val.IsBoolean())
    {
      Napi::TypeError::New(env, "disableTlsVerification must be boolean").ThrowAsJavaScriptException();
      return;
    }
    webSocketConfig.disableTlsVerification = val.ToBoolean();
  }

  if (auto val = getDefined("proxyServer"))
  {
    if (!parseProxyServer(env, val, webSocketConfig.proxyServer))
      return;
  }

  if (auto val = getDefined("protocols"))
  {
    if (!val.IsArray())
    {
      Napi::TypeError::New(env, "protocols must be an array").ThrowAsJavaScriptException();
      return;
    }
    Napi::Array protocols = val.As<Napi::Array>();
    for (uint32_t i = 0; i < protocols.Length(); i++)
    {
      Napi::Value item = protocols.Get(i);
      if (!item.IsString())
      {
        Napi::TypeError::New(env, "protocols entries must be strings").ThrowAsJavaScriptException();
        return;
      }
      webSocketConfig.protocols.push_back(item.As<Napi::String>().ToString());
    }
  }

  if (auto val = getDefined("connectionTimeout"))
  {
    int64_t timeoutMs = 0;
    if (!parseNonNegativeDurationMs(env, val, "connectionTimeout", timeoutMs))
      return;
    webSocketConfig.connectionTimeout = std::chrono::milliseconds(timeoutMs);
  }

  if (auto val = getDefined("pingInterval"))
  {
    int64_t intervalMs = 0;
    if (!parseNonNegativeDurationMs(env, val, "pingInterval", intervalMs))
      return;
    webSocketConfig.pingInterval = std::chrono::milliseconds(intervalMs);
  }

  if (auto val = getDefined("maxOutstandingPings"))
  {
    uint32_t pings = 0;
    if (!parseNonNegativeUint32(env, val, "maxOutstandingPings", pings))
      return;
    webSocketConfig.maxOutstandingPings = static_cast<int>(std::min<uint32_t>(pings, std::numeric_limits<int>::max()));
  }

  if (!parseOptionalString(env, config, "caCertificatePemFile", webSocketConfig.caCertificatePemFile) ||
      !parseOptionalString(env, config, "certificatePemFile", webSocketConfig.certificatePemFile) ||
      !parseOptionalString(env, config, "keyPemFile", webSocketConfig.keyPemFile) ||
      !parseOptionalString(env, config, "keyPemPass", webSocketConfig.keyPemPass))
    return;

  if (auto val = getDefined("maxMessageSize"))
  {
    size_t sizeVal = 0;
    if (!parsePositiveSize(env, val, "maxMessageSize", sizeVal))
      return;
    webSocketConfig.maxMessageSize = sizeVal;
  }

  mDisableTlsVerification = webSocketConfig.disableTlsVerification;

#ifdef _WIN32
  if (webSocketConfig.caCertificatePemFile.has_value() && !mDisableTlsVerification)
  {
    Napi::Error::New(env, "TLS certificate verification with caCertificatePemFile is not supported on Windows by "
                          "libdatachannel")
        .ThrowAsJavaScriptException();
    return;
  }
#endif

  ActiveInstanceGuard guard;
  try
  {
    mChannelPtr = std::make_shared<rtc::WebSocket>(webSocketConfig);
  }
  catch (std::exception &ex)
  {
    Napi::Error::New(env, std::string("libdatachannel error while creating WebSocket: ") + ex.what())
        .ThrowAsJavaScriptException();
    return;
  }

  guard.commit(mAddonData, env, &AddonData::webSockets, this);
}

WebSocketWrapper::~WebSocketWrapper()
{
  PLOG_DEBUG << "Destructor called";
  destroyInDestructor();
}

void WebSocketWrapper::open(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();
  if (!mChannelPtr)
  {
    Napi::Error::New(env, "open() called on destroyed WebSocket").ThrowAsJavaScriptException();
    return;
  }
  if (info.Length() < 1 || !info[0].IsString())
  {
    Napi::TypeError::New(env, "url must be string").ThrowAsJavaScriptException();
    return;
  }

  std::string url = info[0].As<Napi::String>().ToString();
#ifdef _WIN32
  if (!mDisableTlsVerification && url.size() >= 4 && std::tolower(static_cast<unsigned char>(url[0])) == 'w' &&
      std::tolower(static_cast<unsigned char>(url[1])) == 's' &&
      std::tolower(static_cast<unsigned char>(url[2])) == 's' && url[3] == ':')
  {
    Napi::Error::New(env, "TLS certificate verification for wss:// is not supported on Windows by libdatachannel; "
                          "set disableTlsVerification: true to opt in explicitly")
        .ThrowAsJavaScriptException();
    return;
  }
#endif

  try
  {
    mChannelPtr->open(url);
    mCanDetectPrematureClose = true;
  }
  catch (std::exception &ex)
  {
    Napi::Error::New(env, std::string("libdatachannel error while opening WebSocket: ") + ex.what())
        .ThrowAsJavaScriptException();
  }
}

void WebSocketWrapper::forceClose(const Napi::CallbackInfo &)
{
  doCloseWith([](rtc::WebSocket &ws) { ws.forceClose(); }, "force closing", /*fromDestructor=*/false,
              /*forceReset=*/true);
}

Napi::Value WebSocketWrapper::remoteAddress(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();
  if (!mChannelPtr)
    return env.Undefined();

  try
  {
    auto address = mChannelPtr->remoteAddress();
    return address.has_value() ? Napi::Value(Napi::String::New(env, address.value())) : env.Undefined();
  }
  catch (std::exception &ex)
  {
    Napi::Error::New(env, std::string("libdatachannel error: ") + ex.what()).ThrowAsJavaScriptException();
    return env.Undefined();
  }
}

Napi::Value WebSocketWrapper::path(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();
  if (!mChannelPtr)
    return env.Undefined();

  try
  {
    auto path = mChannelPtr->path();
    return path.has_value() ? Napi::Value(Napi::String::New(env, path.value())) : env.Undefined();
  }
  catch (std::exception &ex)
  {
    Napi::Error::New(env, std::string("libdatachannel error: ") + ex.what()).ThrowAsJavaScriptException();
    return env.Undefined();
  }
}
