#include "peer-connection-wrapper.h"
#include "data-channel-wrapper.h"

#if RTC_ENABLE_MEDIA == 1
#include "media-audio-wrapper.h"
#include "media-track-wrapper.h"
#include "media-video-wrapper.h"
#endif

#include "plog/Log.h"

#include <algorithm>
#include <cctype>
#include <iostream>

namespace
{
const char *stateToString(rtc::PeerConnection::State state)
{
  switch (state)
  {
  case rtc::PeerConnection::State::New:
    return "new";
  case rtc::PeerConnection::State::Connecting:
    return "connecting";
  case rtc::PeerConnection::State::Connected:
    return "connected";
  case rtc::PeerConnection::State::Disconnected:
    return "disconnected";
  case rtc::PeerConnection::State::Failed:
    return "failed";
  case rtc::PeerConnection::State::Closed:
    return "closed";
  default:
    return "unknown";
  }
}

const char *iceStateToString(rtc::PeerConnection::IceState state)
{
  switch (state)
  {
  case rtc::PeerConnection::IceState::New:
    return "new";
  case rtc::PeerConnection::IceState::Checking:
    return "checking";
  case rtc::PeerConnection::IceState::Connected:
    return "connected";
  case rtc::PeerConnection::IceState::Completed:
    return "completed";
  case rtc::PeerConnection::IceState::Failed:
    return "failed";
  case rtc::PeerConnection::IceState::Disconnected:
    return "disconnected";
  case rtc::PeerConnection::IceState::Closed:
    return "closed";
  default:
    return "unknown";
  }
}

const char *signalingStateToString(rtc::PeerConnection::SignalingState state)
{
  switch (state)
  {
  case rtc::PeerConnection::SignalingState::Stable:
    return "stable";
  case rtc::PeerConnection::SignalingState::HaveLocalOffer:
    return "have-local-offer";
  case rtc::PeerConnection::SignalingState::HaveRemoteOffer:
    return "have-remote-offer";
  case rtc::PeerConnection::SignalingState::HaveLocalPranswer:
    return "have-local-pranswer";
  case rtc::PeerConnection::SignalingState::HaveRemotePranswer:
    return "have-remote-pranswer";
  default:
    return "unknown";
  }
}

const char *gatheringStateToString(rtc::PeerConnection::GatheringState state)
{
  switch (state)
  {
  case rtc::PeerConnection::GatheringState::New:
    return "new";
  case rtc::PeerConnection::GatheringState::InProgress:
    return "gathering";
  case rtc::PeerConnection::GatheringState::Complete:
    return "complete";
  default:
    return "unknown";
  }
}

const char *candidateTypeToString(rtc::Candidate::Type type)
{
  switch (type)
  {
  case rtc::Candidate::Type::Host:
    return "host";
  case rtc::Candidate::Type::PeerReflexive:
    return "prflx";
  case rtc::Candidate::Type::ServerReflexive:
    return "srflx";
  case rtc::Candidate::Type::Relayed:
    return "relay";
  default:
    return "unknown";
  }
}

const char *candidateTransportTypeToString(rtc::Candidate::TransportType transportType)
{
  switch (transportType)
  {
  case rtc::Candidate::TransportType::Udp:
    return "UDP";
  case rtc::Candidate::TransportType::TcpActive:
    return "TCP_active";
  case rtc::Candidate::TransportType::TcpPassive:
    return "TCP_passive";
  case rtc::Candidate::TransportType::TcpSo:
    return "TCP_so";
  case rtc::Candidate::TransportType::TcpUnknown:
    return "TCP_unknown";
  default:
    return "unknown";
  }
}
bool parsePeerConnectionConfig(Napi::Env env, const Napi::Object &config, rtc::Configuration &rtcConfig)
{
  if (!config.Get("iceServers").IsArray())
  {
    Napi::TypeError::New(env, "iceServers(Array) expected").ThrowAsJavaScriptException();
    return false;
  }

  Napi::Array iceServers = config.Get("iceServers").As<Napi::Array>();
  for (uint32_t i = 0; i < iceServers.Length(); i++)
  {
    Napi::Value item = iceServers.Get(i);
    if (item.IsString())
    {
      try
      {
        rtcConfig.iceServers.emplace_back(item.As<Napi::String>().ToString());
      }
      catch (std::exception &ex)
      {
        Napi::TypeError::New(env, "SyntaxError: IceServer config error: " + std::string(ex.what()))
            .ThrowAsJavaScriptException();
        return false;
      }
    }
    else
    {
      if (!item.IsObject())
      {
        Napi::TypeError::New(env, "IceServer config should be a string Or an object").ThrowAsJavaScriptException();
        return false;
      }

      Napi::Object iceServer = item.As<Napi::Object>();
      if (!iceServer.Get("hostname").IsString() || !iceServer.Get("port").IsNumber())
      {
        Napi::TypeError::New(env, "IceServer config error (hostname OR/AND port is not suitable)")
            .ThrowAsJavaScriptException();
        return false;
      }

      uint16_t serverPort = 0;
      if (!parseUint16(env, iceServer.Get("port"), "iceServer.port", serverPort))
        return false;

      Napi::Value relayTypeVal = iceServer.Get("relayType");
      if (!relayTypeVal.IsUndefined())
      {
        if (!relayTypeVal.IsString() || !iceServer.Get("username").IsString() || !iceServer.Get("password").IsString())
        {
          Napi::TypeError::New(env, "IceServer config error (username AND password is needed)")
              .ThrowAsJavaScriptException();
          return false;
        }

        std::string relayTypeStr = relayTypeVal.As<Napi::String>().ToString();
        rtc::IceServer::RelayType relayType;
        if (relayTypeStr == "TurnUdp")
          relayType = rtc::IceServer::RelayType::TurnUdp;
        else if (relayTypeStr == "TurnTcp")
          relayType = rtc::IceServer::RelayType::TurnTcp;
        else if (relayTypeStr == "TurnTls")
          relayType = rtc::IceServer::RelayType::TurnTls;
        else
        {
          Napi::TypeError::New(env, "Unknown relayType, expected \"TurnUdp\", \"TurnTcp\", or \"TurnTls\"")
              .ThrowAsJavaScriptException();
          return false;
        }

        rtcConfig.iceServers.emplace_back(
            rtc::IceServer(iceServer.Get("hostname").As<Napi::String>(), serverPort,
                           iceServer.Get("username").As<Napi::String>(), iceServer.Get("password").As<Napi::String>(),
                           relayType));
      }
      else
      {
        if (!iceServer.Get("username").IsUndefined() || !iceServer.Get("password").IsUndefined())
        {
          Napi::TypeError::New(
              env, "IceServer config with username/password requires relayType (\"TurnUdp\", \"TurnTcp\", or \"TurnTls\")")
              .ThrowAsJavaScriptException();
          return false;
        }
        rtcConfig.iceServers.emplace_back(rtc::IceServer(iceServer.Get("hostname").As<Napi::String>(), serverPort));
      }
    }
  }

  Napi::Value proxyVal = config.Get("proxyServer");
  if (!proxyVal.IsUndefined() && !parseProxyServer(env, proxyVal, rtcConfig.proxyServer))
    return false;

  if (!parseOptionalString(env, config, "bindAddress", rtcConfig.bindAddress))
    return false;

  if (!config.Get("portRangeBegin").IsUndefined())
  {
    uint16_t port = 0;
    if (!parseUint16(env, config.Get("portRangeBegin"), "portRangeBegin", port))
      return false;
    rtcConfig.portRangeBegin = port;
  }
  if (!config.Get("portRangeEnd").IsUndefined())
  {
    uint16_t port = 0;
    if (!parseUint16(env, config.Get("portRangeEnd"), "portRangeEnd", port))
      return false;
    rtcConfig.portRangeEnd = port;
  }

  if (!parseOptionalBool(env, config, "enableIceTcp", rtcConfig.enableIceTcp) ||
      !parseOptionalBool(env, config, "enableIceUdpMux", rtcConfig.enableIceUdpMux) ||
      !parseOptionalBool(env, config, "disableAutoNegotiation", rtcConfig.disableAutoNegotiation) ||
      !parseOptionalBool(env, config, "disableAutoGathering", rtcConfig.disableAutoGathering) ||
      !parseOptionalBool(env, config, "forceMediaTransport", rtcConfig.forceMediaTransport))
    return false;

  if (!config.Get("maxMessageSize").IsUndefined())
  {
    size_t sizeVal = 0;
    if (!parsePositiveSize(env, config.Get("maxMessageSize"), "maxMessageSize", sizeVal))
      return false;
    rtcConfig.maxMessageSize = sizeVal;
  }

  if (!config.Get("mtu").IsUndefined())
  {
    size_t mtuVal = 0;
    if (!parsePositiveSize(env, config.Get("mtu"), "mtu", mtuVal))
      return false;
    rtcConfig.mtu = mtuVal;
  }

  if (!config.Get("iceTransportPolicy").IsUndefined())
  {
    if (!config.Get("iceTransportPolicy").IsString())
    {
      Napi::TypeError::New(env, "Invalid ICE transport policy, expected string").ThrowAsJavaScriptException();
      return false;
    }
    std::string strPolicy = config.Get("iceTransportPolicy").As<Napi::String>().ToString();
    if (strPolicy == "all")
      rtcConfig.iceTransportPolicy = rtc::TransportPolicy::All;
    else if (strPolicy == "relay")
      rtcConfig.iceTransportPolicy = rtc::TransportPolicy::Relay;
    else
    {
      Napi::TypeError::New(env, "Unknown ICE transport policy").ThrowAsJavaScriptException();
      return false;
    }
  }

  if (!parseOptionalBool(env, config, "disableFingerprintVerification", rtcConfig.disableFingerprintVerification))
    return false;
  if (!parseOptionalString(env, config, "certificatePemFile", rtcConfig.certificatePemFile))
    return false;
  if (!parseOptionalString(env, config, "keyPemFile", rtcConfig.keyPemFile))
    return false;
  if (!parseOptionalString(env, config, "keyPemPass", rtcConfig.keyPemPass))
    return false;

  return true;
}
} // namespace

