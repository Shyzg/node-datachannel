#ifndef ADDON_DATA_H
#define ADDON_DATA_H

#include <atomic>
#include <cstdint>
#include <memory>
#include <shared_mutex>
#include <unordered_set>

#include <napi.h>

class PeerConnectionWrapper;
class DataChannelWrapper;
class IceUdpMuxListenerWrapper;
class ThreadSafeCallback;

#if RTC_ENABLE_MEDIA == 1
class TrackWrapper;
#endif

#if RTC_ENABLE_WEBSOCKET == 1
class WebSocketWrapper;
class WebSocketServerWrapper;
#endif

// Process-wide count of live native wrapper instances across all Node environments/workers
inline std::atomic<int64_t> gActiveNativeInstances{0};
// Coordinates native instance creation (shared lock) with process-wide rtc::Cleanup() (unique lock)
inline std::shared_mutex gLifecycleMutex;

struct AddonData
{
  // Per-environment constructors for C++-instantiated JS wrappers (worker-thread safe)
  Napi::FunctionReference dataChannelCtor;

#if RTC_ENABLE_MEDIA == 1
  Napi::FunctionReference trackCtor;
#endif

#if RTC_ENABLE_WEBSOCKET == 1
  Napi::FunctionReference webSocketCtor;
#endif

  // Per-environment active instances (JS thread only)
  std::unordered_set<PeerConnectionWrapper *> peerConnections;
  std::unordered_set<DataChannelWrapper *> dataChannels;
  std::unordered_set<IceUdpMuxListenerWrapper *> iceUdpMuxListeners;

#if RTC_ENABLE_MEDIA == 1
  std::unordered_set<TrackWrapper *> tracks;
#endif

#if RTC_ENABLE_WEBSOCKET == 1
  std::unordered_set<WebSocketWrapper *> webSockets;
  std::unordered_set<WebSocketServerWrapper *> webSocketServers;
#endif

  std::shared_ptr<ThreadSafeCallback> logCallback;

  ~AddonData();
};

inline AddonData *getAddonData(Napi::Env env) { return env.GetInstanceData<AddonData>(); }

#endif // ADDON_DATA_H
