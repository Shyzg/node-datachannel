#include <napi.h>

#include "addon-data.h"
#include "data-channel-wrapper.h"
#include "ice-udp-mux-listener-wrapper.h"
#include "peer-connection-wrapper.h"
#include "rtc-wrapper.h"

#if RTC_ENABLE_MEDIA == 1
#include "media-audio-wrapper.h"
#include "media-handlers-wrapper.h"
#include "media-rtppacketizationconfig-wrapper.h"
#include "media-rtppacketizer-wrapper.h"
#include "media-track-wrapper.h"
#include "media-video-wrapper.h"
#endif

#if RTC_ENABLE_WEBSOCKET == 1
#include "web-socket-server-wrapper.h"
#include "web-socket-wrapper.h"
#endif

AddonData::~AddonData()
{
  // Clean up any instances still alive when this Node environment (e.g. a Worker) shuts down.
  // Pass fromDestructor=true so we don't call napi_reference_unref during env teardown.
  auto pcs = peerConnections;
  for (auto *inst : pcs)
    inst->doCleanup(/*fromDestructor=*/true);

  auto dcs = dataChannels;
  for (auto *inst : dcs)
    inst->doCleanup(/*fromDestructor=*/true);

  auto muxes = iceUdpMuxListeners;
  for (auto *inst : muxes)
    inst->doCleanup(/*fromDestructor=*/true);

#if RTC_ENABLE_MEDIA == 1
  auto trks = tracks;
  for (auto *inst : trks)
    inst->doCleanup(/*fromDestructor=*/true);
#endif

#if RTC_ENABLE_WEBSOCKET == 1
  auto wss = webSockets;
  for (auto *inst : wss)
    inst->doCleanup(/*fromDestructor=*/true);

  auto servers = webSocketServers;
  for (auto *inst : servers)
    inst->doStop(/*fromDestructor=*/true);
#endif

  RtcWrapper::clearLogCallbackIfActive(logCallback);
}

Napi::Object InitAll(Napi::Env env, Napi::Object exports)
{
  if (!env.GetInstanceData<AddonData>())
    env.SetInstanceData(new AddonData());

  RtcWrapper::Init(env, exports);

#if RTC_ENABLE_MEDIA == 1
  AV1RtpPacketizerWrapper::Init(env, exports);
  H264RtpPacketizerWrapper::Init(env, exports);
  H265RtpPacketizerWrapper::Init(env, exports);
  PacingHandlerWrapper::Init(env, exports);
  RtcpNackResponderWrapper::Init(env, exports);
  RtcpReceivingSessionWrapper::Init(env, exports);
  RtcpSrReporterWrapper::Init(env, exports);
  RtpPacketizationConfigWrapper::Init(env, exports);
  RtpPacketizerWrapper::Init(env, exports);
  TrackWrapper::Init(env, exports);
  VideoWrapper::Init(env, exports);
  AudioWrapper::Init(env, exports);
#endif

  DataChannelWrapper::Init(env, exports);
  IceUdpMuxListenerWrapper::Init(env, exports);
  PeerConnectionWrapper::Init(env, exports);

#if RTC_ENABLE_WEBSOCKET == 1
  WebSocketWrapper::Init(env, exports);
  WebSocketServerWrapper::Init(env, exports);
#endif

  return exports;
}

NODE_API_MODULE(nodeDataChannel, InitAll)
