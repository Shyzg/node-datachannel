#ifndef CHANNEL_WRAPPER_BASE_H
#define CHANNEL_WRAPPER_BASE_H

#include <atomic>
#include <cstddef>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include <napi.h>
#include <rtc/rtc.hpp>

#include "addon-data.h"
#include "plog/Log.h"
#include "thread-safe-callback.h"
#include "wrapper-utils.h"

template <typename Derived, typename RtcChannel, std::unordered_set<Derived *> AddonData::*InstancesMember,
          Napi::FunctionReference AddonData::*CtorMember, bool BinaryOnlyMessages = false, bool GracefulClose = false>
class ChannelWrapperBase : public Napi::ObjectWrap<Derived>
{
public:
  static void CloseAll(AddonData *data)
  {
    if (!data)
      return;
    auto copy = data->*InstancesMember;
    for (auto *inst : copy)
      inst->doClose(/*fromDestructor=*/false, /*forceReset=*/true);
  }

  static void CleanupAll(AddonData *data)
  {
    if (!data)
      return;
    auto copy = data->*InstancesMember;
    for (auto *inst : copy)
      inst->doCleanup();
  }

  explicit ChannelWrapperBase(const Napi::CallbackInfo &info) : Napi::ObjectWrap<Derived>(info) {}

  void close(const Napi::CallbackInfo &) { doClose(/*fromDestructor=*/false, /*forceReset=*/!GracefulClose); }

  Napi::Value sendMessage(const Napi::CallbackInfo &info)
  {
    Napi::Env env = info.Env();
    if (!mChannelPtr)
    {
      Napi::Error::New(env, std::string("sendMessage() called on destroyed ") + Derived::kChannelLabel)
          .ThrowAsJavaScriptException();
      return env.Null();
    }

    if (info.Length() < 1 || !info[0].IsString())
    {
      Napi::TypeError::New(env, "String expected").ThrowAsJavaScriptException();
      return env.Null();
    }

    try
    {
      return Napi::Boolean::New(env, mChannelPtr->send(info[0].As<Napi::String>().ToString()));
    }
    catch (std::exception &ex)
    {
      Napi::Error::New(env, std::string("libdatachannel error while sending ") + Derived::kChannelLabel +
                                " message: " + ex.what())
          .ThrowAsJavaScriptException();
      return Napi::Boolean::New(env, false);
    }
  }

  Napi::Value sendMessageBinary(const Napi::CallbackInfo &info)
  {
    Napi::Env env = info.Env();
    if (!mChannelPtr)
    {
      Napi::Error::New(env, std::string("sendMessageBinary() called on destroyed ") + Derived::kChannelLabel)
          .ThrowAsJavaScriptException();
      return env.Null();
    }

    if (info.Length() < 1 || !info[0].IsBuffer())
    {
      Napi::TypeError::New(env, "Buffer expected").ThrowAsJavaScriptException();
      return env.Null();
    }

    void *bufData = nullptr;
    size_t byteLength = 0;
    if (napi_get_buffer_info(env, info[0], &bufData, &byteLength) != napi_ok)
    {
      Napi::TypeError::New(env, "Buffer expected").ThrowAsJavaScriptException();
      return env.Null();
    }

    try
    {
      return Napi::Boolean::New(env, mChannelPtr->send(static_cast<const std::byte *>(bufData), byteLength));
    }
    catch (std::exception &ex)
    {
      Napi::Error::New(env, std::string("libdatachannel error while sending ") + Derived::kChannelLabel +
                                " message: " + ex.what())
          .ThrowAsJavaScriptException();
      return Napi::Boolean::New(env, false);
    }
  }

  Napi::Value isOpen(const Napi::CallbackInfo &info)
  {
    Napi::Env env = info.Env();
    if (!mChannelPtr)
      return Napi::Boolean::New(env, false);

    try
    {
      return Napi::Boolean::New(env, mChannelPtr->isOpen());
    }
    catch (std::exception &ex)
    {
      Napi::Error::New(env, std::string("libdatachannel error: ") + ex.what()).ThrowAsJavaScriptException();
      return Napi::Boolean::New(env, false);
    }
  }