void PeerConnectionWrapper::CloseAll(AddonData *data)
{
  if (!data)
    return;
  PLOG_DEBUG << "CloseAll() called";
  auto copy = data->peerConnections;
  for (auto *inst : copy)
    inst->doClose();
}

void PeerConnectionWrapper::CleanupAll(AddonData *data)
{
  if (!data)
    return;
  PLOG_DEBUG << "CleanupAll() called";
  auto copy = data->peerConnections;
  for (auto *inst : copy)
    inst->doCleanup();
}

Napi::Object PeerConnectionWrapper::Init(Napi::Env env, Napi::Object exports)
{
  Napi::HandleScope scope(env);

  Napi::Function func = DefineClass(env, "PeerConnection", {
    InstanceMethod("close", &PeerConnectionWrapper::close),
        InstanceMethod("setLocalDescription", &PeerConnectionWrapper::setLocalDescription),
        InstanceMethod("setRemoteDescription", &PeerConnectionWrapper::setRemoteDescription),
        InstanceMethod("localDescription", &PeerConnectionWrapper::localDescription),
        InstanceMethod("remoteDescription", &PeerConnectionWrapper::remoteDescription),
        InstanceMethod("remoteFingerprint", &PeerConnectionWrapper::remoteFingerprint),
        InstanceMethod("addRemoteCandidate", &PeerConnectionWrapper::addRemoteCandidate),
        InstanceMethod("createDataChannel", &PeerConnectionWrapper::createDataChannel),

#if RTC_ENABLE_MEDIA == 1
        InstanceMethod("addTrack", &PeerConnectionWrapper::addTrack),
        InstanceMethod("onTrack", &PeerConnectionWrapper::onTrack),
#endif
        InstanceMethod("hasMedia", &PeerConnectionWrapper::hasMedia),
        InstanceMethod("state", &PeerConnectionWrapper::state),
        InstanceMethod("iceState", &PeerConnectionWrapper::iceState),
        InstanceMethod("signalingState", &PeerConnectionWrapper::signalingState),
        InstanceMethod("gatheringState", &PeerConnectionWrapper::gatheringState),
        InstanceMethod("onLocalDescription", &PeerConnectionWrapper::onLocalDescription),
        InstanceMethod("onLocalCandidate", &PeerConnectionWrapper::onLocalCandidate),
        InstanceMethod("onStateChange", &PeerConnectionWrapper::onStateChange),
        InstanceMethod("onIceStateChange", &PeerConnectionWrapper::onIceStateChange),
        InstanceMethod("onSignalingStateChange", &PeerConnectionWrapper::onSignalingStateChange),
        InstanceMethod("onGatheringStateChange", &PeerConnectionWrapper::onGatheringStateChange),
        InstanceMethod("onDataChannel", &PeerConnectionWrapper::onDataChannel),
        InstanceMethod("bytesSent", &PeerConnectionWrapper::bytesSent),
        InstanceMethod("bytesReceived", &PeerConnectionWrapper::bytesReceived),
        InstanceMethod("rtt", &PeerConnectionWrapper::rtt),
        InstanceMethod("getSelectedCandidatePair", &PeerConnectionWrapper::getSelectedCandidatePair),
        InstanceMethod("maxDataChannelId", &PeerConnectionWrapper::maxDataChannelId),
        InstanceMethod("maxMessageSize", &PeerConnectionWrapper::maxMessageSize),
  });

  exports.Set("PeerConnection", func);
  return exports;
}

