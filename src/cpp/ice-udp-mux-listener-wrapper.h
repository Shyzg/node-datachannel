#ifndef ICE_UDP_MUX_LISTENER_WRAPPER_H
#define ICE_UDP_MUX_LISTENER_WRAPPER_H

#include <atomic>
#include <memory>
#include <optional>
#include <string>

#include <napi.h>
#include <rtc/rtc.hpp>

#include "addon-data.h"
#include "thread-safe-callback.h"
#include "wrapper-utils.h"

class IceUdpMuxListenerWrapper : public Napi::ObjectWrap<IceUdpMuxListenerWrapper>
{
public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  explicit IceUdpMuxListenerWrapper(const Napi::CallbackInfo &info);
  ~IceUdpMuxListenerWrapper();

  void stop(const Napi::CallbackInfo &info);
  void onUnhandledStunRequest(const Napi::CallbackInfo &info);

  static void StopAll(AddonData *data);
  void doCleanup(bool fromDestructor = false);

  Napi::Value port(const Napi::CallbackInfo &info);
  Napi::Value address(const Napi::CallbackInfo &info);

private:
  AddonData *mAddonData = nullptr;
  std::shared_ptr<std::atomic<bool>> mAlive = std::make_shared<std::atomic<bool>>(true);
  SelfRef mSelfRef;
  std::shared_ptr<ThreadSafeCallback> mOnUnhandledStunRequestCallback = nullptr;
  Napi::FunctionReference mOnUnhandledStunRequestFn;
  uint64_t mOnUnhandledStunRequestGen = 0;
  std::optional<std::string> mAddress;
  uint16_t mPort = 0;
  std::unique_ptr<rtc::IceUdpMuxListener> iceUdpMuxListenerPtr = nullptr;
};

#endif // ICE_UDP_MUX_LISTENER_WRAPPER_H
