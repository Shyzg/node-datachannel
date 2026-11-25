#include "media-pacinghandler-wrapper.h"
#include "media-mediahandler-helper.h"
#include "wrapper-utils.h"

#include <chrono>
#include <cmath>

Napi::Object PacingHandlerWrapper::Init(Napi::Env env, Napi::Object exports)
{
  Napi::HandleScope scope(env);

  Napi::Function func = Napi::ObjectWrap<PacingHandlerWrapper>::DefineClass(
      env, "PacingHandler",
      {
          InstanceMethod("addToChain", &PacingHandlerWrapper::addToChain),
      });

  exports.Set("PacingHandler", func);
  return exports;
}

PacingHandlerWrapper::PacingHandlerWrapper(const Napi::CallbackInfo &info)
    : Napi::ObjectWrap<PacingHandlerWrapper>(info)
{
  Napi::Env env = info.Env();

  if (info.Length() < 2)
  {
    Napi::TypeError::New(env, "Expected 2 parameters").ThrowAsJavaScriptException();
    return;
  }
  if (!info[0].IsNumber())
  {
    Napi::TypeError::New(env, "Expected bitsPerSecond to be a number").ThrowAsJavaScriptException();
    return;
  }
  double bitsPerSecond = info[0].As<Napi::Number>().DoubleValue();
  if (!std::isfinite(bitsPerSecond) || bitsPerSecond <= 0)
  {
    Napi::RangeError::New(env, "bitsPerSecond must be a positive finite number").ThrowAsJavaScriptException();
    return;
  }
  int64_t sendInterval = 0;
  if (!parsePositiveInt64(env, info[1], "sendInterval", sendInterval))
    return;

  mHandlerPtr = std::make_shared<rtc::PacingHandler>(bitsPerSecond, std::chrono::milliseconds(sendInterval));
  info.This().As<Napi::Object>().TypeTag(&kPacingHandlerTypeTag);
}

PacingHandlerWrapper::~PacingHandlerWrapper() { mHandlerPtr.reset(); }

std::shared_ptr<rtc::PacingHandler> PacingHandlerWrapper::getHandlerInstance() { return mHandlerPtr; }

void PacingHandlerWrapper::addToChain(const Napi::CallbackInfo &info) { addMediaHandlerToChain(info, mHandlerPtr); }
