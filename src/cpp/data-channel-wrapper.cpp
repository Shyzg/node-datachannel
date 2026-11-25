#include "data-channel-wrapper.h"

Napi::Object DataChannelWrapper::Init(Napi::Env env, Napi::Object exports)
{
  Napi::HandleScope scope(env);

  std::vector<PropertyDescriptor> props = {
      InstanceMethod("getLabel", &DataChannelWrapper::getLabel),
      InstanceMethod("getId", &DataChannelWrapper::getId),
      InstanceMethod("getProtocol", &DataChannelWrapper::getProtocol),
  };
  appendChannelDescriptors(props);

  Napi::Function func = DefineClass(env, "DataChannel", props);
  storeConstructor(env, func);
  exports.Set("DataChannel", func);
  return exports;
}

DataChannelWrapper::DataChannelWrapper(const Napi::CallbackInfo &info) : ChannelWrapperBase(info)
{
  PLOG_DEBUG << "Constructor called";
  if (info.Length() < 1 || !info[0].IsExternal())
  {
    Napi::TypeError::New(info.Env(), "DataChannel cannot be constructed directly").ThrowAsJavaScriptException();
    return;
  }

  auto *ptr = info[0].As<Napi::External<std::shared_ptr<rtc::DataChannel>>>().Data();
  if (!ptr || !*ptr)
  {
    Napi::TypeError::New(info.Env(), "Invalid DataChannel handle").ThrowAsJavaScriptException();
    return;
  }

  mChannelPtr = *ptr;
  PLOG_DEBUG << "Data Channel created";
  registerInstance(info.Env());
}

DataChannelWrapper::~DataChannelWrapper()
{
  PLOG_DEBUG << "Destructor called";
  destroyInDestructor();
}

Napi::Value DataChannelWrapper::getLabel(const Napi::CallbackInfo &info)
{
  if (!mChannelPtr)
  {
    Napi::Error::New(info.Env(), "getLabel() called on destroyed channel").ThrowAsJavaScriptException();
    return info.Env().Null();
  }
  return Napi::String::New(info.Env(), mChannelPtr->label());
}

Napi::Value DataChannelWrapper::getId(const Napi::CallbackInfo &info)
{
  if (!mChannelPtr)
  {
    Napi::Error::New(info.Env(), "getId() called on destroyed channel").ThrowAsJavaScriptException();
    return info.Env().Null();
  }
  auto id = mChannelPtr->id();
  return Napi::Number::New(info.Env(), id.has_value() ? static_cast<int32_t>(*id) : -1);
}

Napi::Value DataChannelWrapper::getProtocol(const Napi::CallbackInfo &info)
{
  if (!mChannelPtr)
  {
    Napi::Error::New(info.Env(), "getProtocol() called on destroyed channel").ThrowAsJavaScriptException();
    return info.Env().Null();
  }
  return Napi::String::New(info.Env(), mChannelPtr->protocol());
}