PeerConnectionWrapper::PeerConnectionWrapper(const Napi::CallbackInfo &info)
    : Napi::ObjectWrap<PeerConnectionWrapper>(info)
{
  PLOG_DEBUG << "Constructor called";
  Napi::Env env = info.Env();
  int length = info.Length();

  if (length < 2 || !info[0].IsString() || !info[1].IsObject())
  {
    Napi::TypeError::New(env, "Peer Name (String) and Configuration (Object) expected").ThrowAsJavaScriptException();
    return;
  }

  rtc::Configuration rtcConfig;
  if (!parsePeerConnectionConfig(env, info[1].As<Napi::Object>(), rtcConfig))
    return;

  ActiveInstanceGuard guard;
  try
  {
    PLOG_DEBUG << "Creating a new Peer Connection";
    mRtcPeerConnPtr = std::make_unique<rtc::PeerConnection>(rtcConfig);
  }
  catch (std::exception &ex)
  {
    Napi::Error::New(env, std::string("libdatachannel error while creating peer connection: ") + ex.what())
        .ThrowAsJavaScriptException();
    return;
  }

  guard.commit(mAddonData, env, &AddonData::peerConnections, this);
}

PeerConnectionWrapper::~PeerConnectionWrapper()
{
  PLOG_DEBUG << "Destructor called";
  if (mRtcPeerConnPtr)
    mRtcPeerConnPtr->resetCallbacks();
  doClose(/*fromDestructor=*/true);
  doCleanup(/*fromDestructor=*/true);
}

void PeerConnectionWrapper::scheduleCloseOnce(PeerConnectionWrapper *self, const std::shared_ptr<CloseState> &state,
                                              const std::shared_ptr<ThreadSafeCallback> &queue)
{
  EventQueueState::scheduleCloseOnce(self, state, queue,
                                     [self](Napi::Env env)
                                     {
                                       if (!self->mOnStateChangeFn.IsEmpty())
                                         self->mOnStateChangeFn.Call({Napi::String::New(env, "closed")});
                                     });
}

std::shared_ptr<ThreadSafeCallback> PeerConnectionWrapper::ensureEventQueue(Napi::Env env)
{
  return mEvents.ensure(this, env);
}

void PeerConnectionWrapper::ensureCloseHook()
{
  if (!mEvents.closeHookInstalled && mRtcPeerConnPtr && mEvents.queue)
  {
    mEvents.closeHookInstalled = true;
    auto queue = mEvents.queue;
    mRtcPeerConnPtr->onStateChange(
        [self = this, state = mEvents.closeState, queue](rtc::PeerConnection::State pcState)
        {
          if (!state->alive.load())
            return;

          if (pcState == rtc::PeerConnection::State::Closed)
          {
            scheduleCloseOnce(self, state, queue);
            return;
          }

          queue->dispatch(
              [self, state, pcState](Napi::Env env)
              {
                if (!state->alive.load())
                  throw ThreadSafeCallback::CancelException();
                if (!self->mOnStateChangeFn.IsEmpty())
                  self->mOnStateChangeFn.Call({Napi::String::New(env, stateToString(pcState))});
              });
        });
  }

  if (mRtcPeerConnPtr && mRtcPeerConnPtr->state() == rtc::PeerConnection::State::Closed)
    scheduleCloseOnce(this, mEvents.closeState, mEvents.queue);
}