  Napi::Value bufferedAmount(const Napi::CallbackInfo &info)
  {
    Napi::Env env = info.Env();
    if (!mChannelPtr)
      return Napi::Number::New(env, 0);

    try
    {
      return Napi::Number::New(env, mChannelPtr->bufferedAmount());
    }
    catch (std::exception &ex)
    {
      Napi::Error::New(env, std::string("libdatachannel error: ") + ex.what()).ThrowAsJavaScriptException();
      return Napi::Number::New(env, 0);
    }
  }

  Napi::Value maxMessageSize(const Napi::CallbackInfo &info)
  {
    Napi::Env env = info.Env();
    if (!mChannelPtr)
      return Napi::Number::New(env, 0);

    try
    {
      return Napi::Number::New(env, mChannelPtr->maxMessageSize());
    }
    catch (std::exception &ex)
    {
      Napi::Error::New(env, std::string("libdatachannel error: ") + ex.what()).ThrowAsJavaScriptException();
      return Napi::Number::New(env, 0);
    }
  }

  void setBufferedAmountLowThreshold(const Napi::CallbackInfo &info)
  {
    Napi::Env env = info.Env();
    if (!mChannelPtr)
    {
      Napi::Error::New(env, std::string("setBufferedAmountLowThreshold() called on destroyed ") +
                                Derived::kChannelLabel)
          .ThrowAsJavaScriptException();
      return;
    }

    if (info.Length() < 1)
    {
      Napi::TypeError::New(env, "Number expected").ThrowAsJavaScriptException();
      return;
    }

    size_t threshold = 0;
    if (!parseNonNegativeSize(env, info[0], "Threshold", threshold))
      return;

    try
    {
      mChannelPtr->setBufferedAmountLowThreshold(threshold);
    }
    catch (std::exception &ex)
    {
      Napi::Error::New(env, std::string("libdatachannel error: ") + ex.what()).ThrowAsJavaScriptException();
    }
  }

  void onOpen(const Napi::CallbackInfo &info)
  {
    bindCallback(info, "onOpen", mOnOpenFn, mOnOpenGen,
                 [this](auto state, auto queue, uint64_t gen)
                 {
                   mChannelPtr->onOpen(
                       [this, state, queue, gen]()
                       {
                         queue->dispatch(
                             [this, state, gen](Napi::Env)
                             {
                               if (!state->alive.load() || mOnOpenGen != gen || mOnOpenFn.IsEmpty())
                                 throw ThreadSafeCallback::CancelException();
                               mOnOpenFn.Call({});
                             });
                       });
                 });
  }

  void onClosed(const Napi::CallbackInfo &info)
  {
    Napi::Env env = info.Env();
    if (!mChannelPtr)
    {
      Napi::Error::New(env, std::string("onClosed() called on destroyed ") + Derived::kChannelLabel)
          .ThrowAsJavaScriptException();
      return;
    }

    if (info.Length() < 1 || !info[0].IsFunction())
    {
      Napi::TypeError::New(env, "Function expected").ThrowAsJavaScriptException();
      return;
    }

    ensureEventQueue(env);
    mOnClosedFn = Napi::Persistent(info[0].As<Napi::Function>());
    checkAlreadyClosed();
  }

  void onError(const Napi::CallbackInfo &info)
  {
    bindCallback(info, "onError", mOnErrorFn, mOnErrorGen,
                 [this](auto state, auto queue, uint64_t gen)
                 {
                   mChannelPtr->onError(
                       [this, state, queue, gen](std::string error)
                       {
                         queue->dispatch(
                             [this, state, gen, error = std::move(error)](Napi::Env env)
                             {
                               if (!state->alive.load() || mOnErrorGen != gen || mOnErrorFn.IsEmpty())
                                 throw ThreadSafeCallback::CancelException();
                               mOnErrorFn.Call({Napi::String::New(env, error)});
                             });
                       });
                 });
  }

