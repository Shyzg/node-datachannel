import RTCDataChannel from './RTCDataChannel.ts';
import RTCError from './RTCError.ts';

export class RTCPeerConnectionIceEvent
  extends Event
  implements globalThis.RTCPeerConnectionIceEvent
{
  #candidate: globalThis.RTCIceCandidate | null;
  #url: string | null;

  constructor(
    type: string,
    eventInitDict?: globalThis.RTCPeerConnectionIceEventInit & { url?: string | null },
  ) {
    super(type || 'icecandidate', eventInitDict);
    this.#candidate = eventInitDict?.candidate ?? null;
    this.#url = eventInitDict?.url ?? null;
  }

  get candidate(): globalThis.RTCIceCandidate | null {
    return this.#candidate;
  }

  get url(): string | null {
    return this.#url;
  }
}

export class RTCDataChannelEvent extends Event implements globalThis.RTCDataChannelEvent {
  #channel: RTCDataChannel;

  constructor(type = 'datachannel', eventInitDict?: globalThis.RTCDataChannelEventInit) {
    super(type);

    if (eventInitDict === undefined)
      throw new TypeError(
        "Failed to construct 'RTCDataChannelEvent': 2 arguments required, but only 1 present.",
      );
    if (typeof eventInitDict !== 'object' || eventInitDict === null)
      throw new TypeError(
        "Failed to construct 'RTCDataChannelEvent': The provided value is not of type 'RTCDataChannelEventInit'.",
      );
    if (!eventInitDict.channel)
      throw new TypeError(
        "Failed to construct 'RTCDataChannelEvent': Failed to read the 'channel' property from 'RTCDataChannelEventInit': Required member is undefined.",
      );
    if (!(eventInitDict.channel instanceof RTCDataChannel))
      throw new TypeError(
        "Failed to construct 'RTCDataChannelEvent': Failed to read the 'channel' property from 'RTCDataChannelEventInit': Failed to convert value to 'RTCDataChannel'.",
      );

    this.#channel = eventInitDict.channel;
  }

  get channel(): RTCDataChannel {
    return this.#channel;
  }
}

export class RTCErrorEvent extends Event implements globalThis.RTCErrorEvent {
  #error: RTCError;
  constructor(type: string, init?: globalThis.RTCErrorEventInit) {
    if (init === undefined)
      throw new TypeError(
        "Failed to construct 'RTCErrorEvent': 2 arguments required, but only 1 present.",
      );
    if (typeof init !== 'object' || init === null)
      throw new TypeError(
        "Failed to construct 'RTCErrorEvent': The provided value is not of type 'RTCErrorEventInit'.",
      );
    if (!init.error)
      throw new TypeError(
        "Failed to construct 'RTCErrorEvent': Failed to read the 'error' property from 'RTCErrorEventInit': Required member is undefined.",
      );
    if (!(init.error instanceof RTCError))
      throw new TypeError(
        "Failed to construct 'RTCErrorEvent': Failed to read the 'error' property from 'RTCErrorEventInit': Failed to convert value to 'RTCError'.",
      );
    super(type || 'error');
    this.#error = init.error;
  }

  get error(): RTCError {
    return this.#error;
  }
}
