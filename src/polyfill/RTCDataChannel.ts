import type { DataChannel } from '../lib/index.ts';
import { RTCErrorEvent } from './Events.ts';
import RTCError from './RTCError.ts';

export default class RTCDataChannel extends EventTarget implements globalThis.RTCDataChannel {
  #dataChannel: DataChannel;
  #readyState: globalThis.RTCDataChannelState;
  #bufferedAmountLowThreshold: number;
  #binaryType: BinaryType;
  #maxPacketLifeTime: number | null;
  #maxRetransmits: number | null;
  #negotiated: boolean;
  #ordered: boolean;
  #id: number | null;
  #label: string;
  #protocol: string;

  #closeRequested = false;
  #sendQueue: Promise<void> | null = null;

  // events
  onbufferedamountlow: globalThis.RTCDataChannel['onbufferedamountlow'] = null;
  onclose: globalThis.RTCDataChannel['onclose'] = null;
  onclosing: globalThis.RTCDataChannel['onclosing'] = null;
  onerror: globalThis.RTCDataChannel['onerror'] = null;
  onmessage: globalThis.RTCDataChannel['onmessage'] = null;
  onopen: globalThis.RTCDataChannel['onopen'] = null;

  constructor(dataChannel: DataChannel, opts: globalThis.RTCDataChannelInit = {}) {
    super();

    this.#dataChannel = dataChannel;
    this.#binaryType = 'arraybuffer';
    this.#readyState = this.#dataChannel.isOpen() ? 'open' : 'connecting';
    this.#bufferedAmountLowThreshold = 0;
    this.#maxPacketLifeTime = opts.maxPacketLifeTime ?? null;
    this.#maxRetransmits = opts.maxRetransmits ?? null;
    this.#negotiated = opts.negotiated ?? false;
    this.#ordered = opts.ordered ?? true;
    const initialId = this.#dataChannel.getId();
    this.#id = initialId >= 0 ? initialId : null;
    this.#label = this.#dataChannel.getLabel();
    this.#protocol = this.#dataChannel.getProtocol();

    const handleOpen = () => {
      if (this.#readyState !== 'connecting') return;
      const openId = this.#dataChannel.getId();
      if (openId >= 0) this.#id = openId;
      this.#readyState = 'open';
      this.dispatchEvent(new Event('open', {}));
    };

    // forward dataChannel events
    // Re-check isOpen() after registering onOpen() to avoid missing a C++ thread open transition.
    this.#dataChannel.onOpen(handleOpen);
    if (this.#readyState === 'connecting' && this.#dataChannel.isOpen()) {
      handleOpen();
    }

    this.#dataChannel.onClosed(() => {
      // Simulate closing event
      if (!this.#closeRequested) {
        this.#readyState = 'closing';
        this.dispatchEvent(new Event('closing'));
      }

      setImmediate(() => {
        this.#readyState = 'closed';
        this.dispatchEvent(new Event('close'));
      });
    });

