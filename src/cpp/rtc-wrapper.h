#ifndef RTC_WRAPPER_H
#define RTC_WRAPPER_H

#include <napi.h>

#include "rtc/rtc.hpp"

#include <mutex>

#include "addon-data.h"
#include "thread-safe-callback.h"

class RtcWrapper
{
public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  static void preload(const Napi::CallbackInfo &info);
  static void initLogger(const Napi::CallbackInfo &info);
  static void cleanup(const Napi::CallbackInfo &info);
  static void setSctpSettings(const Napi::CallbackInfo &info);
  static Napi::Value getLibraryVersion(const Napi::CallbackInfo &info);
  static void clearLogCallbackIfActive(AddonData *data, bool fromDestructor = false);

private:
  static inline std::mutex logMutex;
  static inline std::weak_ptr<ThreadSafeCallback> activeLogCallback;
};

#endif // RTC_WRAPPER_H
