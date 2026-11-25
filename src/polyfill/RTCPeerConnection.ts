import { PeerConnection } from '../lib/index.ts';
import type {
  DataChannelInitConfig,
  DescriptionType,
  SelectedCandidateInfo,
} from '../lib/types.ts';
import { RTCDataChannelEvent, RTCPeerConnectionIceEvent } from './Events.ts';
import type RTCCertificate from './RTCCertificate.ts';
import RTCDataChannel from './RTCDataChannel.ts';
import RTCIceCandidate from './RTCIceCandidate.ts';
import RTCSctpTransport from './RTCSctpTransport.ts';
import RTCSessionDescription from './RTCSessionDescription.ts';

// extend RTCConfiguration with peerIdentity
interface RTCConfiguration extends globalThis.RTCConfiguration {
  peerIdentity?: string;
  peerConnection?: PeerConnection;
}

interface DeferredSessionDescription {
  promise: Promise<RTCSessionDescription>;
  resolve: (value: RTCSessionDescription) => void;
  reject: (reason?: unknown) => void;
  settled: boolean;
}

export default class RTCPeerConnection extends EventTarget implements globalThis.RTCPeerConnection {
  static async generateCertificate(): Promise<RTCCertificate> {
    throw new DOMException('Not implemented');
  }

  #peerConnection: PeerConnection;
  #localOffer: DeferredSessionDescription;
  #localAnswer: DeferredSessionDescription;
  #localOfferConsumed = false;
  #localAnswerConsumed = false;
  #dataChannels: Set<globalThis.RTCDataChannel>;
  #dataChannelsOpened = 0;
  #dataChannelsClosed = 0;
  #config: globalThis.RTCConfiguration;
  #sctp: RTCSctpTransport;
  #announceNegotiation = false;

  #localCandidates: globalThis.RTCIceCandidate[] = [];
  #remoteCandidates: globalThis.RTCIceCandidate[] = [];

  // events
  onconnectionstatechange: globalThis.RTCPeerConnection['onconnectionstatechange'] = null;
  ondatachannel: globalThis.RTCPeerConnection['ondatachannel'] = null;
  onicecandidate: globalThis.RTCPeerConnection['onicecandidate'] = null;
  onicecandidateerror: globalThis.RTCPeerConnection['onicecandidateerror'] = null;
  oniceconnectionstatechange: globalThis.RTCPeerConnection['oniceconnectionstatechange'] = null;
  onicegatheringstatechange: globalThis.RTCPeerConnection['onicegatheringstatechange'] = null;
  onnegotiationneeded: globalThis.RTCPeerConnection['onnegotiationneeded'] = null;
  onsignalingstatechange: globalThis.RTCPeerConnection['onsignalingstatechange'] = null;
  ontrack: globalThis.RTCPeerConnection['ontrack'] = null;

  private _normalizeConfiguration(
    config: globalThis.RTCConfiguration,
  ): globalThis.RTCConfiguration {
    if (config?.iceServers === null) throw new TypeError('IceServers cannot be null');

    const normalizedServers: globalThis.RTCIceServer[] = [];

    // Check all properties of iceServers without mutating caller input
    if (Array.isArray(config?.iceServers)) {
      for (const server of config.iceServers) {
        if (server === null) throw new TypeError('IceServers cannot be null');
        if (server === undefined) throw new TypeError('IceServers cannot be undefined');
        if (Object.keys(server).length === 0) throw new TypeError('IceServers cannot be empty');

        const rawUrls = typeof server.urls === 'string' ? [server.urls] : [...(server.urls ?? [])];

        if (rawUrls.length === 0 || rawUrls.some((url) => url === ''))
          throw new DOMException('IceServers urls cannot be empty', 'SyntaxError');

        if (
          rawUrls.some((url) => {
            try {
              const parsedURL = new URL(url);
              return !/^(stun:|turn:|turns:)$/.test(parsedURL.protocol);
            } catch {
              return true;
            }
          })
        )
          throw new DOMException('IceServers urls wrong format', 'SyntaxError');

        // Lowercase scheme so libdatachannel accepts RFC 3986 mixed-case schemes like "Stun:"
        const urls = rawUrls.map((url) => url.replace(/^[^:]+:/, (scheme) => scheme.toLowerCase()));

        if (urls.some((url) => /^turns?:/.test(url))) {
          if (!server.username)
            throw new DOMException('IceServers username cannot be null', 'InvalidAccessError');
          if (!server.credential)
            throw new DOMException(
              'IceServers credential cannot be undefined',
              'InvalidAccessError',
            );
        }

        normalizedServers.push({ ...server, urls });
      }
    }

    const iceTransportPolicy = config?.iceTransportPolicy ?? 'all';
    if (iceTransportPolicy !== 'all' && iceTransportPolicy !== 'relay')
      throw new TypeError('IceTransportPolicy must be either "all" or "relay"');

    return {
      ...config,
      iceServers: normalizedServers,
      iceTransportPolicy,
    };
  }

