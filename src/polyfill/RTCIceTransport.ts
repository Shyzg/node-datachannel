import RTCIceCandidate from './RTCIceCandidate.ts';
import type RTCPeerConnection from './RTCPeerConnection.ts';

export default class RTCIceTransport extends EventTarget implements globalThis.RTCIceTransport {
  #pc: RTCPeerConnection;

  ongatheringstatechange: globalThis.RTCIceTransport['ongatheringstatechange'] = null;
  onselectedcandidatepairchange: globalThis.RTCIceTransport['onselectedcandidatepairchange'] = null;
  onstatechange: globalThis.RTCIceTransport['onstatechange'] = null;

  constructor(init: { pc: RTCPeerConnection }) {
    super();
    this.#pc = init.pc;

    this.#pc.addEventListener('icegatheringstatechange', () => {
      const e = new Event('gatheringstatechange');
      this.dispatchEvent(e);
      this.ongatheringstatechange?.(e);
    });
    this.#pc.addEventListener('iceconnectionstatechange', () => {
      const e = new Event('statechange');
      this.dispatchEvent(e);
      this.onstatechange?.(e);
    });
  }

  get component(): globalThis.RTCIceComponent | null {
    const cp = this.getSelectedCandidatePair();
    if (!cp?.local) return null;
    return cp.local.component;
  }

  get gatheringState(): globalThis.RTCIceGatheringState {
    return this.#pc ? this.#pc.iceGatheringState : 'new';
  }

  get role(): globalThis.RTCIceRole {
    return this.#pc.localDescription?.type === 'offer' ? 'controlling' : 'controlled';
  }

  get state(): globalThis.RTCIceTransportState {
    return this.#pc ? this.#pc.iceConnectionState : 'new';
  }

  getLocalCandidates(): globalThis.RTCIceCandidate[] {
    return this.#pc?.ext_localCandidates ?? [];
  }

  getLocalParameters(): RTCIceParameters | null {
    const cp = this.#pc?.selectedCandidatePair();
    if (!cp?.local) return null;
    return new RTCIceParameters(
      new RTCIceCandidate({
        candidate: cp.local.candidate,
        sdpMLineIndex: 0,
      }),
    );
  }

  getRemoteCandidates(): globalThis.RTCIceCandidate[] {
    return this.#pc?.ext_remoteCandidates ?? [];
  }

  getRemoteParameters(): RTCIceParameters | null {
    const cp = this.#pc?.selectedCandidatePair();
    if (!cp?.remote) return null;
    return new RTCIceParameters(
      new RTCIceCandidate({
        candidate: cp.remote.candidate,
        sdpMLineIndex: 0,
      }),
    );
  }

  getSelectedCandidatePair(): globalThis.RTCIceCandidatePair | null {
    const cp = this.#pc?.selectedCandidatePair();
    if (!cp) return null;
    return {
      local: new RTCIceCandidate({
        candidate: cp.local.candidate,
        sdpMid: cp.local.mid,
      }),
      remote: new RTCIceCandidate({
        candidate: cp.remote.candidate,
        sdpMid: cp.remote.mid,
      }),
    };
  }
}

export class RTCIceParameters {
  usernameFragment = '';
  password = '';
  constructor({
    usernameFragment,
    password = '',
  }: {
    usernameFragment?: string | null;
    password?: string;
  }) {
    this.usernameFragment = usernameFragment ?? '';
    this.password = password;
  }
}