void PeerConnectionWrapper::doClose(bool fromDestructor)
{
  PLOG_DEBUG << "doClose() called";
  if (fromDestructor)
  {
    if (mRtcPeerConnPtr)
    {
      try
      {
        mRtcPeerConnPtr->close();
      }
      catch (std::exception &ex)
      {
        std::cerr << std::string("libdatachannel error while closing peer connection: ") + ex.what() << std::endl;
      }
      mRtcPeerConnPtr.reset();
    }
    doCleanup(/*fromDestructor=*/true);
    return;
  }

  // Stop non-close callbacks immediately, but keep onIceStateChange and onStateChange alive
  // so synchronous IceState::Closed and State::Closed transitions still deliver in FIFO order.
  ++mOnLocalDescriptionGen;
  ++mOnLocalCandidateGen;
  ++mOnSignalingStateChangeGen;
  ++mOnGatheringStateChangeGen;
  ++mOnDataChannelGen;
  resetFuncRef(mOnLocalDescriptionFn, false);
  resetFuncRef(mOnLocalCandidateFn, false);
  resetFuncRef(mOnSignalingStateChangeFn, false);
  resetFuncRef(mOnGatheringStateChangeFn, false);
  resetFuncRef(mOnDataChannelFn, false);
#if RTC_ENABLE_MEDIA == 1
  ++mOnTrackGen;
  resetFuncRef(mOnTrackFn, false);
#endif

  bool hasUserCloseCb = !mOnStateChangeFn.IsEmpty() || !mOnIceStateChangeFn.IsEmpty();
  bool willEmitClose = hasUserCloseCb && mEvents.closeState->closeDispatched.load();

  if (mRtcPeerConnPtr)
  {
    bool wasClosed = mRtcPeerConnPtr->state() == rtc::PeerConnection::State::Closed;
    bool closedOk = false;
    try
    {
      mRtcPeerConnPtr->close();
      closedOk = true;
    }
    catch (std::exception &ex)
    {
      std::cerr << std::string("libdatachannel error while closing peer connection: ") + ex.what() << std::endl;
    }
    mRtcPeerConnPtr.reset();

    // Guard against a background thread setting State::Closed just before invoking onStateChange:
    // ensure close callback + doCleanup() is scheduled exactly once.
    if (hasUserCloseCb && (wasClosed || closedOk))
    {
      scheduleCloseOnce(this, mEvents.closeState, mEvents.queue);
    }

    willEmitClose = hasUserCloseCb && mEvents.closeState->closeDispatched.load();
  }

  if (!willEmitClose)
    doCleanup();
}

void PeerConnectionWrapper::close(const Napi::CallbackInfo &) { doClose(); }

void PeerConnectionWrapper::doCleanup(bool fromDestructor)
{
  PLOG_DEBUG << "doCleanup() called";
  mEvents.closeState->alive.store(false);
  resetCallback(mEvents.queue);
  resetFuncRef(mOnLocalDescriptionFn, fromDestructor);
  resetFuncRef(mOnLocalCandidateFn, fromDestructor);
  resetFuncRef(mOnStateChangeFn, fromDestructor);
  resetFuncRef(mOnIceStateChangeFn, fromDestructor);
  resetFuncRef(mOnSignalingStateChangeFn, fromDestructor);
  resetFuncRef(mOnGatheringStateChangeFn, fromDestructor);
  resetFuncRef(mOnDataChannelFn, fromDestructor);
#if RTC_ENABLE_MEDIA == 1
  resetFuncRef(mOnTrackFn, fromDestructor);
#endif
  mRtcPeerConnPtr.reset();

  if (mAddonData)
  {
    if (mAddonData->peerConnections.erase(this) > 0)
      --gActiveNativeInstances;
    mAddonData = nullptr;
  }

  mEvents.selfRef.release(this, fromDestructor);
}

