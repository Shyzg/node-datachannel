#ifndef WEB_SOCKET_WRAPPER_H
#define WEB_SOCKET_WRAPPER_H

#include "channel-wrapper-base.h"

class WebSocketWrapper
    : public ChannelWrapperBase<WebSocketWrapper, rtc::WebSocket, &AddonData::webSockets, &AddonData::webSocketCtor,
                                /*BinaryOnlyMessages=*/false, /*GracefulClose=*/true>
{
public:
  static constexpr const char *kChannelLabel = "WebSocket";

  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  explicit WebSocketWrapper(const Napi::CallbackInfo &info);
  ~WebSocketWrapper();

  void open(const Napi::CallbackInfo &info);
  void forceClose(const Napi::CallbackInfo &info);
  Napi::Value remoteAddress(const Napi::CallbackInfo &info);
  Napi::Value path(const Napi::CallbackInfo &info);
};

#endif // WEB_SOCKET_WRAPPER_H
