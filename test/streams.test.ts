import assert from 'node:assert/strict';
import { Readable } from 'node:stream';
import { after, describe, test } from 'node:test';
import * as nodeDataChannel from '../src/lib/index.ts';

function waitForGathering(peer: nodeDataChannel.PeerConnection): Promise<void> {
  return new Promise((resolve) => {
    peer.onGatheringStateChange((state) => {
      if (state === 'complete') resolve();
    });
    if (peer.gatheringState() === 'complete') resolve();
  });
}

function createFakeChannel(overrides: Partial<nodeDataChannel.DataChannel> = {}) {
  const handlers: {
    onOpen?: () => void;
    onClosed?: () => void;
    onError?: (err: string) => void;
    onBufferedAmountLow?: () => void;
    onMessage?: (msg: string | Buffer) => void;
  } = {};

  const channel: nodeDataChannel.DataChannel = {
    isOpen: () => true,
    onMessage: (cb) => {
      handlers.onMessage = cb;
    },
    onClosed: (cb) => {
      handlers.onClosed = cb;
    },
    onError: (cb) => {
      handlers.onError = cb;
    },
    onOpen: (cb) => {
      handlers.onOpen = cb;
    },
    onBufferedAmountLow: (cb) => {
      handlers.onBufferedAmountLow = cb;
    },
    bufferedAmount: () => 0,
    maxMessageSize: () => 65536,
    setBufferedAmountLowThreshold: () => {},
    close: () => {},
    sendMessage: () => true,
    sendMessageBinary: () => true,
    getLabel: () => 'test',
    getId: () => 0,
    getProtocol: () => '',
    ...overrides,
  };

  return { channel, handlers };
}

