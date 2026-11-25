#ifndef THREAD_SAFE_CALLBACK_H
#define THREAD_SAFE_CALLBACK_H

#include <napi.h>

#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

class ThreadSafeCallback
{
public:
  using arg_vector_t = std::vector<napi_value>;
  using arg_func_t = std::function<void(napi_env, arg_vector_t &)>;
  using dispatch_func_t = std::function<void(Napi::Env)>;
  using cleanup_func_t = std::function<void()>;

  ThreadSafeCallback(Napi::Function callback, bool unref = false);
  explicit ThreadSafeCallback(Napi::Env env, bool unref = true);
  ~ThreadSafeCallback();

  ThreadSafeCallback(const ThreadSafeCallback &) = delete;
  ThreadSafeCallback(ThreadSafeCallback &&) = delete;

  ThreadSafeCallback &operator=(const ThreadSafeCallback &) = delete;
  ThreadSafeCallback &operator=(ThreadSafeCallback &&) = delete;

  void call(
      arg_func_t argFunc, cleanup_func_t cleanupFunc = []() {});
  void dispatch(
      dispatch_func_t fn, cleanup_func_t cleanupFunc = []() {});
  void cancel();

  class CancelException : public std::exception
  {
    const char *what() const noexcept override;
  };

private:
  struct CallbackData
  {
    arg_func_t argFunc;
    dispatch_func_t dispatchFunc;
    cleanup_func_t cleanupFunc;
  };
  struct State
  {
    std::mutex mutex;
    bool aborted = false;
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