  setConfiguration(config: globalThis.RTCConfiguration): void {
    this._normalizeConfiguration(config);
    throw new DOMException(
      'Updating RTCPeerConnection configuration after construction is not supported',
      'NotSupportedError',
    );
  }

  constructor(config: RTCConfiguration = { iceServers: [], iceTransportPolicy: 'all' }) {
    super();

    this.#config = this._normalizeConfiguration(config);
    this.#localOffer = createDeferredDescription();
    this.#localAnswer = createDeferredDescription();
    this.#dataChannels = new Set();

    try {
      const peerIdentity = config?.peerIdentity ?? `peer-${getRandomString(7)}`;
      this.#peerConnection =
        config?.peerConnection ??
        new PeerConnection(peerIdentity, {
          ...this.#config,
          iceServers:
            this.#config.iceServers?.flatMap((server) => {
              const urls = Array.isArray(server.urls) ? server.urls : [server.urls];

              return urls.map((url) => {
                if (server.username && server.credential) {
                  const [protocol, rest] = url.split(/:(.*)/);
                  const hostPart = rest.replace(/^\/\//, '');
                  return `${protocol}:${encodeURIComponent(server.username)}:${encodeURIComponent(server.credential)}@${hostPart}`;
                }
                return url;
              });
            }) ?? [],
        });
    } catch (error) {
      const message =
        typeof (error as { message?: unknown })?.message === 'string'
          ? (error as { message: string }).message
          : null;
      if (!message) throw new DOMException('Unknown error', 'NotFoundError');
      throw new DOMException(message, 'SyntaxError');
    }

