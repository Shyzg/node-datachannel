#ifndef MEDIA_MEDIAHANDLER_HELPER_H
#define MEDIA_MEDIAHANDLER_HELPER_H

#include <memory>

#include <napi.h>
#include <rtc/rtc.hpp>

std::shared_ptr<rtc::MediaHandler> asMediaHandler(const Napi::Object &obj);
void addMediaHandlerToChain(const Napi::CallbackInfo &info, const std::shared_ptr<rtc::MediaHandler> &handler);
std::shared_ptr<rtc::RtpPacketizationConfig> extractRtpConfig(Napi::Env env, Napi::Value val,
                                                              Napi::ObjectReference &outRef);
Napi::Value returnRtpConfig(const Napi::CallbackInfo &info, const void *ptr, const Napi::ObjectReference &ref);
bool parseOptionalMaxFragmentSize(const Napi::CallbackInfo &info, size_t argIndex, size_t &outSize,
                                  size_t minSize = 4);

#endif // MEDIA_MEDIAHANDLER_HELPER_H