    this.#dataChannel.onError((msg) => {
      this.dispatchEvent(
        new RTCErrorEvent('error', {
          error: new RTCError(
            {
              errorDetail: 'data-channel-failure',
            },
            msg,
          ),
        }),
      );
    });

    this.#dataChannel.onBufferedAmountLow(() => {
      this.dispatchEvent(new Event('bufferedamountlow'));
    });

    this.#dataChannel.onMessage((message) => {
      if (typeof message === 'string') {
        this.dispatchEvent(new MessageEvent('message', { data: message }));
        return;
      }

      let data: Blob | ArrayBuffer;

      if (
        message.buffer instanceof ArrayBuffer &&
        message.byteOffset === 0 &&
        message.byteLength === message.buffer.byteLength
      ) {
        data = message.buffer;
      } else {
        // message is view on underlying buffer, must create new
        // ArrayBuffer that only contains message data
        data = new ArrayBuffer(message.byteLength);
        new Uint8Array(data, 0, message.byteLength).set(message);
      }

      if (this.#binaryType === 'blob') {
        data = new Blob([data]);
      }

      this.dispatchEvent(new MessageEvent('message', { data }));
    });

    // forward events to properties
    this.addEventListener('message', (e) => {
      if (this.onmessage) this.onmessage(e as MessageEvent);
    });
    this.addEventListener('bufferedamountlow', (e) => {
      if (this.onbufferedamountlow) this.onbufferedamountlow(e);
    });
    this.addEventListener('error', (e) => {
      if (this.onerror) this.onerror(e as RTCErrorEvent);
    });
    this.addEventListener('close', (e) => {
      if (this.onclose) this.onclose(e);
    });
    this.addEventListener('closing', (e) => {
      if (this.onclosing) this.onclosing(e);
    });
    this.addEventListener('open', (e) => {
      if (this.onopen) this.onopen(e);
    });
  }

  set binaryType(type) {
    if (type !== 'blob' && type !== 'arraybuffer') {
      throw new DOMException(
        `Failed to set the 'binaryType' property on 'RTCDataChannel': Unknown binary type : ${type}`,
        'TypeMismatchError',
      );
    }
    this.#binaryType = type;
  }

  get binaryType(): BinaryType {
    return this.#binaryType;
  }

  get bufferedAmount(): number {
    return this.#dataChannel.bufferedAmount();
  }

  get bufferedAmountLowThreshold(): number {
    return this.#bufferedAmountLowThreshold;
  }

  set bufferedAmountLowThreshold(value) {
    const number = Math.max(0, Number(value) || 0);
    this.#bufferedAmountLowThreshold = number;
    this.#dataChannel.setBufferedAmountLowThreshold(number);
  }

  get id(): number | null {
    return this.#id;
  }

  get label(): string {
    return this.#label;
  }

  get maxPacketLifeTime(): number | null {
    return this.#maxPacketLifeTime;
  }

  get maxRetransmits(): number | null {
    return this.#maxRetransmits;
  }

  get negotiated(): boolean {
    return this.#negotiated;
  }

  get ordered(): boolean {
    return this.#ordered;
  }

  get protocol(): string {
    return this.#protocol;
  }

  get readyState(): globalThis.RTCDataChannelState {
    return this.#readyState;
  }

  #sendBinaryNow(buf: Uint8Array): void {
    if (process?.versions?.bun && !Buffer.isBuffer(buf)) {
      this.#dataChannel.sendMessageBinary(Buffer.from(buf.buffer, buf.byteOffset, buf.byteLength));
    } else {
      this.#dataChannel.sendMessageBinary(buf);
    }
  }

  #dispatchSendError(err: unknown): void {
    const message =
      typeof (err as { message?: unknown })?.message === 'string'
        ? (err as { message: string }).message
        : String(err);
    this.dispatchEvent(
      new RTCErrorEvent('error', {
        error: new RTCError({ errorDetail: 'data-channel-failure' }, message),
      }),
    );
  }

  #enqueueAsyncSend(task: () => Promise<void> | void): void {
    const prev = this.#sendQueue ?? Promise.resolve();
    const next = prev
      .then(task)
      .catch((err) => {
        this.#dispatchSendError(err);
      })
      .finally(() => {
        if (this.#sendQueue === next) {
          this.#sendQueue = null;
        }
      });
    this.#sendQueue = next;
  }

  send(data: string | Blob | ArrayBuffer | ArrayBufferView): void {
    if (this.#readyState !== 'open') {
      throw new DOMException(
        "Failed to execute 'send' on 'RTCDataChannel': RTCDataChannel.readyState is not 'open'",
        'InvalidStateError',
      );
    }

    if (data instanceof Blob) {
      const bytesPromise = data.arrayBuffer();
      this.#enqueueAsyncSend(async () => {
        const ab = await bytesPromise;
        if (this.#readyState !== 'open') return;
        this.#sendBinaryNow(new Uint8Array(ab));
      });
      return;
    }

    // If a Blob send is currently in flight, queue this send behind it to preserve ordering
    if (this.#sendQueue) {
      if (typeof data === 'string') {
        this.#enqueueAsyncSend(() => {
          if (this.#readyState !== 'open') return;
          this.#dataChannel.sendMessage(data);
        });
      } else {
        const view = ArrayBuffer.isView(data)
          ? new Uint8Array(data.buffer, data.byteOffset, data.byteLength)
          : new Uint8Array(data);
        const copy = new Uint8Array(view.byteLength);
        copy.set(view);
        this.#enqueueAsyncSend(() => {
          if (this.#readyState !== 'open') return;
          this.#sendBinaryNow(copy);
        });
      }
      return;
    }

    if (typeof data === 'string') {
      this.#dataChannel.sendMessage(data);
    } else if (data instanceof Uint8Array) {
      this.#sendBinaryNow(data);
    } else if (ArrayBuffer.isView(data)) {
      this.#sendBinaryNow(new Uint8Array(data.buffer, data.byteOffset, data.byteLength));
    } else {
      this.#sendBinaryNow(new Uint8Array(data));
    }
  }

  close(): void {
    if (this.#readyState === 'closed' || this.#readyState === 'closing') return;
    this.#closeRequested = true;
    this.#readyState = 'closing';
    setImmediate(() => {
      this.#dataChannel.close();
    });
  }
}
