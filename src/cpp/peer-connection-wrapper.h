#ifndef PEER_CONNECTION_WRAPPER_H
#define PEER_CONNECTION_WRAPPER_H

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

#include <napi.h>
#include <rtc/rtc.hpp>

#include "addon-data.h"
#include "thread-safe-callback.h"
#include "wrapper-utils.h"

class PeerConnectionWrapper : public Napi::ObjectWrap<PeerConnectionWrapper>
{
public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  explicit PeerConnectionWrapper(const Napi::CallbackInfo &info);
  ~PeerConnectionWrapper();

  // Functions
  void close(const Napi::CallbackInfo &info);
  void setLocalDescription(const Napi::CallbackInfo &info);
  void setRemoteDescription(const Napi::CallbackInfo &info);
  Napi::Value localDescription(const Napi::CallbackInfo &info);
  Napi::Value remoteDescription(const Napi::CallbackInfo &info);
  void addRemoteCandidate(const Napi::CallbackInfo &info);
  Napi::Value createDataChannel(const Napi::CallbackInfo &info);

#if RTC_ENABLE_MEDIA == 1
  Napi::Value addTrack(const Napi::CallbackInfo &info);
  void onTrack(const Napi::CallbackInfo &info);
#endif

  Napi::Value hasMedia(const Napi::CallbackInfo &info);
  Napi::Value state(const Napi::CallbackInfo &info);
  Napi::Value iceState(const Napi::CallbackInfo &info);
  Napi::Value signalingState(const Napi::CallbackInfo &info);
  Napi::Value gatheringState(const Napi::CallbackInfo &info);
  Napi::Value remoteFingerprint(const Napi::CallbackInfo &info);

  // Callbacks
  void onLocalDescription(const Napi::CallbackInfo &info);
  void onLocalCandidate(const Napi::CallbackInfo &info);
  void onStateChange(const Napi::CallbackInfo &info);
  void onIceStateChange(const Napi::CallbackInfo &info);
  void onSignalingStateChange(const Napi::CallbackInfo &info);
  void onGatheringStateChange(const Napi::CallbackInfo &info);
  void onDataChannel(const Napi::CallbackInfo &info);

  // Stats
  Napi::Value bytesSent(const Napi::CallbackInfo &info);
  Napi::Value bytesReceived(const Napi::CallbackInfo &info);
  Napi::Value rtt(const Napi::CallbackInfo &info);
  Napi::Value getSelectedCandidatePair(const Napi::CallbackInfo &info);
  Napi::Value maxDataChannelId(const Napi::CallbackInfo &info);
  Napi::Value maxMessageSize(const Napi::CallbackInfo &info);

  static void CloseAll(AddonData *data);
  static void CleanupAll(AddonData *data);

  void doClose(bool fromDestructor = false);
  void doCleanup(bool fromDestructor = false);

private:
  static void scheduleCloseOnce(PeerConnectionWrapper *self, const std::shared_ptr<CloseState> &state,
                                const std::shared_ptr<ThreadSafeCallback> &queue);
  std::shared_ptr<ThreadSafeCallback> ensureEventQueue(Napi::Env env);
  void ensureCloseHook();
  template <typename BindFn>
  void bindCallback(const Napi::CallbackInfo &info, const char *methodName, Napi::FunctionReference &fnRef,
                    uint64_t &genRef, BindFn &&bindFn);

  AddonData *mAddonData = nullptr;
  std::unique_ptr<rtc::PeerConnection> mRtcPeerConnPtr = nullptr;
  EventQueueState mEvents;

  // JS Callback References (JS thread only)
  Napi::FunctionReference mOnLocalDescriptionFn;
  Napi::FunctionReference mOnLocalCandidateFn;
  Napi::FunctionReference mOnStateChangeFn;
  Napi::FunctionReference mOnIceStateChangeFn;
  Napi::FunctionReference mOnSignalingStateChangeFn;
  Napi::FunctionReference mOnGatheringStateChangeFn;
  Napi::FunctionReference mOnDataChannelFn;
#if RTC_ENABLE_MEDIA == 1
  Napi::FunctionReference mOnTrackFn;
  uint64_t mOnTrackGen = 0;
#endif

  uint64_t mOnLocalDescriptionGen = 0;
  uint64_t mOnLocalCandidateGen = 0;
  uint64_t mOnIceStateChangeGen = 0;
  uint64_t mOnSignalingStateChangeGen = 0;
  uint64_t mOnGatheringStateChangeGen = 0;
  uint64_t mOnDataChannelGen = 0;
};

#endif // PEER_CONNECTION_WRAPPER_H