  void onBufferedAmountLow(const Napi::CallbackInfo &info)
  {
    bindCallback(info, "onBufferedAmountLow", mOnBufferedAmountLowFn, mOnBufferedAmountLowGen,
                 [this](auto state, auto queue, uint64_t gen)
                 {
                   mChannelPtr->onBufferedAmountLow(
                       [this, state, queue, gen]()
                       {
                         queue->dispatch(
                             [this, state, gen](Napi::Env)
                             {
                               if (!state->alive.load() || mOnBufferedAmountLowGen != gen ||
                                   mOnBufferedAmountLowFn.IsEmpty())
                                 throw ThreadSafeCallback::CancelException();
                               mOnBufferedAmountLowFn.Call({});
                             });
                       });
                 });
  }

  void onMessage(const Napi::CallbackInfo &info)
  {
    // checkAlreadyClosed runs inside bindCallback AFTER mChannelPtr->onMessage() so any pending
    // messages flushed by libdatachannel are queued in FIFO order BEFORE the close notification.
    bindCallback(
        info, "onMessage", mOnMessageFn, mOnMessageGen,
        [this](auto state, auto queue, uint64_t gen)
        {
          mChannelPtr->onMessage(
              [this, state, queue, gen](std::variant<rtc::binary, std::string> message)
              {
                queue->dispatch(
                    [this, state, gen, message = std::move(message)](Napi::Env env)
                    {
                      if (!state->alive.load() || mOnMessageGen != gen || mOnMessageFn.IsEmpty())
                        throw ThreadSafeCallback::CancelException();

                      if constexpr (BinaryOnlyMessages)
                      {
                        if (!std::holds_alternative<rtc::binary>(message))
                          throw ThreadSafeCallback::CancelException();
                        auto bin = std::get<rtc::binary>(std::move(message));
                        mOnMessageFn.Call({Napi::Buffer<std::byte>::Copy(env, bin.data(), bin.size())});
                      }
                      else
                      {
                        if (std::holds_alternative<std::string>(message))
                        {
                          mOnMessageFn.Call({Napi::String::New(env, std::get<std::string>(message))});
                        }
                        else
                        {
                          auto bin = std::get<rtc::binary>(std::move(message));
                          mOnMessageFn.Call({Napi::Buffer<std::byte>::Copy(env, bin.data(), bin.size())});
                        }
                      }
                    });
              });
        });
  }

  void doClose(bool fromDestructor = false, bool forceReset = true)
  {
    doCloseWith([](RtcChannel &ch) { ch.close(); }, "closing", fromDestructor, forceReset);
  }

  void doCleanup(bool fromDestructor = false)
  {
    mCloseState->alive.store(false);
    resetCallback(mEventQueue);
    resetFuncRef(mOnOpenFn, fromDestructor);
    resetFuncRef(mOnClosedFn, fromDestructor);
    resetFuncRef(mOnErrorFn, fromDestructor);
    resetFuncRef(mOnBufferedAmountLowFn, fromDestructor);
    resetFuncRef(mOnMessageFn, fromDestructor);
    mChannelPtr.reset();

    if (mAddonData)
    {
      if ((mAddonData->*InstancesMember).erase(static_cast<Derived *>(this)) > 0)
        --gActiveNativeInstances;
      mAddonData = nullptr;
    }

    mSelfRef.release(this, fromDestructor);
  }

protected:
  using PropertyDescriptor = typename Napi::ObjectWrap<Derived>::PropertyDescriptor;

  struct CloseState
  {
    std::atomic<bool> alive{true};
    std::atomic<bool> closeDispatched{false};
  };