    // forward peerConnection events
    this.#peerConnection.onStateChange(() => {
      this.dispatchEvent(new Event('connectionstatechange'));
    });

    this.#peerConnection.onIceStateChange(() => {
      this.dispatchEvent(new Event('iceconnectionstatechange'));
    });

    this.#peerConnection.onSignalingStateChange(() => {
      this.dispatchEvent(new Event('signalingstatechange'));
    });

    this.#peerConnection.onGatheringStateChange((state) => {
      this.dispatchEvent(new Event('icegatheringstatechange'));
      if (state === 'complete') {
        this.dispatchEvent(new RTCPeerConnectionIceEvent('icecandidate', { candidate: null }));
      }
    });

    this.#peerConnection.onDataChannel((channel) => {
      const dc = new RTCDataChannel(channel);
      this.#trackDataChannel(dc);
      this.dispatchEvent(new RTCDataChannelEvent('datachannel', { channel: dc }));
    });

    this.#peerConnection.onLocalDescription((sdp, type) => {
      this.#announceNegotiation = false;
      if (type === 'offer') {
        if (this.#localOffer.settled) {
          this.#localOffer = createDeferredDescription();
          this.#localOfferConsumed = false;
        }
        this.#localOffer.resolve(new RTCSessionDescription({ sdp, type }));
      }

      if (type === 'answer') {
        if (this.#localAnswer.settled) {
          this.#localAnswer = createDeferredDescription();
          this.#localAnswerConsumed = false;
        }
        this.#localAnswer.resolve(new RTCSessionDescription({ sdp, type }));
      }
    });

    this.#peerConnection.onLocalCandidate((candidate, sdpMid) => {
      if (sdpMid === 'unspec') {
        return;
      }

      const iceCandidate = new RTCIceCandidate({ candidate, sdpMid });
      this.#localCandidates.push(iceCandidate);
      this.dispatchEvent(
        new RTCPeerConnectionIceEvent('icecandidate', { candidate: iceCandidate }),
      );
    });

    // forward events to properties
    this.addEventListener('connectionstatechange', (e) => {
      this.onconnectionstatechange?.(e);
    });
    this.addEventListener('signalingstatechange', (e) => {
      this.onsignalingstatechange?.(e);
    });
    this.addEventListener('iceconnectionstatechange', (e) => {
      this.oniceconnectionstatechange?.(e);
    });
    this.addEventListener('icegatheringstatechange', (e) => {
      this.onicegatheringstatechange?.(e);
    });
    this.addEventListener('datachannel', (e) => {
      this.ondatachannel?.(e as RTCDataChannelEvent);
    });
    this.addEventListener('icecandidate', (e) => {
      this.onicecandidate?.(e as globalThis.RTCPeerConnectionIceEvent);
    });
    this.addEventListener('negotiationneeded', (e) => {
      this.#announceNegotiation = true;
      this.onnegotiationneeded?.(e);
    });

    this.#sctp = new RTCSctpTransport({
      pc: this,
    });
  }

  // Extra Functions
  get ext_maxDataChannelId(): number {
    return this.#peerConnection.maxDataChannelId();
  }

  get ext_maxMessageSize(): number {
    return this.#peerConnection.maxMessageSize();
  }

  get ext_localCandidates(): globalThis.RTCIceCandidate[] {
    return this.#localCandidates;
  }

  get ext_remoteCandidates(): globalThis.RTCIceCandidate[] {
    return this.#remoteCandidates;
  }

  selectedCandidatePair(): {
    local: SelectedCandidateInfo;
    remote: SelectedCandidateInfo;
  } | null {
    return this.#peerConnection.getSelectedCandidatePair();
  }

  get canTrickleIceCandidates(): boolean | null {
    return null;
  }

  get connectionState(): globalThis.RTCPeerConnectionState {
    return this.#peerConnection.state();
  }

  get iceConnectionState(): globalThis.RTCIceConnectionState {
    let state = this.#peerConnection.iceState();
    // libdatachannel uses 'completed' instead of 'connected'
    // see /webrtc/getstats.html
    if (state === 'completed') state = 'connected';
    return state;
  }

  get iceGatheringState(): globalThis.RTCIceGatheringState {
    return this.#peerConnection.gatheringState();
  }

  get currentLocalDescription(): globalThis.RTCSessionDescription | null {
    if (this.signalingState !== 'stable') return null;
    return this.localDescription;
  }

  get currentRemoteDescription(): globalThis.RTCSessionDescription | null {
    if (this.signalingState !== 'stable') return null;
    return this.remoteDescription;
  }

  get localDescription(): globalThis.RTCSessionDescription | null {
    const desc = this.#peerConnection.localDescription();
    return desc ? new RTCSessionDescription(desc as globalThis.RTCSessionDescriptionInit) : null;
  }

  get pendingLocalDescription(): globalThis.RTCSessionDescription | null {
    const state = this.signalingState;
    if (state !== 'have-local-offer' && state !== 'have-local-pranswer') return null;
    return this.localDescription;
  }

  get pendingRemoteDescription(): globalThis.RTCSessionDescription | null {
    const state = this.signalingState;
    if (state !== 'have-remote-offer' && state !== 'have-remote-pranswer') return null;
    return this.remoteDescription;
  }

  get remoteDescription(): globalThis.RTCSessionDescription | null {
    const desc = this.#peerConnection.remoteDescription();
    return desc ? new RTCSessionDescription(desc as globalThis.RTCSessionDescriptionInit) : null;
  }

  get sctp(): RTCSctpTransport {
    return this.#sctp;
  }

  get signalingState(): globalThis.RTCSignalingState {
    return this.#peerConnection.signalingState();
  }

  async addIceCandidate(candidate?: globalThis.RTCIceCandidateInit | null): Promise<void> {
    if (!candidate?.candidate) {
      return;
    }

    if (candidate.sdpMid == null && candidate.sdpMLineIndex == null) {
      throw new TypeError('sdpMid must be set');
    }

    try {
      const mid = candidate.sdpMid ?? String(candidate.sdpMLineIndex ?? 0);
      this.#peerConnection.addRemoteCandidate(candidate.candidate, mid);
      this.#remoteCandidates.push(
        new RTCIceCandidate({
          candidate: candidate.candidate,
          sdpMid: candidate.sdpMid ?? undefined,
          sdpMLineIndex: candidate.sdpMLineIndex ?? undefined,
        }),
      );
    } catch (error) {
      const message =
        typeof (error as { message?: unknown })?.message === 'string'
          ? (error as { message: string }).message
          : null;
      if (!message) throw new DOMException('Unknown error', 'NotFoundError');

      // Check error Message if contains specific message
      if (message.includes('remote candidate without remote description'))
        throw new DOMException(message, 'InvalidStateError');
      if (message.includes('Invalid candidate format'))
        throw new DOMException(message, 'OperationError');

      throw new DOMException(message, 'NotFoundError');
    }
  }

  addTrack(_track: MediaStreamTrack, ..._streams: MediaStream[]): globalThis.RTCRtpSender {
    throw new DOMException('Not implemented');
  }

  addTransceiver(
    _trackOrKind: MediaStreamTrack | string,
    _init?: globalThis.RTCRtpTransceiverInit,
  ): globalThis.RTCRtpTransceiver {
    throw new DOMException('Not implemented');
  }

  #trackDataChannel(dc: RTCDataChannel): void {
    this.#dataChannels.add(dc);
    if (dc.readyState === 'open') {
      this.#dataChannelsOpened++;
    } else {
      dc.addEventListener(
        'open',
        () => {
          this.#dataChannelsOpened++;
        },
        { once: true },
      );
    }
    dc.addEventListener(
      'close',
      () => {
        this.#dataChannels.delete(dc);
        this.#dataChannelsClosed++;
      },
      { once: true },
    );
  }

  close(): void {
    for (const dc of [...this.#dataChannels]) {
      dc.close();
    }
    this.#peerConnection.close();
    if (this.#localOfferConsumed && !this.#localOffer.settled) {
      this.#localOffer.reject(new DOMException('PeerConnection closed', 'InvalidStateError'));
    }
    if (this.#localAnswerConsumed && !this.#localAnswer.settled) {
      this.#localAnswer.reject(new DOMException('PeerConnection closed', 'InvalidStateError'));
    }
  }

  createAnswer(
    options?: globalThis.RTCAnswerOptions,
  ): Promise<globalThis.RTCSessionDescriptionInit>;
  createAnswer(
    successCallback: globalThis.RTCSessionDescriptionCallback,
    failureCallback: globalThis.RTCPeerConnectionErrorCallback,
  ): Promise<void>;
  createAnswer(
    successCallbackOrOptions?:
      | globalThis.RTCAnswerOptions
      | globalThis.RTCSessionDescriptionCallback,
    failureCallback?: globalThis.RTCPeerConnectionErrorCallback,
  ): Promise<globalThis.RTCSessionDescriptionInit> | Promise<void> {
    let result: Promise<globalThis.RTCSessionDescriptionInit>;
    const state = this.signalingState;
    if (this.connectionState === 'closed' || state === 'closed') {
      result = Promise.reject(new DOMException('PeerConnection closed', 'InvalidStateError'));
    } else if (state === 'have-local-offer' || state === 'have-remote-pranswer') {
      result = Promise.reject(
        new DOMException(`Cannot create answer in signaling state '${state}'`, 'InvalidStateError'),
      );
    } else if (this.#localAnswerConsumed) {
      if (state !== 'have-remote-offer' && state !== 'have-local-pranswer') {
        result = Promise.reject(
          new DOMException(
            `Cannot create answer in signaling state '${state}'`,
            'InvalidStateError',
          ),
        );
      } else {
        this.#localAnswer = createDeferredDescription();
        try {
          this.#peerConnection.setLocalDescription('answer');
        } catch (err) {
          this.#localAnswer.reject(err);
        }
        result = this.#localAnswer.promise;
      }
    } else {
      if (
        !this.#localAnswer.settled &&
        (state === 'have-remote-offer' || state === 'have-local-pranswer')
      ) {
        try {
          this.#peerConnection.setLocalDescription('answer');
        } catch (err) {
          this.#localAnswer.reject(err);
        }
      }
      this.#localAnswerConsumed = true;
      result = this.#localAnswer.promise;
    }

    if (typeof successCallbackOrOptions === 'function') {
      return result.then(
        (desc) => {
          successCallbackOrOptions(desc);
        },
        (err) => {
          failureCallback?.(err as DOMException);
        },
      );
    }
    return result;
  }

  createDataChannel(label: string, opts: globalThis.RTCDataChannelInit = {}): RTCDataChannel {
    const nativeOpts: DataChannelInitConfig = {
      ...opts,
      // libdatachannel uses "unordered", opposite of RTCDataChannelInit.ordered
      unordered: opts.ordered === false,
    };

    const channel = this.#peerConnection.createDataChannel(label, nativeOpts);
    const dataChannel = new RTCDataChannel(channel, opts);

    // ensure we can close all channels when shutting down
    this.#trackDataChannel(dataChannel);

    if (!this.#announceNegotiation) {
      this.#announceNegotiation = true;
      this.dispatchEvent(new Event('negotiationneeded'));
    }

    return dataChannel;
  }

  createOffer(options?: globalThis.RTCOfferOptions): Promise<globalThis.RTCSessionDescriptionInit>;
  createOffer(
    successCallback: globalThis.RTCSessionDescriptionCallback,
    failureCallback: globalThis.RTCPeerConnectionErrorCallback,
    options?: globalThis.RTCOfferOptions,
  ): Promise<void>;
  createOffer(
    successCallbackOrOptions?:
      | globalThis.RTCOfferOptions
      | globalThis.RTCSessionDescriptionCallback,
    failureCallback?: globalThis.RTCPeerConnectionErrorCallback,
  ): Promise<globalThis.RTCSessionDescriptionInit> | Promise<void> {
    let result: Promise<globalThis.RTCSessionDescriptionInit>;
    const state = this.signalingState;
    if (this.connectionState === 'closed' || state === 'closed') {
      result = Promise.reject(new DOMException('PeerConnection closed', 'InvalidStateError'));
    } else if (state === 'have-local-offer') {
      if (this.#localOfferConsumed) {
        const current = this.localDescription;
        result = current ? Promise.resolve(current) : this.#localOffer.promise;
      } else {
        this.#localOfferConsumed = true;
        result = this.#localOffer.promise;
      }
    } else if (state !== 'stable') {
      result = Promise.reject(
        new DOMException(`Cannot create offer in signaling state '${state}'`, 'InvalidStateError'),
      );
    } else {
      const hasNegotiableContent =
        this.#dataChannels.size > 0 ||
        this.#peerConnection.hasMedia() ||
        this.#peerConnection.localDescription() !== null ||
        this.#peerConnection.remoteDescription() !== null;

      if (this.#localOfferConsumed) {
        this.#localOffer = createDeferredDescription();
        try {
          this.#peerConnection.setLocalDescription('offer');
        } catch (err) {
          this.#localOffer.reject(err);
        }
      } else if (!this.#localOffer.settled && hasNegotiableContent) {
        try {
          this.#peerConnection.setLocalDescription('offer');
        } catch (err) {
          this.#localOffer.reject(err);
        }
      }
      this.#localOfferConsumed = true;
      result = this.#localOffer.promise;
    }

    if (typeof successCallbackOrOptions === 'function') {
      return result.then(
        (desc) => {
          successCallbackOrOptions(desc);
        },
        (err) => {
          failureCallback?.(err as DOMException);
        },
      );
    }
    return result;
  }

  getConfiguration(): globalThis.RTCConfiguration {
    return this.#config;
  }

  getReceivers(): globalThis.RTCRtpReceiver[] {
    throw new DOMException('Not implemented');
  }

  getSenders(): globalThis.RTCRtpSender[] {
    throw new DOMException('Not implemented');
  }

  async getStats(): Promise<globalThis.RTCStatsReport> {
    const report = new Map<string, Record<string, unknown>>();
    const cp = this.#peerConnection?.getSelectedCandidatePair();
    const bytesSent = this.#peerConnection?.bytesSent();
    const bytesReceived = this.#peerConnection?.bytesReceived();
    const rttMs = this.#peerConnection?.rtt();
    const rttSeconds = typeof rttMs === 'number' && rttMs >= 0 ? rttMs / 1000 : undefined;

    if (!cp) {
      return report as globalThis.RTCStatsReport;
    }

    const localIdRs = getRandomString(8);
    const localId = `RTCIceCandidate_${localIdRs}`;
    report.set(localId, {
      id: localId,
      type: 'local-candidate',
      timestamp: Date.now(),
      candidateType: cp.local.type,
      ip: cp.local.address,
      port: cp.local.port,
    });

    const remoteIdRs = getRandomString(8);
    const remoteId = `RTCIceCandidate_${remoteIdRs}`;
    report.set(remoteId, {
      id: remoteId,
      type: 'remote-candidate',
      timestamp: Date.now(),
      candidateType: cp.remote.type,
      ip: cp.remote.address,
      port: cp.remote.port,
    });

    const candidateId = `RTCIceCandidatePair_${localIdRs}_${remoteIdRs}`;
    report.set(candidateId, {
      id: candidateId,
      type: 'candidate-pair',
      timestamp: Date.now(),
      localCandidateId: localId,
      remoteCandidateId: remoteId,
      state: 'succeeded',
      nominated: true,
      writable: true,
      bytesSent: bytesSent,
      bytesReceived: bytesReceived,
      ...(rttSeconds !== undefined
        ? { totalRoundTripTime: rttSeconds, currentRoundTripTime: rttSeconds }
        : {}),
    });

    const transportId = 'RTCTransport_0_1';
    report.set(transportId, {
      id: transportId,
      timestamp: Date.now(),
      type: 'transport',
      bytesSent: bytesSent,
      bytesReceived: bytesReceived,
      dtlsState: 'connected',
      selectedCandidatePairId: candidateId,
      selectedCandidatePairChanges: 1,
    });

    report.set('P', {
      id: 'P',
      type: 'peer-connection',
      timestamp: Date.now(),
      dataChannelsOpened: this.#dataChannelsOpened,
      dataChannelsClosed: this.#dataChannelsClosed,
    });

    return report as globalThis.RTCStatsReport;
  }

  getTransceivers(): globalThis.RTCRtpTransceiver[] {
    return [];
  }

  removeTrack(): void {
    throw new DOMException('Not implemented');
  }

  restartIce(): Promise<void> {
    throw new DOMException('Not implemented');
  }

  async setLocalDescription(
    description?: globalThis.RTCLocalSessionDescriptionInit,
  ): Promise<void> {
    if (!description?.type) {
      this.#peerConnection.setLocalDescription();
      return;
    }

    if (description.type === 'rollback') {
      this.#peerConnection.setLocalDescription('rollback');
      this.#localOffer = createDeferredDescription();
      this.#localOfferConsumed = false;
      return;
    }

    if (description.type === 'offer') {
      // Skip redundant call when libdatachannel auto-negotiation already applied the local offer
      if (this.signalingState === 'have-local-offer') {
        return;
      }
      this.#peerConnection.setLocalDescription('offer');
      return;
    }

    if (description.type === 'answer' || description.type === 'pranswer') {
      if (this.signalingState === 'have-remote-offer') {
        this.#peerConnection.setLocalDescription(description.type as DescriptionType);
      }
    }
  }

  async setRemoteDescription(description: globalThis.RTCSessionDescriptionInit): Promise<void> {
    if (description.type === 'rollback') {
      this.#peerConnection.setRemoteDescription(description.sdp ?? '', 'rollback');
      return;
    }

    if (description.sdp == null) {
      throw new DOMException('Remote SDP must be set');
    }

    // Prepare a fresh answer deferred before libdatachannel auto-generates an answer for a renegotiation offer
    if (description.type === 'offer' && this.#localAnswer.settled) {
      this.#localAnswer = createDeferredDescription();
      this.#localAnswerConsumed = false;
    }

    this.#peerConnection.setRemoteDescription(description.sdp, description.type as DescriptionType);
  }
}

function createDeferredDescription(): DeferredSessionDescription {
  const { promise, resolve, reject } = Promise.withResolvers<RTCSessionDescription>();
  const deferred: DeferredSessionDescription = {
    promise,
    settled: false,
    resolve(value) {
      deferred.settled = true;
      resolve(value);
    },
    reject(reason) {
      deferred.settled = true;
      reject(reason);
    },
  };
  return deferred;
}

function getRandomString(length: number): string {
  return Math.random()
    .toString(36)
    .substring(2, 2 + length);
}