void PeerConnectionWrapper::setLocalDescription(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();
  int length = info.Length();

  if (!mRtcPeerConnPtr)
  {
    Napi::Error::New(env, "setLocalDescription() called on destroyed peer connection").ThrowAsJavaScriptException();
    return;
  }

  rtc::Description::Type type = rtc::Description::Type::Unspec;
  rtc::LocalDescriptionInit init;

  if (length > 0 && !info[0].IsUndefined())
  {
    if (!info[0].IsString())
    {
      Napi::TypeError::New(env, "type (String) expected").ThrowAsJavaScriptException();
      return;
    }
    std::string typeStr = info[0].As<Napi::String>().ToString();
    std::transform(typeStr.begin(), typeStr.end(), typeStr.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (typeStr == "answer")
      type = rtc::Description::Type::Answer;
    else if (typeStr == "offer")
      type = rtc::Description::Type::Offer;
    else if (typeStr == "pranswer")
      type = rtc::Description::Type::Pranswer;
    else if (typeStr == "rollback")
      type = rtc::Description::Type::Rollback;
    else if (typeStr != "unspec")
    {
      Napi::TypeError::New(env, "Unknown local description type").ThrowAsJavaScriptException();
      return;
    }
  }

  if (length > 1 && !info[1].IsUndefined())
  {
    if (info[1].IsObject())
    {
      Napi::Object obj = info[1].As<Napi::Object>();
      if (obj.Get("iceUfrag").IsString())
        init.iceUfrag = obj.Get("iceUfrag").As<Napi::String>();
      if (obj.Get("icePwd").IsString())
        init.icePwd = obj.Get("icePwd").As<Napi::String>();
    }
  }

  mRtcPeerConnPtr->setLocalDescription(type, init);
}

void PeerConnectionWrapper::setRemoteDescription(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();
  int length = info.Length();

  if (!mRtcPeerConnPtr)
  {
    Napi::Error::New(env, "setRemoteDescription() called on destroyed peer connection").ThrowAsJavaScriptException();
    return;
  }

  if (length < 2 || !info[0].IsString() || !info[1].IsString())
  {
    Napi::TypeError::New(env, "String,String expected").ThrowAsJavaScriptException();
    return;
  }

  std::string sdp = info[0].As<Napi::String>().ToString();
  std::string type = info[1].As<Napi::String>().ToString();

  try
  {
    rtc::Description desc(sdp, type);
    mRtcPeerConnPtr->setRemoteDescription(desc);
  }
  catch (std::exception &ex)
  {
    Napi::Error::New(env, std::string("libdatachannel error while adding remote description: ") + ex.what())
        .ThrowAsJavaScriptException();
  }
}

Napi::Value PeerConnectionWrapper::localDescription(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();
  std::optional<rtc::Description> desc = mRtcPeerConnPtr ? mRtcPeerConnPtr->localDescription() : std::nullopt;
  if (!desc.has_value())
    return env.Null();

  Napi::Object obj = Napi::Object::New(env);
  obj.Set("type", desc->typeString());
  obj.Set("sdp", desc.value());
  return obj;
}

Napi::Value PeerConnectionWrapper::remoteDescription(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();
  std::optional<rtc::Description> desc = mRtcPeerConnPtr ? mRtcPeerConnPtr->remoteDescription() : std::nullopt;
  if (!desc.has_value())
    return env.Null();

  Napi::Object obj = Napi::Object::New(env);
  obj.Set("type", desc->typeString());
  obj.Set("sdp", desc.value());
  return obj;
}

void PeerConnectionWrapper::addRemoteCandidate(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();
  int length = info.Length();

  if (!mRtcPeerConnPtr)
  {
    Napi::Error::New(env, "addRemoteCandidate() called on destroyed peer connection").ThrowAsJavaScriptException();
    return;
  }

  if (length < 2 || !info[0].IsString() || !info[1].IsString())
  {
    Napi::TypeError::New(env, "String, String expected").ThrowAsJavaScriptException();
    return;
  }

  try
  {
    std::string candidate = info[0].As<Napi::String>().ToString();
    std::string mid = info[1].As<Napi::String>().ToString();
    mRtcPeerConnPtr->addRemoteCandidate(rtc::Candidate(candidate, mid));
  }
  catch (std::exception &ex)
  {
    Napi::Error::New(env, std::string("libdatachannel error while adding remote candidate: ") + ex.what())
        .ThrowAsJavaScriptException();
  }
}

Napi::Value PeerConnectionWrapper::createDataChannel(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();
  int length = info.Length();

  if (!mRtcPeerConnPtr)
  {
    Napi::Error::New(env, "createDataChannel() called on destroyed peer connection").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (length < 1 || !info[0].IsString())
  {
    Napi::TypeError::New(env, "Data Channel Label expected").ThrowAsJavaScriptException();
    return env.Null();
  }

  rtc::DataChannelInit init;
  if (length > 1 && !info[1].IsUndefined())
  {
    if (!info[1].IsObject())
    {
      Napi::TypeError::New(env, "Data Channel Init Config expected(As Object)").ThrowAsJavaScriptException();
      return env.Null();
    }

    Napi::Object initConfig = info[1].As<Napi::Object>();

    if (!initConfig.Get("protocol").IsUndefined())
    {
      if (!initConfig.Get("protocol").IsString())
      {
        Napi::TypeError::New(env, "Wrong DataChannel Init Config (protocol)").ThrowAsJavaScriptException();
        return env.Null();
      }
      init.protocol = initConfig.Get("protocol").As<Napi::String>();
    }

    if (!initConfig.Get("negotiated").IsUndefined())
    {
      if (!initConfig.Get("negotiated").IsBoolean())
      {
        Napi::TypeError::New(env, "Wrong DataChannel Init Config (negotiated)").ThrowAsJavaScriptException();
        return env.Null();
      }
      init.negotiated = initConfig.Get("negotiated").As<Napi::Boolean>();
    }

    if (!initConfig.Get("id").IsUndefined())
    {
      uint16_t channelId = 0;
      if (!parseUint16(env, initConfig.Get("id"), "id", channelId))
        return env.Null();
      init.id = channelId;
    }

    if (!initConfig.Get("unordered").IsUndefined())
    {
      if (!initConfig.Get("unordered").IsBoolean())
      {
        Napi::TypeError::New(env, "Wrong DataChannel Init Config (unordered)").ThrowAsJavaScriptException();
        return env.Null();
      }
      init.reliability.unordered = initConfig.Get("unordered").As<Napi::Boolean>();
    }

    if (!initConfig.Get("maxPacketLifeTime").IsUndefined() && !initConfig.Get("maxPacketLifeTime").IsNull() &&
        !initConfig.Get("maxRetransmits").IsUndefined() && !initConfig.Get("maxRetransmits").IsNull())
    {
      Napi::TypeError::New(env, "Wrong DataChannel Init Config, maxPacketLifeTime and maxRetransmits are exclusive")
          .ThrowAsJavaScriptException();
      return env.Null();
    }

    if (!initConfig.Get("maxPacketLifeTime").IsUndefined() && !initConfig.Get("maxPacketLifeTime").IsNull())
    {
      uint32_t lifetimeMs = 0;
      if (!parseNonNegativeUint32(env, initConfig.Get("maxPacketLifeTime"), "maxPacketLifeTime", lifetimeMs))
        return env.Null();
      init.reliability.maxPacketLifeTime = std::chrono::milliseconds(lifetimeMs);
    }
    else if (!initConfig.Get("maxRetransmits").IsUndefined() && !initConfig.Get("maxRetransmits").IsNull())
    {
      uint32_t maxRetransmits = 0;
      if (!parseNonNegativeUint32(env, initConfig.Get("maxRetransmits"), "maxRetransmits", maxRetransmits))
        return env.Null();
      init.reliability.maxRetransmits = maxRetransmits;
    }
  }

  try
  {
    std::string label = info[0].As<Napi::String>().ToString();
    std::shared_ptr<rtc::DataChannel> dataChannel = mRtcPeerConnPtr->createDataChannel(label, std::move(init));
    auto *data = getAddonData(env);
    return data->dataChannelCtor.New({Napi::External<std::shared_ptr<rtc::DataChannel>>::New(env, &dataChannel)});
  }
  catch (std::exception &ex)
  {
    Napi::Error::New(env, std::string("libdatachannel error while creating datachannel: ") + ex.what())
        .ThrowAsJavaScriptException();
    return env.Null();
  }
}

template <typename BindFn>
void PeerConnectionWrapper::bindCallback(const Napi::CallbackInfo &info, const char *methodName,
                                         Napi::FunctionReference &fnRef, uint64_t &genRef, BindFn &&bindFn)
{
  Napi::Env env = info.Env();
  if (!mRtcPeerConnPtr)
  {
    Napi::Error::New(env, std::string(methodName) + "() called on destroyed peer connection")
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
  bindFn(mEvents.closeState, std::move(queue), gen);
  ensureCloseHook();
}

void PeerConnectionWrapper::onLocalDescription(const Napi::CallbackInfo &info)
{
  bindCallback(info, "onLocalDescription", mOnLocalDescriptionFn, mOnLocalDescriptionGen,
               [this](auto state, auto queue, uint64_t gen)
               {
                 mRtcPeerConnPtr->onLocalDescription(
                     [this, state, queue, gen](rtc::Description sdp)
                     {
                       if (!state->alive.load())
                         return;
                       queue->dispatch(
                           [this, state, gen, sdp = std::move(sdp)](Napi::Env env)
                           {
                             if (!state->alive.load() || mOnLocalDescriptionGen != gen ||
                                 mOnLocalDescriptionFn.IsEmpty())
                               throw ThreadSafeCallback::CancelException();
                             mOnLocalDescriptionFn.Call(
                                 {Napi::String::New(env, std::string(sdp)), Napi::String::New(env, sdp.typeString())});
                           });
                     });
               });
}

void PeerConnectionWrapper::onLocalCandidate(const Napi::CallbackInfo &info)
{
  bindCallback(info, "onLocalCandidate", mOnLocalCandidateFn, mOnLocalCandidateGen,
               [this](auto state, auto queue, uint64_t gen)
               {
                 mRtcPeerConnPtr->onLocalCandidate(
                     [this, state, queue, gen](rtc::Candidate cand)
                     {
                       if (!state->alive.load())
                         return;
                       queue->dispatch(
                           [this, state, gen, cand = std::move(cand)](Napi::Env env)
                           {
                             if (!state->alive.load() || mOnLocalCandidateGen != gen || mOnLocalCandidateFn.IsEmpty())
                               throw ThreadSafeCallback::CancelException();
                             mOnLocalCandidateFn.Call(
                                 {Napi::String::New(env, std::string(cand)), Napi::String::New(env, cand.mid())});
                           });
                     });
               });
}

void PeerConnectionWrapper::onStateChange(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();
  if (!mRtcPeerConnPtr)
  {
    Napi::Error::New(env, "onStateChange() called on destroyed peer connection").ThrowAsJavaScriptException();
    return;
  }
  if (info.Length() < 1 || !info[0].IsFunction())
  {
    Napi::TypeError::New(env, "Function expected").ThrowAsJavaScriptException();
    return;
  }

  ensureEventQueue(env);
  mOnStateChangeFn = Napi::Persistent(info[0].As<Napi::Function>());
  ensureCloseHook();
}

void PeerConnectionWrapper::onIceStateChange(const Napi::CallbackInfo &info)
{
  bindCallback(info, "onIceStateChange", mOnIceStateChangeFn, mOnIceStateChangeGen,
               [this](auto state, auto queue, uint64_t gen)
               {
                 mRtcPeerConnPtr->onIceStateChange(
                     [this, state, queue, gen](rtc::PeerConnection::IceState iceState)
                     {
                       if (!state->alive.load())
                         return;
                       queue->dispatch(
                           [this, state, gen, iceState](Napi::Env env)
                           {
                             if (!state->alive.load() || mOnIceStateChangeGen != gen || mOnIceStateChangeFn.IsEmpty())
                               throw ThreadSafeCallback::CancelException();
                             mOnIceStateChangeFn.Call({Napi::String::New(env, iceStateToString(iceState))});
                           });
                     });
               });
}

void PeerConnectionWrapper::onSignalingStateChange(const Napi::CallbackInfo &info)
{
  bindCallback(info, "onSignalingStateChange", mOnSignalingStateChangeFn, mOnSignalingStateChangeGen,
               [this](auto state, auto queue, uint64_t gen)
               {
                 mRtcPeerConnPtr->onSignalingStateChange(
                     [this, state, queue, gen](rtc::PeerConnection::SignalingState sigState)
                     {
                       if (!state->alive.load())
                         return;
                       queue->dispatch(
                           [this, state, gen, sigState](Napi::Env env)
                           {
                             if (!state->alive.load() || mOnSignalingStateChangeGen != gen ||
                                 mOnSignalingStateChangeFn.IsEmpty())
                               throw ThreadSafeCallback::CancelException();
                             mOnSignalingStateChangeFn.Call(
                                 {Napi::String::New(env, signalingStateToString(sigState))});
                           });
                     });
               });
}

void PeerConnectionWrapper::onGatheringStateChange(const Napi::CallbackInfo &info)
{
  bindCallback(info, "onGatheringStateChange", mOnGatheringStateChangeFn, mOnGatheringStateChangeGen,
               [this](auto state, auto queue, uint64_t gen)
               {
                 mRtcPeerConnPtr->onGatheringStateChange(
                     [this, state, queue, gen](rtc::PeerConnection::GatheringState gathState)
                     {
                       if (!state->alive.load())
                         return;
                       queue->dispatch(
                           [this, state, gen, gathState](Napi::Env env)
                           {
                             if (!state->alive.load() || mOnGatheringStateChangeGen != gen ||
                                 mOnGatheringStateChangeFn.IsEmpty())
                               throw ThreadSafeCallback::CancelException();
                             mOnGatheringStateChangeFn.Call(
                                 {Napi::String::New(env, gatheringStateToString(gathState))});
                           });
                     });
               });
}

void PeerConnectionWrapper::onDataChannel(const Napi::CallbackInfo &info)
{
  bindCallback(info, "onDataChannel", mOnDataChannelFn, mOnDataChannelGen,
               [this](auto state, auto queue, uint64_t gen)
               {
                 mRtcPeerConnPtr->onDataChannel(
                     [this, state, queue, gen](std::shared_ptr<rtc::DataChannel> dc)
                     {
                       if (!state->alive.load())
                         return;
                       queue->dispatch(
                           [this, state, gen, dc = std::move(dc)](Napi::Env env) mutable
                           {
                             if (!state->alive.load() || mOnDataChannelGen != gen || mOnDataChannelFn.IsEmpty())
                               throw ThreadSafeCallback::CancelException();

                             auto *data = getAddonData(env);
                             auto instance = data->dataChannelCtor.New(
                                 {Napi::External<std::shared_ptr<rtc::DataChannel>>::New(env, &dc)});
                             mOnDataChannelFn.Call({instance});
                           });
                     });
               });
}

Napi::Value PeerConnectionWrapper::bytesSent(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();
  if (!mRtcPeerConnPtr)
    return Napi::Number::New(env, 0);

  try
  {
    return Napi::Number::New(env, mRtcPeerConnPtr->bytesSent());
  }
  catch (std::exception &ex)
  {
    Napi::Error::New(env, std::string("libdatachannel error: ") + ex.what()).ThrowAsJavaScriptException();
    return Napi::Number::New(env, 0);
  }
}

Napi::Value PeerConnectionWrapper::bytesReceived(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();
  if (!mRtcPeerConnPtr)
    return Napi::Number::New(env, 0);

  try
  {
    return Napi::Number::New(env, mRtcPeerConnPtr->bytesReceived());
  }
  catch (std::exception &ex)
  {
    Napi::Error::New(env, std::string("libdatachannel error: ") + ex.what()).ThrowAsJavaScriptException();
    return Napi::Number::New(env, 0);
  }
}

Napi::Value PeerConnectionWrapper::rtt(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();
  if (!mRtcPeerConnPtr)
    return Napi::Number::New(env, -1);

  try
  {
    return Napi::Number::New(env, mRtcPeerConnPtr->rtt().value_or(std::chrono::milliseconds(-1)).count());
  }
  catch (std::exception &ex)
  {
    Napi::Error::New(env, std::string("libdatachannel error: ") + ex.what()).ThrowAsJavaScriptException();
    return env.Null();
  }
}

Napi::Value PeerConnectionWrapper::getSelectedCandidatePair(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();
  if (!mRtcPeerConnPtr)
    return env.Null();

  try
  {
    rtc::Candidate local, remote;
    if (!mRtcPeerConnPtr->getSelectedCandidatePair(&local, &remote))
      return env.Null();

    Napi::Object retvalue = Napi::Object::New(env);
    Napi::Object localObj = Napi::Object::New(env);
    Napi::Object remoteObj = Napi::Object::New(env);

    localObj.Set("address", local.address().value_or("?"));
    localObj.Set("port", local.port().value_or(0));
    localObj.Set("type", candidateTypeToString(local.type()));
    localObj.Set("transportType", candidateTransportTypeToString(local.transportType()));
    localObj.Set("candidate", local.candidate());
    localObj.Set("mid", local.mid());
    localObj.Set("priority", local.priority());

    remoteObj.Set("address", remote.address().value_or("?"));
    remoteObj.Set("port", remote.port().value_or(0));
    remoteObj.Set("type", candidateTypeToString(remote.type()));
    remoteObj.Set("transportType", candidateTransportTypeToString(remote.transportType()));
    remoteObj.Set("candidate", remote.candidate());
    remoteObj.Set("mid", remote.mid());
    remoteObj.Set("priority", remote.priority());

    retvalue.Set("local", localObj);
    retvalue.Set("remote", remoteObj);
    return retvalue;
  }
  catch (std::exception &ex)
  {
    Napi::Error::New(env, std::string("libdatachannel error: ") + ex.what()).ThrowAsJavaScriptException();
    return env.Null();
  }
}

Napi::Value PeerConnectionWrapper::maxDataChannelId(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();
  if (!mRtcPeerConnPtr)
    return Napi::Number::New(env, 0);

  try
  {
    return Napi::Number::New(env, mRtcPeerConnPtr->maxDataChannelId());
  }
  catch (std::exception &ex)
  {
    Napi::Error::New(env, std::string("libdatachannel error: ") + ex.what()).ThrowAsJavaScriptException();
    return Napi::Number::New(env, 0);
  }
}

Napi::Value PeerConnectionWrapper::maxMessageSize(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();
  if (!mRtcPeerConnPtr)
    return Napi::Number::New(env, 0);

  try
  {
    return Napi::Number::New(env, mRtcPeerConnPtr->remoteMaxMessageSize());
  }
  catch (std::exception &ex)
  {
    Napi::Error::New(env, std::string("libdatachannel error: ") + ex.what()).ThrowAsJavaScriptException();
    return Napi::Number::New(env, 0);
  }
}

Napi::Value PeerConnectionWrapper::remoteFingerprint(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();
  if (!mRtcPeerConnPtr)
    return env.Null();

  try
  {
    auto fingerprint = mRtcPeerConnPtr->remoteFingerprint();
    if (!fingerprint.isValid())
      return env.Null();

    Napi::Object fingerprintObject = Napi::Object::New(env);
    fingerprintObject.Set("value", fingerprint.value);
    fingerprintObject.Set("algorithm", rtc::CertificateFingerprint::AlgorithmIdentifier(fingerprint.algorithm));
    return fingerprintObject;
  }
  catch (std::exception &ex)
  {
    Napi::Error::New(env, std::string("libdatachannel error: ") + ex.what()).ThrowAsJavaScriptException();
    return env.Null();
  }
}

#if RTC_ENABLE_MEDIA == 1
Napi::Value PeerConnectionWrapper::addTrack(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();
  if (!mRtcPeerConnPtr)
  {
    Napi::Error::New(env, "addTrack() called on destroyed peer connection").ThrowAsJavaScriptException();
    return env.Null();
  }
  if (info.Length() < 1 || !info[0].IsObject())
  {
    Napi::TypeError::New(env, "Media class instance expected").ThrowAsJavaScriptException();
    return env.Null();
  }

  try
  {
    auto *data = getAddonData(env);
    Napi::Object obj = info[0].As<Napi::Object>();
    if (auto *videoPtr = unwrapChecked<VideoWrapper>(obj, &kVideoTypeTag))
    {
      std::shared_ptr<rtc::Track> track = mRtcPeerConnPtr->addTrack(videoPtr->getVideoInstance());
      return data->trackCtor.New({Napi::External<std::shared_ptr<rtc::Track>>::New(env, &track)});
    }

    if (auto *audioPtr = unwrapChecked<AudioWrapper>(obj, &kAudioTypeTag))
    {
      std::shared_ptr<rtc::Track> track = mRtcPeerConnPtr->addTrack(audioPtr->getAudioInstance());
      return data->trackCtor.New({Napi::External<std::shared_ptr<rtc::Track>>::New(env, &track)});
    }

    Napi::Error::New(env, std::string("Unknown media type")).ThrowAsJavaScriptException();
    return env.Null();
  }
  catch (std::exception &ex)
  {
    Napi::Error::New(env, std::string("libdatachannel error: ") + ex.what()).ThrowAsJavaScriptException();
    return env.Null();
  }
}

void PeerConnectionWrapper::onTrack(const Napi::CallbackInfo &info)
{
  bindCallback(info, "onTrack", mOnTrackFn, mOnTrackGen,
               [this](auto state, auto queue, uint64_t gen)
               {
                 mRtcPeerConnPtr->onTrack(
                     [this, state, queue, gen](std::shared_ptr<rtc::Track> track)
                     {
                       if (!state->alive.load())
                         return;
                       queue->dispatch(
                           [this, state, gen, track = std::move(track)](Napi::Env env) mutable
                           {
                             if (!state->alive.load() || mOnTrackGen != gen || mOnTrackFn.IsEmpty())
                               throw ThreadSafeCallback::CancelException();

                             auto *data = getAddonData(env);
                             auto instance =
                                 data->trackCtor.New({Napi::External<std::shared_ptr<rtc::Track>>::New(env, &track)});
                             mOnTrackFn.Call({instance});
                           });
                     });
               });
}
#endif

Napi::Value PeerConnectionWrapper::hasMedia(const Napi::CallbackInfo &info)
{
  return Napi::Boolean::New(info.Env(), mRtcPeerConnPtr ? mRtcPeerConnPtr->hasMedia() : false);
}

Napi::Value PeerConnectionWrapper::state(const Napi::CallbackInfo &info)
{
  return Napi::String::New(
      info.Env(), stateToString(mRtcPeerConnPtr ? mRtcPeerConnPtr->state() : rtc::PeerConnection::State::Closed));
}

Napi::Value PeerConnectionWrapper::iceState(const Napi::CallbackInfo &info)
{
  return Napi::String::New(
      info.Env(),
      iceStateToString(mRtcPeerConnPtr ? mRtcPeerConnPtr->iceState() : rtc::PeerConnection::IceState::Closed));
}

Napi::Value PeerConnectionWrapper::signalingState(const Napi::CallbackInfo &info)
{
  if (!mRtcPeerConnPtr)
    return Napi::String::New(info.Env(), "closed");
  return Napi::String::New(info.Env(), signalingStateToString(mRtcPeerConnPtr->signalingState()));
}

Napi::Value PeerConnectionWrapper::gatheringState(const Napi::CallbackInfo &info)
{
  return Napi::String::New(
      info.Env(), gatheringStateToString(mRtcPeerConnPtr ? mRtcPeerConnPtr->gatheringState()
                                                         : rtc::PeerConnection::GatheringState::Complete));
}
