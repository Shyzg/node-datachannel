import RTCDtlsTransport from './RTCDtlsTransport.ts';
import type RTCPeerConnection from './RTCPeerConnection.ts';

export default class RTCSctpTransport extends EventTarget implements globalThis.RTCSctpTransport {
  #pc: RTCPeerConnection;
  #transport: RTCDtlsTransport;

  onstatechange: globalThis.RTCSctpTransport['onstatechange'] = null;

  constructor(initial: { pc: RTCPeerConnection }) {
    super();
    this.#pc = initial.pc;

    this.#transport = new RTCDtlsTransport({
      pc: initial.pc,
    });

    this.#pc.addEventListener('connectionstatechange', () => {
      const e = new Event('statechange');
      this.dispatchEvent(e);
      this.onstatechange?.(e);
    });
  }

  get maxChannels(): number | null {
    if (this.state !== 'connected') return null;
    return this.#pc.ext_maxDataChannelId;
  }

  get maxMessageSize(): number {
    if (this.state !== 'connected') return 0;
    return this.#pc?.ext_maxMessageSize ?? 65536;
  }

  get state(): globalThis.RTCSctpTransportState {
    // reduce state from new, connecting, connected, disconnected, failed, closed, unknown
    // to RTCSctpTransport states connecting, connected, closed
    let state = this.#pc.connectionState;
    if (state === 'new' || state === 'connecting') {
      state = 'connecting';
    } else if (state === 'disconnected' || state === 'failed' || state === 'closed') {
      state = 'closed';
    }
    return state;
  }

  get transport(): RTCDtlsTransport {
    return this.#transport;
  }
}