describe('DataChannel streams', () => {
  after(() => {
    nodeDataChannel.cleanup();
  });

  test('can build an echo pipeline', async () => {
    const clientPeer = new nodeDataChannel.PeerConnection('Client', { iceServers: [] });
    const echoPeer = new nodeDataChannel.PeerConnection('Echo', { iceServers: [] });

    const echoStream = new nodeDataChannel.DataChannelStream(
      echoPeer.createDataChannel('echo-channel'),
    );
    echoStream.pipe(echoStream);

    await waitForGathering(echoPeer);

    const echoDesc = echoPeer.localDescription();
    if (!echoDesc) throw new Error('Expected echoPeer localDescription');
    clientPeer.setRemoteDescription(echoDesc.sdp, echoDesc.type);
    await waitForGathering(clientPeer);

    const clientDesc = clientPeer.localDescription();
    if (!clientDesc) throw new Error('Expected clientPeer localDescription');
    echoPeer.setRemoteDescription(clientDesc.sdp, clientDesc.type);

    const clientChannel: nodeDataChannel.DataChannel = await new Promise((resolve) =>
      clientPeer.onDataChannel(resolve),
    );

    const clientResponsePromise = new Promise((resolve) => clientChannel.onMessage(resolve));
    clientChannel.sendMessage('test message');

    assert.equal(await clientResponsePromise, 'test message');

    clientChannel.close();
    clientPeer.close();
    echoPeer.close();
  });

  test('preserves unread buffered messages when channel closes', () => {
    const { channel, handlers } = createFakeChannel();
    const stream = new nodeDataChannel.DataChannelStream(channel);
    handlers.onMessage?.('msg-1');
    handlers.onMessage?.('msg-2');
    handlers.onClosed?.();

    assert.equal(stream.read(), 'msg-1');
    assert.equal(stream.read(), 'msg-2');
    assert.equal(stream.read(), null);
  });

  test('accepts Uint8Array writes, applies write backpressure, and fails writes on close-before-open', async () => {
    const sentBinary: Uint8Array[] = [];
    let buffered = 0;
    const { channel, handlers } = createFakeChannel({
      bufferedAmount: () => buffered,
      sendMessageBinary: (buf) => {
        sentBinary.push(buf);
        buffered = 128 * 1024;
        return false;
      },
    });

    const stream = new nodeDataChannel.DataChannelStream(channel);
    let writeFinished = false;
    stream.write(new Uint8Array([1, 2, 3]), () => {
      writeFinished = true;
    });

    assert.equal(sentBinary.length, 1);
    assert.equal(writeFinished, false);

    buffered = 0;
    handlers.onBufferedAmountLow?.();
    await new Promise((r) => setImmediate(r));
    assert.equal(writeFinished, true);

    const { channel: unopened, handlers: unopenedHandlers } = createFakeChannel({
      isOpen: () => false,
      sendMessage: () => {
        throw new Error('DataChannel not open');
      },
    });
    const unopenedStream = new nodeDataChannel.DataChannelStream(unopened);
    unopenedStream.on('error', () => {});
    const writeErr = await new Promise<Error | null | undefined>((resolve) => {
      unopenedStream.write('queued', (err) => resolve(err));
      unopenedHandlers.onClosed?.();
    });
    assert.ok(writeErr instanceof Error);
    assert.equal(unopenedStream.writableCorked, 0);
  });

  test('caches label/id/protocol after close and handles immediate open during onOpen registration', () => {
    let opened = false;
    const { channel } = createFakeChannel({
      isOpen: () => opened,
      getLabel: () => {
        if (!opened) return 'cached-label';
        throw new Error('destroyed');
      },
      getId: () => 7,
      getProtocol: () => 'proto-x',
      onOpen: () => {
        opened = true;
      },
    });

    const stream = new nodeDataChannel.DataChannelStream(channel);
    assert.equal(stream.writableCorked, 0);
    assert.equal(stream.label, 'cached-label');
    assert.equal(stream.id, 7);
    assert.equal(stream.protocol, 'proto-x');
  });

  test('transfers bulk binary chunks over a real DataChannel with SCTP buffering and backpressure', async () => {
    const p1 = new nodeDataChannel.PeerConnection('BulkSender', { iceServers: [] });
    const p2 = new nodeDataChannel.PeerConnection('BulkReceiver', { iceServers: [] });
    p1.onLocalDescription((s, t) => p2.setRemoteDescription(s, t));
    p2.onLocalDescription((s, t) => p1.setRemoteDescription(s, t));
    p1.onLocalCandidate((c, m) => p2.addRemoteCandidate(c, m));
    p2.onLocalCandidate((c, m) => p1.addRemoteCandidate(c, m));

    const chunkCount = 40;
    const chunkSize = 16 * 1024;
    const totalExpected = chunkCount * chunkSize;
    let receivedBytes = 0;
    const allReceived = Promise.withResolvers<void>();
    const rxChannels: nodeDataChannel.DataChannel[] = [];

    p2.onDataChannel((dc) => {
      rxChannels.push(dc);
      dc.onMessage((msg) => {
        if (Buffer.isBuffer(msg)) {
          receivedBytes += msg.byteLength;
          if (receivedBytes >= totalExpected) allReceived.resolve();
        }
      });
    });

    const txChannel = p1.createDataChannel('bulk');
    const txStream = new nodeDataChannel.DataChannelStream(txChannel);
    const errors: Error[] = [];
    txStream.on('error', (err) => errors.push(err));

    const payload = Buffer.alloc(chunkSize, 0x5a);
    for (let i = 0; i < chunkCount; i++) {
      if (!txStream.write(payload)) {
        await new Promise<void>((resolve) => txStream.once('drain', resolve));
      }
    }

    await allReceived.promise;
    assert.deepEqual(errors, []);
    assert.equal(receivedBytes, totalExpected);

    txStream.destroy();
    for (const ch of rxChannels) ch.close();
    p1.close();
    p2.close();
  });

  test('supports piping and calling end() before the DataChannel opens', async () => {
    const p1 = new nodeDataChannel.PeerConnection('EarlyEnd1', { iceServers: [] });
    const p2 = new nodeDataChannel.PeerConnection('EarlyEnd2', { iceServers: [] });
    p1.onLocalDescription((s, t) => p2.setRemoteDescription(s, t));
    p2.onLocalDescription((s, t) => p1.setRemoteDescription(s, t));
    p1.onLocalCandidate((c, m) => p2.addRemoteCandidate(c, m));
    p2.onLocalCandidate((c, m) => p1.addRemoteCandidate(c, m));

    const received: string[] = [];
    const rxClosed = Promise.withResolvers<void>();
    p2.onDataChannel((dc) => {
      dc.onMessage((msg) => received.push(String(msg)));
      dc.onClosed(() => rxClosed.resolve());
    });

    const txStream = new nodeDataChannel.DataChannelStream(p1.createDataChannel('early-end'));
    const errors: Error[] = [];
    txStream.on('error', (err) => errors.push(err));

    Readable.from(['first', 'second', 'third']).pipe(txStream);

    await rxClosed.promise;
    assert.deepEqual(errors, []);
    assert.deepEqual(received, ['first', 'second', 'third']);
    p1.close();
    p2.close();
  });
});
