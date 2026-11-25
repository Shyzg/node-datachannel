#include "ice-udp-mux-listener-wrapper.h"

#include "plog/Log.h"

#include <iostream>

void IceUdpMuxListenerWrapper::StopAll(AddonData *data)
{
  if (!data)
    return;
  PLOG_DEBUG << "IceUdpMuxListenerWrapper StopAll() called";
  auto copy = data->iceUdpMuxListeners;
  for (auto *inst : copy)
    inst->doCleanup();
}

Napi::Object IceUdpMuxListenerWrapper::Init(Napi::Env env, Napi::Object exports)
{
  Napi::HandleScope scope(env);

  Napi::Function func =
      DefineClass(env, "IceUdpMuxListener",
                  {InstanceMethod("stop", &IceUdpMuxListenerWrapper::stop),
                   InstanceMethod("onUnhandledStunRequest", &IceUdpMuxListenerWrapper::onUnhandledStunRequest),
                   InstanceAccessor("port", &IceUdpMuxListenerWrapper::port, nullptr),
                   InstanceAccessor("address", &IceUdpMuxListenerWrapper::address, nullptr)});

  exports.Set("IceUdpMuxListener", func);
  return exports;
}

IceUdpMuxListenerWrapper::IceUdpMuxListenerWrapper(const Napi::CallbackInfo &info)
    : Napi::ObjectWrap<IceUdpMuxListenerWrapper>(info)
{
  Napi::Env env = info.Env();
  int length = info.Length();

  if (length > 0 && info[0].IsNumber())
  {
    if (!parseUint16Port(env, info[0], "port", mPort))
      return;
  }
  else
  {
    Napi::TypeError::New(env, "Port (Number) and optional Address (String) expected").ThrowAsJavaScriptException();
    return;
  }

  if (length > 1 && !info[1].IsUndefined())
  {
    if (!info[1].IsString())
    {
      Napi::TypeError::New(env, "Address (String) expected").ThrowAsJavaScriptException();
      return;
    }
    mAddress = info[1].As<Napi::String>().ToString();
  }

  iceUdpMuxListenerPtr = std::make_unique<rtc::IceUdpMuxListener>(mPort, mAddress);
  mAddonData = getAddonData(env);
  if (mAddonData && mAddonData->iceUdpMuxListeners.insert(this).second)
    ++gActiveNativeInstances;
}

IceUdpMuxListenerWrapper::~IceUdpMuxListenerWrapper() { doCleanup(/*fromDestructor=*/true); }

void IceUdpMuxListenerWrapper::doCleanup(bool fromDestructor)
{
  mAlive->store(false);
  resetCallback(mOnUnhandledStunRequestCallback);

  if (iceUdpMuxListenerPtr)
  {
    try
    {
      iceUdpMuxListenerPtr->stop();
    }
    catch (std::exception &ex)
    {
      std::cerr << std::string("libdatachannel error while stopping IceUdpMuxListener: ") + ex.what() << std::endl;
    }
    iceUdpMuxListenerPtr.reset();
  }

  if (mAddonData)
  {
    if (mAddonData->iceUdpMuxListeners.erase(this) > 0)
      --gActiveNativeInstances;
    mAddonData = nullptr;
  }

  mSelfRef.release(this, fromDestructor);
}

Napi::Value IceUdpMuxListenerWrapper::port(const Napi::CallbackInfo &info)
{
  return Napi::Number::New(info.Env(), mPort);
}

Napi::Value IceUdpMuxListenerWrapper::address(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();
  return mAddress.has_value() ? Napi::Value(Napi::String::New(env, mAddress.value())) : env.Undefined();
}

void IceUdpMuxListenerWrapper::stop(const Napi::CallbackInfo &) { doCleanup(); }

void IceUdpMuxListenerWrapper::onUnhandledStunRequest(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();
  if (!iceUdpMuxListenerPtr)
  {
    Napi::Error::New(env, "IceUdpMuxListenerWrapper::onUnhandledStunRequest() called on destroyed IceUdpMuxListener")
        .ThrowAsJavaScriptException();
    return;
  }

  if (info.Length() < 1 || !info[0].IsFunction())
  {
    Napi::TypeError::New(env, "Function expected").ThrowAsJavaScriptException();
    return;
  }

  mSelfRef.acquire(this);
  auto cb = std::make_shared<ThreadSafeCallback>(info[0].As<Napi::Function>());
  resetCallback(mOnUnhandledStunRequestCallback);
  mOnUnhandledStunRequestCallback = cb;

  iceUdpMuxListenerPtr->OnUnhandledStunRequest(
      [alive = mAlive, cb = std::move(cb)](rtc::IceUdpMuxRequest request)
      {
        cb->call(
            [alive, request = std::move(request)](Napi::Env env, std::vector<napi_value> &args)
            {
              if (!alive->load())
                throw ThreadSafeCallback::CancelException();

              Napi::Object reqObj = Napi::Object::New(env);
              reqObj.Set("ufrag", request.remoteUfrag.c_str());
              reqObj.Set("localUfrag", request.localUfrag.c_str());
              reqObj.Set("host", request.remoteAddress.c_str());
              reqObj.Set("port", request.remotePort);
              args = {reqObj};
            });
      });
}
