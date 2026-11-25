#ifndef THREAD_SAFE_CALLBACK_H
#define THREAD_SAFE_CALLBACK_H

#include <napi.h>
#include <rtc/rtc.hpp>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <variant>

class ThreadSafeCallback
{
public:
  using dispatch_func_t = std::function<void(Napi::Env)>;
  using cleanup_func_t = std::function<void()>;
  using message_variant_t = std::variant<rtc::binary, std::string>;
  using message_handler_t = std::function<void(Napi::Env, uint64_t, message_variant_t &&)>;

  explicit ThreadSafeCallback(Napi::Env env, bool unref = true);
  ~ThreadSafeCallback();

  ThreadSafeCallback(const ThreadSafeCallback &) = delete;
  ThreadSafeCallback(ThreadSafeCallback &&) = delete;

  ThreadSafeCallback &operator=(const ThreadSafeCallback &) = delete;
  ThreadSafeCallback &operator=(ThreadSafeCallback &&) = delete;

  void setMessageHandler(message_handler_t handler);

  void dispatch(
      dispatch_func_t fn, cleanup_func_t cleanupFunc = []() {}, bool isClose = false);
  void dispatchMessage(uint64_t gen, message_variant_t message, bool insertBeforeClose = false);
  void cancel();

  class CancelException : public std::exception
  {
    const char *what() const noexcept override;
  };

private:
  struct MessageItem
  {
    uint64_t gen = 0;
    message_variant_t message;
  };
  struct CallbackData
  {
    dispatch_func_t dispatchFunc;
    cleanup_func_t cleanupFunc;
    std::optional<MessageItem> messageItem;
    bool isClose = false;
  };
  struct State
  {
    std::mutex mutex;
    bool aborted = false;
    message_handler_t messageHandler;
    std::deque<CallbackData> pending;
  };
  using ContextType = std::shared_ptr<State>;

  static void callbackFunc(Napi::Env env, Napi::Function callback, ContextType *context, std::nullptr_t *data);
  static void finalizeFunc(Napi::Env env, void *finalizeData, ContextType *context);

  using tsfn_t = Napi::TypedThreadSafeFunction<ContextType, std::nullptr_t, callbackFunc>;
  tsfn_t tsfn;
  std::shared_ptr<State> mState = std::make_shared<State>();
};

inline void resetCallback(std::shared_ptr<ThreadSafeCallback> &cb)
{
  if (cb)
  {
    cb->cancel();
    cb.reset();
  }
}

#endif // THREAD_SAFE_CALLBACK_H
