// https://developer.mozilla.org/docs/Web/API/RTCIceCandidate
//
// Example: candidate:123456 1 UDP 123456 192.168.1.1 12345 typ host raddr=10.0.0.1 rport=54321 generation 0

export default class RTCIceCandidate implements globalThis.RTCIceCandidate {
  #address: string | null = null;
  #candidate: string;
  #component: globalThis.RTCIceComponent | null = null;
  #foundation: string | null = null;
  #port: number | null = null;
  #priority: number | null = null;
  #protocol: globalThis.RTCIceProtocol | null = null;
  #relatedAddress: string | null = null;
  #relatedPort: number | null = null;
  #sdpMLineIndex: number | null;
  #sdpMid: string | null;
  #tcpType: globalThis.RTCIceTcpCandidateType | null = null;
  #type: globalThis.RTCIceCandidateType | null = null;
  #usernameFragment: string | null;

  constructor({
    candidate,
    sdpMLineIndex,
    sdpMid,
    usernameFragment,
  }: globalThis.RTCIceCandidateInit) {
    if (sdpMLineIndex == null && sdpMid == null)
      throw new TypeError('At least one of sdpMLineIndex or sdpMid must be specified');

    this.#candidate = candidate ?? '';
    this.#sdpMLineIndex = sdpMLineIndex ?? null;
    this.#sdpMid = sdpMid ?? null;
    this.#usernameFragment = usernameFragment ?? null;

    const trimmed = candidate?.trim();
    if (trimmed) {
      const fields = trimmed.split(/\s+/);
      this.#foundation = fields[0]?.replace(/^(a=)?candidate:/, '') ?? null;
      this.#component = fields[1] === '1' ? 'rtp' : fields[1] === '2' ? 'rtcp' : null;
      this.#protocol = (fields[2]?.toLowerCase() as globalThis.RTCIceProtocol) ?? null;
      const priority = parseInt(fields[3], 10);
      this.#priority = Number.isNaN(priority) ? null : priority;
      this.#address = fields[4] ?? null;
      const port = parseInt(fields[5], 10);
      this.#port = Number.isNaN(port) ? null : port;
      this.#type = (fields[7] as globalThis.RTCIceCandidateType) ?? null;
      this.#tcpType = null;
      this.#relatedAddress = null;
      this.#relatedPort = null;

      // Parse optional candidate attributes (raddr, rport, tcptype, ufrag)
      for (let i = 8; i + 1 < fields.length; i += 2) {
        const field = fields[i];
        const value = fields[i + 1];
        if (field === 'raddr') {
          this.#relatedAddress = value;
        } else if (field === 'rport') {
          const rport = parseInt(value, 10);
          this.#relatedPort = Number.isNaN(rport) ? null : rport;
        } else if (field === 'ufrag' && this.#usernameFragment === null) {
          this.#usernameFragment = value;
        } else if (this.#protocol === 'tcp' && field === 'tcptype') {
          this.#tcpType = value as globalThis.RTCIceTcpCandidateType;
        }
      }
    }
  }

  get address(): string | null {
    return this.#address ?? null;
  }

  get candidate(): string {
    return this.#candidate;
  }

  get component(): globalThis.RTCIceComponent | null {
    return this.#component;
  }

  get foundation(): string | null {
    return this.#foundation ?? null;
  }

  get port(): number | null {
    return this.#port ?? null;
  }

  get priority(): number | null {
    return this.#priority ?? null;
  }

  get protocol(): globalThis.RTCIceProtocol | null {
    return this.#protocol ?? null;
  }

  get relatedAddress(): string | null {
    return this.#relatedAddress;
  }

  get relatedPort(): number | null {
    return this.#relatedPort ?? null;
  }

  get sdpMLineIndex(): number | null {
    return this.#sdpMLineIndex;
  }

  get sdpMid(): string | null {
    return this.#sdpMid;
  }

  get tcpType(): globalThis.RTCIceTcpCandidateType | null {
    return this.#tcpType;
  }

  get type(): globalThis.RTCIceCandidateType | null {
    return this.#type ?? null;
  }

  get usernameFragment(): string | null {
    return this.#usernameFragment;
  }

  toJSON(): globalThis.RTCIceCandidateInit {
    return {
      candidate: this.#candidate,
      sdpMLineIndex: this.#sdpMLineIndex,
      sdpMid: this.#sdpMid,
      usernameFragment: this.#usernameFragment,
    };
  }
}