  void registerInstance(Napi::Env env)
  {
    mAddonData = getAddonData(env);
    if (mAddonData && (mAddonData->*InstancesMember).insert(static_cast<Derived *>(this)).second)
      ++gActiveNativeInstances;
  }

  void destroyInDestructor()
  {
    if (mChannelPtr)
      mChannelPtr->resetCallbacks();
    doClose(/*fromDestructor=*/true, /*forceReset=*/true);
    doCleanup(/*fromDestructor=*/true);
  }

  bool isPermanentlyClosed() const
  {
    return mChannelPtr && mCanDetectPrematureClose && mChannelPtr->isClosed();
  }

  static void scheduleCloseOnce(ChannelWrapperBase *self, const std::shared_ptr<CloseState> &state,
                                const std::shared_ptr<ThreadSafeCallback> &queue)
  {
    if (!queue || state->closeDispatched.exchange(true))
      return;

    queue->dispatch(
        [self, state](Napi::Env)
        {
          if (!state->alive.load())
            throw ThreadSafeCallback::CancelException();
          if (!self->mOnClosedFn.IsEmpty())
            self->mOnClosedFn.Call({});
        },
        [self, state]()
        {
          if (state->alive.load())
            self->doCleanup();
        });
  }

  void checkAlreadyClosed()
  {
    if (isPermanentlyClosed())
      scheduleCloseOnce(this, mCloseState, mEventQueue);
  }

  template <typename CloseFn>
  void doCloseWith(CloseFn &&closeFn, const char *action, bool fromDestructor = false, bool forceReset = true)
  {
    if (fromDestructor)
    {
      if (mChannelPtr)
      {
        try
        {
          closeFn(*mChannelPtr);
        }
        catch (std::exception &ex)
        {
          std::cerr << std::string("libdatachannel error while ") + action + " " + Derived::kChannelLabel + ": " +
                           ex.what()
                    << std::endl;
        }
        mChannelPtr.reset();
      }
      doCleanup(/*fromDestructor=*/true);
      return;
    }

    // Invalidate non-close callbacks so no further open/message/error events run after explicit close()
    ++mOnOpenGen;
    ++mOnErrorGen;
    ++mOnBufferedAmountLowGen;
    ++mOnMessageGen;
    resetFuncRef(mOnOpenFn, false);
    resetFuncRef(mOnErrorFn, false);
    resetFuncRef(mOnBufferedAmountLowFn, false);
    resetFuncRef(mOnMessageFn, false);

    bool hasUserCloseCb = !mOnClosedFn.IsEmpty();
    bool willEmitClose = hasUserCloseCb && mCloseState->closeDispatched.load();

    if (mChannelPtr)
    {
      bool wasClosed = mChannelPtr->isClosed();
      bool closedOk = false;
      try
      {
        closeFn(*mChannelPtr);
        closedOk = true;
      }
      catch (std::exception &ex)
      {
        std::cerr << std::string("libdatachannel error while ") + action + " " + Derived::kChannelLabel + ": " +
                         ex.what()
                  << std::endl;
      }

      if (forceReset || wasClosed || !closedOk || mChannelPtr->isClosed())
      {
        mChannelPtr.reset();
        willEmitClose =
            (closedOk && !wasClosed && hasUserCloseCb) || (hasUserCloseCb && mCloseState->closeDispatched.load());
      }
      else
      {
        // Keep mChannelPtr alive while a graceful close handshake (e.g. WebSocket CLOSE frame) completes;
        // onClosed will trigger doCleanup() once closed (or on libdatachannel's close timeout).
        willEmitClose = true;
      }
    }

    if (!willEmitClose)
      doCleanup();
  }

  std::shared_ptr<ThreadSafeCallback> ensureEventQueue(Napi::Env env)
  {
    mSelfRef.acquire(this);
    if (!mEventQueue)
    {
      mEventQueue = std::make_shared<ThreadSafeCallback>(env, /*unref=*/false);
    }

    if (!mCloseHookInstalled && mChannelPtr)
    {
      mCloseHookInstalled = true;
      auto queue = mEventQueue;
      mChannelPtr->onClosed(
          [self = this, state = mCloseState, queue]()
          {
            if (!state->alive.load())
              return;
            scheduleCloseOnce(self, state, queue);
          });
    }

    return mEventQueue;
  }

