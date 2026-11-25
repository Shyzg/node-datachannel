import * as stream from 'node:stream';
import type { DataChannel } from './index.ts';

const WRITE_BUFFER_HIGH_WATER_BYTES = 64 * 1024;

/**
 * Turns a node-datachannel DataChannel into a real Node.js stream, complete with buffering,
 * backpressure (up to a point - if the buffer fills up, messages are dropped), and
 * support for piping data elsewhere.
 *
 * Read & written data may be either UTF-8 strings or Buffers/Uint8Arrays - this difference
 * exists at the protocol level, and is preserved here throughout.
 */
export default class DataChannelStream extends stream.Duplex {
  private _rawChannel: DataChannel;
  private _readActive: boolean;
  private _corkedForOpen: boolean;
  private _pendingWriteCallback: ((error?: Error | null) => void) | null = null;
  private _label: string;
  private _id: number;
  private _protocol: string;

  constructor(rawChannel: DataChannel, streamOptions?: Omit<stream.DuplexOptions, 'objectMode'>) {
    super({
      allowHalfOpen: false, // Default to autoclose on end().
      ...streamOptions,
      objectMode: true, // Preserve the string/buffer distinction (WebRTC treats them differently)
    });

    this._rawChannel = rawChannel;
    this._readActive = true;
    this._corkedForOpen = false;
    this._label = rawChannel.getLabel();
    this._id = rawChannel.getId();
    this._protocol = rawChannel.getProtocol();

    rawChannel.onMessage((msg) => {
      if (!this._readActive) return; // If the buffer is full, drop messages.

      // If the push is rejected, we pause reading until the next call to _read().
      this._readActive = this.push(msg);
    });

    // End readable side without destroying so unread buffered messages can still be drained
    rawChannel.onClosed(() => {
      if (!this.destroyed) {
        this.push(null);
      }

      // If closed while corked waiting to open, uncork so queued writes fail instead of hanging
      if (this._corkedForOpen) {
        this._corkedForOpen = false;
        this.uncork();
      }

      if (this._pendingWriteCallback) {
        const cb = this._pendingWriteCallback;
        this._pendingWriteCallback = null;
        cb(new Error('DataChannel closed'));
      }
    });

    rawChannel.onError((errMsg: string) => {
      this.destroy(new Error(`DataChannel error: ${errMsg}`));
    });

    if (
      typeof rawChannel.setBufferedAmountLowThreshold === 'function' &&
      typeof rawChannel.onBufferedAmountLow === 'function'
    ) {
      rawChannel.setBufferedAmountLowThreshold(WRITE_BUFFER_HIGH_WATER_BYTES);
      rawChannel.onBufferedAmountLow(() => {
        const cb = this._pendingWriteCallback;
        if (cb) {
          this._pendingWriteCallback = null;
          cb(null);
        }
      });
    }

    // Buffer all writes until the DataChannel opens.
    // Re-check isOpen() after registering onOpen() to avoid missing an open transition on the C++ thread.
    if (!rawChannel.isOpen()) {
      this._corkedForOpen = true;
      this.cork();
      rawChannel.onOpen(() => {
        const id = rawChannel.getId();
        if (id >= 0) this._id = id;
        if (this._corkedForOpen) {
          this._corkedForOpen = false;
          this.uncork();
        }
      });
      if (rawChannel.isOpen() && this._corkedForOpen) {
        const id = rawChannel.getId();
        if (id >= 0) this._id = id;
        this._corkedForOpen = false;
        this.uncork();
      }
    }
  }

  _read(): void {
    // Stop dropping messages, if the buffer filling up meant we were doing so before.
    this._readActive = true;
  }

  _write(
    chunk: unknown,
    _encoding: BufferEncoding,
    callback: (error?: Error | null) => void,
  ): void {
    let sentOk: boolean;

    const prevBuffered =
      typeof this._rawChannel.bufferedAmount === 'function' ? this._rawChannel.bufferedAmount() : 0;

    try {
      if (chunk instanceof Uint8Array) {
        sentOk = this._rawChannel.sendMessageBinary(chunk);
      } else if (typeof chunk === 'string') {
        sentOk = this._rawChannel.sendMessage(chunk);
      } else {
        const typeName =
          (chunk as { constructor?: { name?: string } })?.constructor?.name || typeof chunk;
        throw new Error(`Cannot write ${typeName} to DataChannel stream`);
      }
    } catch (err) {
      callback(err instanceof Error ? err : new Error(String(err)));
      return;
    }

    const currentBuffered =
      typeof this._rawChannel.bufferedAmount === 'function' ? this._rawChannel.bufferedAmount() : 0;

    // libdatachannel returns false when the message is queued in its send buffer (normal under load).
    // Only fail if the channel is no longer open and did not buffer the message.
    if (!sentOk && currentBuffered <= prevBuffered && !this._rawChannel.isOpen()) {
      callback(new Error('Failed to write to DataChannel'));
      return;
    }

    if (
      typeof this._rawChannel.onBufferedAmountLow === 'function' &&
      currentBuffered > WRITE_BUFFER_HIGH_WATER_BYTES
    ) {
      this._pendingWriteCallback = callback;
    } else {
      callback(null);
    }
  }

  _final(callback: (error?: Error | null) => void): void {
    if (!this.allowHalfOpen) this.destroy();
    callback(null);
  }

  _destroy(maybeErr: Error | null, callback: (error?: Error | null) => void): void {
    if (this._corkedForOpen) {
      this._corkedForOpen = false;
      this.uncork();
    }
    if (this._pendingWriteCallback) {
      const cb = this._pendingWriteCallback;
      this._pendingWriteCallback = null;
      cb(maybeErr ?? new Error('DataChannel stream destroyed'));
    }
    // When the stream is destroyed, we close the DataChannel.
    this._rawChannel.close();
    callback(maybeErr);
  }

  get label(): string {
    return this._label;
  }

  get id(): number {
    return this._id;
  }

  get protocol(): string {
    return this._protocol;
  }
}
