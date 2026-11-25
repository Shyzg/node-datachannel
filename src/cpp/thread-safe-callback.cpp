#include "thread-safe-callback.h"

#include <memory>

const char *ThreadSafeCallback::CancelException::what() const noexcept { return "ThreadSafeCallback cancelled"; }

void ThreadSafeCallback::finalizeFunc(Napi::Env, void *, ContextType *context)
{
  std::unique_ptr<ContextType> owned(context);
  if (!owned || !*owned)
    return;
  std::deque<CallbackData> toDrop;
  message_handler_t handlerToDrop;
  {
    std::lock_guard<std::mutex> lock((*owned)->mutex);
    (*owned)->aborted = true;
    toDrop.swap((*owned)->pending);
    handlerToDrop.swap((*owned)->messageHandler);
  }
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
  message_handler_t handlerToDrop;
  {
    std::lock_guard<std::mutex> lock(mState->mutex);
    toDrop.swap(mState->pending);
    handlerToDrop.swap(mState->messageHandler);
    if (!mState->aborted)
    {
      mState->aborted = true;
      tsfn.Abort();
    }
  }
}

void ThreadSafeCallback::setMessageHandler(message_handler_t handler)
{
  std::lock_guard<std::mutex> lock(mState->mutex);
  mState->messageHandler = std::move(handler);
}

void ThreadSafeCallback::dispatch(dispatch_func_t fn, cleanup_func_t cleanupFunc, bool isClose)
{
  std::lock_guard<std::mutex> lock(mState->mutex);
  if (mState->aborted)
    return;

  mState->pending.push_back(CallbackData{std::move(fn), std::move(cleanupFunc), std::nullopt, isClose});
  // NonBlockingCall never blocks with unlimited queue, keeping the lock deadlock-free
  if (tsfn.NonBlockingCall(static_cast<std::nullptr_t *>(nullptr)) != napi_ok)
  {
    mState->pending.pop_back();
  }
}

void ThreadSafeCallback::dispatchMessage(uint64_t gen, message_variant_t message, bool insertBeforeClose)
{
  std::lock_guard<std::mutex> lock(mState->mutex);
  if (mState->aborted)
    return;

  CallbackData item{nullptr, nullptr, MessageItem{gen, std::move(message)}, false};
  auto insertPos = mState->pending.end();
  if (insertBeforeClose)
  {
    for (auto it = mState->pending.begin(); it != mState->pending.end(); ++it)
    {
      if (it->isClose)
      {
        insertPos = it;
        break;
      }
    }
  }
  auto inserted = mState->pending.insert(insertPos, std::move(item));
  if (tsfn.NonBlockingCall(static_cast<std::nullptr_t *>(nullptr)) != napi_ok)
  {
    mState->pending.erase(inserted);
  }
}

void ThreadSafeCallback::callbackFunc(Napi::Env env, Napi::Function, ContextType *context, std::nullptr_t *)
{
  if (!context || !*context)
    return;

  CallbackData item;
  message_handler_t msgHandler;
  {
    std::lock_guard<std::mutex> lock((*context)->mutex);
    if ((*context)->pending.empty())
      return;
    item = std::move((*context)->pending.front());
    (*context)->pending.pop_front();
    if (item.messageItem.has_value())
      msgHandler = (*context)->messageHandler;
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
    if (item.messageItem.has_value())
    {
      if (msgHandler)
        msgHandler(env, item.messageItem->gen, std::move(item.messageItem->message));
    }
    else if (item.dispatchFunc)
    {
      item.dispatchFunc(env);
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
