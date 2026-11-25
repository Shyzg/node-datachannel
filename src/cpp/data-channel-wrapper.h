#ifndef DATA_CHANNEL_WRAPPER_H
#define DATA_CHANNEL_WRAPPER_H

#include "channel-wrapper-base.h"

class DataChannelWrapper
    : public ChannelWrapperBase<DataChannelWrapper, rtc::DataChannel, &AddonData::dataChannels,
                                &AddonData::dataChannelCtor>
{
public:
  static constexpr const char *kChannelLabel = "channel";

  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  explicit DataChannelWrapper(const Napi::CallbackInfo &info);
  ~DataChannelWrapper();

  Napi::Value getLabel(const Napi::CallbackInfo &info);
  Napi::Value getId(const Napi::CallbackInfo &info);
  Napi::Value getProtocol(const Napi::CallbackInfo &info);
};

#endif // DATA_CHANNEL_WRAPPER_H
