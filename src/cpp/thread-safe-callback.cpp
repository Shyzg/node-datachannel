#include "thread-safe-callback.h"

#include <memory>

const char *ThreadSafeCallback::CancelException::what() const noexcept { return "ThreadSafeCallback cancelled"; }

void ThreadSafeCallback::finalizeFunc(Napi::Env, void *, ContextType *context)
{
  std::unique_ptr<ContextType> owned(context);
  if (!owned || !*owned)
    return;
  std::deque<CallbackData> toDrop;
  {
    std::lock_guard<std::mutex> lock((*owned)->mutex);
    (*owned)->aborted = true;
    toDrop.swap((*owned)->pending);
  }
}

ThreadSafeCallback::ThreadSafeCallback(Napi::Function callback, bool unref)
{
  Napi::Env env = callback.Env();

  if (!callback.IsFunction())
    throw Napi::Error::New(env, "Callback must be a function");

  auto *ctx = new ContextType(mState);
  try
  {
    tsfn = tsfn_t::New(env, std::move(callback), "ThreadSafeCallback callback",
                       0, // unlimited queue
                       1, ctx, finalizeFunc, static_cast<void *>(nullptr));
  }
  catch (...)
  {
    delete ctx;
    throw;
  }
  if (unref)
    tsfn.Unref(env);
}

ThreadSafeCallback::ThreadSafeCallback(Napi::Env env, bool unref)
{
  auto *ctx = new ContextType(mState);
  try
  {
    tsfn = tsfn_t::New(env, "ThreadSafeCallback internal",
                       0, // unlimited queue
                       1, ctx, finalizeFunc, static_cast<void *>(nullptr));
  }
  catch (...)
  {
    delete ctx;
    throw;
  }
  if (unref)
    tsfn.Unref(env);
}

ThreadSafeCallback::~ThreadSafeCallback() { cancel(); }

void ThreadSafeCallback::cancel()
{
  // Destroy queued payloads outside the mutex so captured native handles (e.g. rtc::WebSocket)
  // are released synchronously before rtc::Cleanup() waits on them, without risking lock re-entry.
  std::deque<CallbackData> toDrop;
  {
    std::lock_guard<std::mutex> lock(mState->mutex);
    toDrop.swap(mState->pending);
    if (!mState->aborted)
    {
      mState->aborted = true;
      tsfn.Abort();
    }
  }
}

void ThreadSafeCallback::call(arg_func_t argFunc, cleanup_func_t cleanupFunc)
{
  std::lock_guard<std::mutex> lock(mState->mutex);
  if (mState->aborted)
    return;

  mState->pending.push_back(CallbackData{std::move(argFunc), nullptr, std::move(cleanupFunc)});
  // NonBlockingCall never blocks with unlimited queue, keeping the lock deadlock-free
  if (tsfn.NonBlockingCall(static_cast<std::nullptr_t *>(nullptr)) != napi_ok)
  {
    mState->pending.pop_back();
  }
}

void ThreadSafeCallback::dispatch(dispatch_func_t fn, cleanup_func_t cleanupFunc)
{
  std::lock_guard<std::mutex> lock(mState->mutex);
  if (mState->aborted)
    return;

  mState->pending.push_back(CallbackData{nullptr, std::move(fn), std::move(cleanupFunc)});
  if (tsfn.NonBlockingCall(static_cast<std::nullptr_t *>(nullptr)) != napi_ok)
  {
    mState->pending.pop_back();
  }
}

void ThreadSafeCallback::callbackFunc(Napi::Env env, Napi::Function callback, ContextType *context, std::nullptr_t *)
{
  if (!context || !*context)
    return;

  CallbackData item;
  {
    std::lock_guard<std::mutex> lock((*context)->mutex);
    if ((*context)->pending.empty())
      return;
    item = std::move((*context)->pending.front());
    (*context)->pending.pop_front();
  }

  // if env is gone, it could mean this cb was destroyed. See issue#176
  if (!env)
    return;

  struct CleanupGuard
  {
    cleanup_func_t fn;
    ~CleanupGuard()
    {
      if (fn)
        fn();
    }
  } guard{std::move(item.cleanupFunc)};

  napi_value pendingError = nullptr;
  try
  {
    if (item.dispatchFunc)
    {
      item.dispatchFunc(env);
    }
    else
    {
      arg_vector_t args;
      if (item.argFunc)
        item.argFunc(env, args);
      if (callback)
        callback.Call(args);
    }
  }
  catch (CancelException &)
  {
    guard.fn = nullptr;
    return;
  }
  catch (const Napi::Error &err)
  {
    pendingError = err.Value();
  }

  if (guard.fn)
  {
    auto fn = std::move(guard.fn);
    guard.fn = nullptr;
    fn();
  }

  if (pendingError)
  {
    napi_fatal_exception(env, pendingError);
  }
}
