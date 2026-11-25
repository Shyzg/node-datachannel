#ifndef WEB_SOCKET_SERVER_WRAPPER_H
#define WEB_SOCKET_SERVER_WRAPPER_H

#include <atomic>
#include <memory>

#include <napi.h>
#include <rtc/rtc.hpp>

#include "addon-data.h"
#include "thread-safe-callback.h"
#include "wrapper-utils.h"

class WebSocketServerWrapper : public Napi::ObjectWrap<WebSocketServerWrapper>
{
public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  explicit WebSocketServerWrapper(const Napi::CallbackInfo &info);
  ~WebSocketServerWrapper();

  void stop(const Napi::CallbackInfo &info);
  Napi::Value port(const Napi::CallbackInfo &info);
  void onClient(const Napi::CallbackInfo &info);

  static void StopAll(AddonData *data);
  void doStop(bool fromDestructor = false);

private:
  AddonData *mAddonData = nullptr;
  std::unique_ptr<rtc::WebSocketServer> mWebSocketServerPtr = nullptr;
  std::shared_ptr<std::atomic<bool>> mAlive = std::make_shared<std::atomic<bool>>(true);
  SelfRef mSelfRef;
  std::shared_ptr<ThreadSafeCallback> mOnClientCallback = nullptr;
  Napi::FunctionReference mOnClientFn;
  uint64_t mOnClientGen = 0;
};

#endif // WEB_SOCKET_SERVER_WRAPPER_H