  static void storeConstructor(Napi::Env env, Napi::Function func)
  {
    auto *data = getAddonData(env);
    if ((data->*CtorMember).IsEmpty())
      data->*CtorMember = Napi::Persistent(func);
  }

  static void appendChannelDescriptors(std::vector<PropertyDescriptor> &props, bool includeBufferedAmount = true)
  {
    using Base =
        ChannelWrapperBase<Derived, RtcChannel, InstancesMember, CtorMember, BinaryOnlyMessages, GracefulClose>;
    props.push_back(Derived::InstanceMethod("close", &Base::close));
    props.push_back(Derived::InstanceMethod("sendMessage", &Base::sendMessage));
    props.push_back(Derived::InstanceMethod("sendMessageBinary", &Base::sendMessageBinary));
    props.push_back(Derived::InstanceMethod("isOpen", &Base::isOpen));
    props.push_back(Derived::InstanceMethod("maxMessageSize", &Base::maxMessageSize));
    props.push_back(Derived::InstanceMethod("onOpen", &Base::onOpen));
    props.push_back(Derived::InstanceMethod("onClosed", &Base::onClosed));
    props.push_back(Derived::InstanceMethod("onError", &Base::onError));
    props.push_back(Derived::InstanceMethod("onMessage", &Base::onMessage));
    if (includeBufferedAmount)
    {
      props.push_back(Derived::InstanceMethod("bufferedAmount", &Base::bufferedAmount));
      props.push_back(Derived::InstanceMethod("setBufferedAmountLowThreshold", &Base::setBufferedAmountLowThreshold));
      props.push_back(Derived::InstanceMethod("onBufferedAmountLow", &Base::onBufferedAmountLow));
    }
  }

  AddonData *mAddonData = nullptr;
  std::shared_ptr<RtcChannel> mChannelPtr = nullptr;
  std::shared_ptr<CloseState> mCloseState = std::make_shared<CloseState>();
  std::shared_ptr<ThreadSafeCallback> mEventQueue = nullptr;
  SelfRef mSelfRef;
  bool mCloseHookInstalled = false;
  bool mCanDetectPrematureClose = true;

  Napi::FunctionReference mOnOpenFn;
  Napi::FunctionReference mOnClosedFn;
  Napi::FunctionReference mOnErrorFn;
  Napi::FunctionReference mOnBufferedAmountLowFn;
  Napi::FunctionReference mOnMessageFn;

  uint64_t mOnOpenGen = 0;
  uint64_t mOnErrorGen = 0;
  uint64_t mOnBufferedAmountLowGen = 0;
  uint64_t mOnMessageGen = 0;

private:
  template <typename BindFn>
  void bindCallback(const Napi::CallbackInfo &info, const char *methodName, Napi::FunctionReference &fnRef,
                    uint64_t &genRef, BindFn &&bindFn)
  {
    Napi::Env env = info.Env();
    if (!mChannelPtr)
    {
      Napi::Error::New(env, std::string(methodName) + "() called on destroyed " + Derived::kChannelLabel)
          .ThrowAsJavaScriptException();
      return;
    }

    if (info.Length() < 1 || !info[0].IsFunction())
    {
      Napi::TypeError::New(env, "Function expected").ThrowAsJavaScriptException();
      return;
    }

    auto queue = ensureEventQueue(env);
    fnRef = Napi::Persistent(info[0].As<Napi::Function>());
    uint64_t gen = ++genRef;
    bindFn(mCloseState, std::move(queue), gen);
    checkAlreadyClosed();
  }
};

#endif // CHANNEL_WRAPPER_BASE_H
