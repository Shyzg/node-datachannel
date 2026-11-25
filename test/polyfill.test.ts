import assert from 'node:assert/strict';
import { after, describe, mock, test } from 'node:test';
import { type DataChannel, PeerConnection, cleanup, initLogger } from '../src/lib/index.ts';
import {
  RTCDataChannel as PolyfillRTCDataChannel,
  RTCDataChannelEvent,
  RTCError,
  RTCErrorEvent,
  RTCIceCandidate,
  RTCPeerConnection,
  RTCPeerConnectionIceEvent,
} from '../src/polyfill/index.ts';
import { eventPromise } from './fixtures/event-promise.ts';

function makeStubRawChannel(overrides: Partial<DataChannel> = {}): DataChannel {
  return {
    isOpen: () => true,
    getId: () => 1,
    getLabel: () => 'stub',
    getProtocol: () => '',
    bufferedAmount: () => 0,
    maxMessageSize: () => 65536,
    setBufferedAmountLowThreshold: () => {},
    sendMessage: () => true,
    sendMessageBinary: () => true,
    onOpen: () => {},
    onClosed: () => {},
    onError: () => {},
    onBufferedAmountLow: () => {},
    onMessage: () => {},
    close: () => {},
    ...overrides,
  };
}

describe('polyfill', { timeout: 30_000 }, () => {
  after(() => {
    cleanup();
  });

  test('generateCertificate should throw', async () => {
    await assert.rejects(
      () => RTCPeerConnection.generateCertificate(),
      (err: unknown) => err instanceof DOMException && err.message === 'Not implemented',
    );
  });

  test('can assign polyfill to global type', () => {
    const pc: globalThis.RTCPeerConnection = new RTCPeerConnection();
    assert.ok(pc);
    pc.close();
  });

  test('P2P Test', async () => {
    const p1ConnectionStateMock = mock.fn();
    const p1IceConnectionStateMock = mock.fn();
    const p1IceGatheringStateMock = mock.fn();
    const p1IceCandidateMock = mock.fn();
    const p1SDPMock = mock.fn();
    const p1DCMock = mock.fn();
    const p1MessageMock = mock.fn<(data: Blob | ArrayBuffer | string) => void>();
    const p2ConnectionStateMock = mock.fn();
    const p2IceConnectionStateMock = mock.fn();
    const p2IceGatheringStateMock = mock.fn();
    const p2IceCandidateMock = mock.fn();
    const p2SDPMock = mock.fn();
    const p2DCMock = mock.fn();
    const p2MessageMock = mock.fn<(data: Blob | ArrayBuffer | string) => void>();

    const peer1 = new RTCPeerConnection({
      peerIdentity: 'peer1',
      iceServers: [{ urls: ['stun:stun.l.google.com:19302'] }],
    });

    const peer2 = new RTCPeerConnection({
      peerIdentity: 'peer2',
      iceServers: [{ urls: ['stun:stun.l.google.com:19302'] }],
    });

    let dc1: RTCDataChannel | null = null;
    let dc2: RTCDataChannel | null = null;

    function createBinaryTestData(
      binaryData = new ArrayBuffer(17),
      offset = 0,
    ): Uint8Array<ArrayBuffer> {
      const dv = new DataView(binaryData, offset);
      dv.setInt8(0, 123);
      dv.setFloat32(1, 123.456);
      dv.setUint32(5, 987654321);
      dv.setFloat64(9, 789.012);
      return new Uint8Array(binaryData, offset, 17);
    }

    function analyzeBinaryTestData(binaryData: ArrayBufferLike): boolean {
      const dv = new DataView(binaryData);
      return (
        dv.getInt8(0) === 123 &&
        dv.getFloat32(1) === Math.fround(123.456) &&
        dv.getUint32(5) === 987654321 &&
        dv.getFloat64(9) === 789.012
      );
    }

    const testMessages: Array<{
      binaryType: BinaryType;
      data: string | Blob | Uint8Array<ArrayBuffer>;
    }> = [
      { binaryType: 'arraybuffer', data: 'Hello' },
      { binaryType: 'arraybuffer', data: createBinaryTestData() },
      { binaryType: 'arraybuffer', data: createBinaryTestData(new ArrayBuffer(100)) },
      { binaryType: 'arraybuffer', data: createBinaryTestData(new ArrayBuffer(100), 50) },
      { binaryType: 'arraybuffer', data: createBinaryTestData(new ArrayBuffer(100), 50) },
      { binaryType: 'blob', data: 'Hello' },
      { binaryType: 'blob', data: new Blob([createBinaryTestData()]) },
    ];

    const testMessageCount = testMessages.length;
    let currentIndex = -1;

    async function analyzeData(idx: number, data: Blob | ArrayBuffer | string): Promise<boolean> {
      switch (idx) {
        case 0:
          return data === 'Hello';
        case 1:
        case 2:
        case 3:
        case 4:
          return analyzeBinaryTestData(data as ArrayBuffer);
        case 5:
          return data === 'Hello';
        case 6:
          return analyzeBinaryTestData(await (data as Blob).arrayBuffer());
      }
      return false;
    }

    async function finalizeTest(): Promise<void> {
      peer1.close();
      peer2.close();

      assert.ok(p1ConnectionStateMock.mock.calls.length >= 1);
      assert.ok(p2ConnectionStateMock.mock.calls.length >= 1);
      assert.ok(p1IceConnectionStateMock.mock.calls.length >= 1);
      assert.ok(p2IceConnectionStateMock.mock.calls.length >= 1);
      assert.ok(p1IceGatheringStateMock.mock.calls.length >= 1);
      assert.ok(p2IceGatheringStateMock.mock.calls.length >= 1);

      assert.equal(p1SDPMock.mock.calls.length, 1);
      assert.equal(p2SDPMock.mock.calls.length, 1);

      assert.ok(p1IceCandidateMock.mock.calls.length >= 1);
      assert.ok(p2IceCandidateMock.mock.calls.length >= 1);

      assert.equal(p1DCMock.mock.calls.length, 1);
      assert.equal(p2DCMock.mock.calls.length, 1);

      assert.equal(p1MessageMock.mock.calls.length, testMessageCount);
      assert.equal(p2MessageMock.mock.calls.length, testMessageCount);

      for (let i = 0; i < testMessageCount; i++) {
        assert.equal(await analyzeData(i, p1MessageMock.mock.calls[i].arguments[0]), true);
        assert.equal(await analyzeData(i, p2MessageMock.mock.calls[i].arguments[0]), true);
      }
    }

    async function nextSendTest(): Promise<void> {
      const current = testMessages[++currentIndex];
      if (!current || !dc1) return;

      dc1.binaryType = current.binaryType;
      if (dc2) {
        dc2.binaryType = current.binaryType;
      }

      if (typeof current.data === 'string') {
        dc1.send(current.data);
      } else if (current.data instanceof Blob) {
        dc1.send(current.data);
      } else {
        dc1.send(current.data);
      }
    }

    peer1.onconnectionstatechange = (): void => {
      p1ConnectionStateMock();
    };
    peer1.oniceconnectionstatechange = (): void => {
      p1IceConnectionStateMock();
    };
    peer1.onicegatheringstatechange = (): void => {
      p1IceGatheringStateMock();
    };
    peer1.onicecandidate = (e): void => {
      p1IceCandidateMock();
      peer2.addIceCandidate(e.candidate);
    };

    peer2.onconnectionstatechange = (): void => {
      p2ConnectionStateMock();
    };
    peer2.oniceconnectionstatechange = (): void => {
      p2IceConnectionStateMock();
    };
    peer2.onicegatheringstatechange = (): void => {
      p2IceGatheringStateMock();
    };
    peer2.onicecandidate = (e): void => {
      p2IceCandidateMock();
      peer1.addIceCandidate(e.candidate);
    };
    peer2.ondatachannel = (dce): void => {
      p2DCMock();
      const channel = dce.channel;
      dc2 = channel;
      channel.onmessage = (msg): void => {
        p2MessageMock(msg.data);
        channel.send(msg.data);
      };
    };

    peer1.createOffer().then((desc) => {
      p1SDPMock();
      peer2.setRemoteDescription(desc);
    });

    peer2.createAnswer().then((answerDesc) => {
      p2SDPMock();
      peer1.setRemoteDescription(answerDesc);
    });

    const sentAll = Promise.withResolvers<void>();

    dc1 = peer1.createDataChannel('test-p2p');
    dc1.onopen = (): void => {
      p1DCMock();
      nextSendTest().catch((err) => sentAll.reject(err));
    };
    dc1.onmessage = (msg): void => {
      p1MessageMock(msg.data);
      if (p1MessageMock.mock.calls.length === testMessageCount) {
        finalizeTest()
          .then(() => sentAll.resolve())
          .catch((err) => sentAll.reject(err));
      } else {
        nextSendTest().catch((err) => sentAll.reject(err));
      }
    };

    await sentAll.promise;
  });

  test('it can access datachannel informational fields after closing', async () => {
    const peer1 = new RTCPeerConnection();
    const peer2 = new RTCPeerConnection();

    const label = 'label';
    const protocol = 'protocol';

    const dc: RTCDataChannel = peer1.createDataChannel(label, { protocol });
    const initialId = dc.id;
    assert.equal(initialId, null);

    const peer1Offer = await peer1.createOffer();
    await peer2.setRemoteDescription(peer1Offer);

    const peer2Answer = await peer2.createAnswer();
    await peer1.setRemoteDescription(peer2Answer);

    peer1.addEventListener('icecandidate', (e) => {
      peer2.addIceCandidate((e as RTCPeerConnectionIceEvent).candidate);
    });

    peer2.addEventListener('icecandidate', (e) => {
      peer1.addIceCandidate((e as RTCPeerConnectionIceEvent).candidate);
    });

    await eventPromise(dc, 'open');

    const id = dc.id;
    assert.ok(typeof id === 'number');
    assert.ok(id >= 0 && id < 65535);
    assert.equal(dc.label, label);
    assert.equal(dc.protocol, protocol);

    peer1.close();
    peer2.close();

    if (dc.readyState !== 'closed') {
      await eventPromise(dc, 'close');
    }

    assert.equal(dc.readyState, 'closed');
    assert.equal(dc.id, id);
    assert.equal(dc.label, label);
    assert.equal(dc.protocol, protocol);
  });

  test('it should accept a preconfigured PeerConnection', () => {
    const peerConnection = new PeerConnection('Peer', { iceServers: [] });

    const originalFunc = peerConnection.state.bind(peerConnection);
    const spy = mock.fn(originalFunc);
    Object.defineProperty(peerConnection, 'state', {
      value: spy,
      writable: true,
      enumerable: true,
    });

    const rtcPeerConnection = new RTCPeerConnection({ peerConnection });
    const connectionState = rtcPeerConnection.connectionState;
    assert.ok(spy.mock.calls.length >= 1);
    assert.equal(connectionState, originalFunc());
    rtcPeerConnection.close();
  });

  test('dispatches RTCErrorEvent on DataChannel errors without ReferenceError', () => {
    let onErrorCb: ((msg: string) => void) | undefined;
    const fakeRawChannel = makeStubRawChannel({
      getLabel: () => 'err-chan',
      onError: (cb) => {
        onErrorCb = cb;
      },
    });

    const dc = new PolyfillRTCDataChannel(fakeRawChannel);
    const errSpy = mock.fn<(ev: RTCErrorEvent) => void>();
    dc.onerror = (ev) => errSpy(ev as RTCErrorEvent);

    onErrorCb?.('sctp failure');
    assert.equal(errSpy.mock.calls.length, 1);
    const evt = errSpy.mock.calls[0]?.arguments[0];
    assert.ok(evt instanceof RTCErrorEvent);
    assert.ok(evt.error instanceof RTCError);
    assert.equal(evt.error.errorDetail, 'data-channel-failure');
    assert.equal(evt.error.message, 'sctp failure');
  });

  test('handles null descriptions, negotiationneeded, config immutability, and ICE parsing', async () => {
    const inputConfig: RTCConfiguration = {
      iceServers: [{ urls: 'stun:stun.l.google.com:19302' }],
    };
    const pc = new RTCPeerConnection(inputConfig);

    assert.equal(typeof inputConfig.iceServers?.[0].urls, 'string');
    assert.equal(pc.localDescription, null);
    assert.equal(pc.remoteDescription, null);
    assert.equal(pc.sctp.transport.iceTransport.getLocalParameters(), null);

    const negSpy = mock.fn();
    pc.onnegotiationneeded = negSpy;
    pc.createDataChannel('chan');
    assert.equal(negSpy.mock.calls.length, 1);

    await assert.rejects(
      () =>
        pc.addIceCandidate({
          candidate: 'candidate:1 1 UDP 2122252543 127.0.0.1 12345 typ host',
          sdpMid: 'audio',
        }),
      /remote description/i,
    );

    const tcpCand = new RTCIceCandidate({
      candidate:
        'a=candidate:1234 1 TCP 2122252543 192.168.1.2 9 typ host tcptype active ufrag abcd',
      sdpMid: 'audio',
    });
    assert.equal(tcpCand.foundation, '1234');
    assert.equal(tcpCand.protocol, 'tcp');
    assert.equal(tcpCand.tcpType, 'active');
    assert.equal(tcpCand.usernameFragment, 'abcd');

    assert.throws(
      () =>
        new RTCDataChannelEvent(
          'datachannel',
          null as unknown as globalThis.RTCDataChannelEventInit,
        ),
      TypeError,
    );
    assert.throws(
      () => new RTCErrorEvent('error', null as unknown as globalThis.RTCErrorEventInit),
      TypeError,
    );

    pc.close();
  });

  test('encodes TURN credentials, updates RTCDataChannel readyState on close, parses ICE edge cases, and handles renegotiation', async () => {
    const logs: string[] = [];
    initLogger('Debug', (_lvl, msg) => logs.push(msg));
    const turnPc = new RTCPeerConnection({
      iceServers: [
        {
          urls: 'turn:turn.example.com:3478',
          username: '1700000000:alice',
          credential: 'p@ss/word',
        },
        {
          urls: 'turn://turn2.example.com:3478',
          username: 'bob',
          credential: 'secret',
        },
      ],
    });
    turnPc.createDataChannel('turn-check');
    await new Promise((r) => setTimeout(r, 150));
    turnPc.close();
    initLogger('Warning');
    assert.equal(
      logs.some((l) => l.includes('turn.example.com:3478')),
      true,
    );
    assert.equal(
      logs.some((l) => l.includes('"ss:3478"')),
      false,
    );
    assert.equal(
      new RTCPeerConnectionIceEvent('icecandidate', { candidate: null }).candidate,
      null,
    );

    const nullCand = new RTCIceCandidate({
      candidate: null as unknown as string,
      sdpMid: '0',
    });
    assert.equal(nullCand.candidate, '');

    const crlfCand = new RTCIceCandidate({
      candidate: 'candidate:1 1 UDP 123 1.2.3.4 5555 typ host ufrag abcd\r\n',
      sdpMid: '0',
    });
    assert.equal(crlfCand.usernameFragment, 'abcd');

    const kwCand = new RTCIceCandidate({
      candidate:
        'candidate:1 1 UDP 123 1.2.3.4 5555 typ srflx raddr 10.0.0.1 rport 1234 ufrag raddr',
      sdpMid: '0',
    });
    assert.equal(kwCand.relatedAddress, '10.0.0.1');
    assert.equal(kwCand.usernameFragment, 'raddr');

    const pc1 = new RTCPeerConnection();
    const pc2 = new RTCPeerConnection();
    assert.equal(pc1.sctp.transport.iceTransport.getRemoteParameters(), null);

    let sawEndOfCandidates = false;
    const dc1 = pc1.createDataChannel('chan-1');
    const orderedReceived: string[] = [];
    const orderedDone = Promise.withResolvers<void>();
    pc2.ondatachannel = (e) => {
      if (e.channel.label === 'chan-1') {
        e.channel.onmessage = (m) => {
          const text =
            typeof m.data === 'string' ? m.data : new TextDecoder().decode(m.data as ArrayBuffer);
          orderedReceived.push(text);
          if (orderedReceived.length === 2) orderedDone.resolve();
        };
      }
    };

    pc1.onicecandidate = (e) => {
      if (e.candidate === null) sawEndOfCandidates = true;
      pc2.addIceCandidate(e.candidate);
    };
    pc2.onicecandidate = (e) => {
      pc1.addIceCandidate(e.candidate);
    };

    const offer1 = await pc1.createOffer();
    await pc2.setRemoteDescription(offer1);
    const answer1 = await pc2.createAnswer();
    await pc1.setRemoteDescription(answer1);
    await eventPromise(dc1, 'open');

    dc1.send(new Blob(['blob-first']));
    dc1.send('string-second');
    await orderedDone.promise;
    assert.deepEqual(orderedReceived, ['blob-first', 'string-second']);
    assert.equal(sawEndOfCandidates, true);
    assert.notEqual(pc1.sctp.transport.iceTransport.getRemoteParameters(), null);

    const stats = await pc1.getStats();
    let foundPair = false;
    for (const [, v] of stats) {
      if (v.type === 'candidate-pair') {
        foundPair = true;
        if (v.currentRoundTripTime !== undefined) {
          assert.ok(v.currentRoundTripTime >= 0 && v.currentRoundTripTime < 0.5);
        }
      }
    }
    assert.equal(foundPair, true);

    pc1.createDataChannel('chan-2');
    const offer2 = await pc1.createOffer();
    assert.notEqual(offer2, offer1);
    assert.equal(offer2.type, 'offer');
    await pc2.setRemoteDescription(offer2);
    const answer2 = await pc2.createAnswer();
    assert.notEqual(answer2, answer1);
    assert.equal(answer2.type, 'answer');
    await pc1.setRemoteDescription(answer2);

    dc1.bufferedAmountLowThreshold = -10;
    assert.equal(dc1.bufferedAmountLowThreshold, 0);
    dc1.close();
    assert.equal(dc1.readyState, 'closing');
    assert.throws(() => dc1.send('after-close'), /not 'open'/);

    pc1.close();
    pc2.close();
    assert.equal(pc1.signalingState, 'closed');
    await assert.rejects(() => pc1.createOffer(), /closed/i);
    await assert.rejects(() => pc2.createAnswer(), /closed/i);
  });

  test('supports answerer renegotiation, disableAutoNegotiation, rollback, mixed-case ICE URLs, empty remote certs, and RTCDataChannel open race', async () => {
    const mixedPc = new RTCPeerConnection({
      iceServers: [{ urls: 'Stun:stun.l.google.com:19302' }],
    });
    assert.deepEqual(mixedPc.getConfiguration().iceServers?.[0].urls, [
      'stun:stun.l.google.com:19302',
    ]);
    assert.deepEqual(mixedPc.sctp.transport.getRemoteCertificates(), []);
    assert.throws(() => mixedPc.setConfiguration({ iceServers: [] }), /not supported/i);

    mixedPc.createDataChannel('rb');
    await mixedPc.createOffer();
    assert.equal(mixedPc.signalingState, 'have-local-offer');
    assert.notEqual(mixedPc.pendingLocalDescription, null);
    assert.equal(mixedPc.currentLocalDescription, null);
    await mixedPc.setLocalDescription({ type: 'rollback' });
    assert.equal(mixedPc.signalingState, 'stable');
    mixedPc.close();

    const pc1 = new RTCPeerConnection();
    const pc2 = new RTCPeerConnection();
    pc1.onicecandidate = (e) => pc2.addIceCandidate(e.candidate);
    pc2.onicecandidate = (e) => pc1.addIceCandidate(e.candidate);
    const dc1 = pc1.createDataChannel('initial');
    const offer1 = await pc1.createOffer();
    await pc1.setLocalDescription(offer1);
    await pc2.setRemoteDescription(offer1);
    const answer1 = await pc2.createAnswer();
    await pc2.setLocalDescription(answer1);
    await pc1.setRemoteDescription(answer1);
    await eventPromise(dc1, 'open');

    pc2.createDataChannel('from-answerer');
    const reoffer = await pc2.createOffer();
    assert.equal(reoffer.type, 'offer');
    await pc1.setRemoteDescription(reoffer);
    const reanswer = await pc1.createAnswer();
    assert.equal(reanswer.type, 'answer');
    await pc2.setRemoteDescription(reanswer);
    pc1.close();
    pc2.close();

    const manual1 = new RTCPeerConnection({
      peerConnection: new PeerConnection('Manual1', {
        iceServers: [],
        disableAutoNegotiation: true,
      }),
    });
    const manual2 = new RTCPeerConnection({
      peerConnection: new PeerConnection('Manual2', {
        iceServers: [],
        disableAutoNegotiation: true,
      }),
    });
    manual1.createDataChannel('manual-dc');
    const mOffer = await manual1.createOffer();
    assert.equal(mOffer.type, 'offer');
    await manual2.setRemoteDescription(mOffer);
    const mAnswer = await manual2.createAnswer();
    assert.equal(mAnswer.type, 'answer');
    await manual1.setRemoteDescription(mAnswer);
    manual1.close();
    manual2.close();

    let openedNow = false;
    const racingRaw = makeStubRawChannel({
      isOpen: () => openedNow,
      getId: () => (openedNow ? 5 : -1),
      getLabel: () => 'race',
      onOpen: () => {
        openedNow = true;
      },
    });
    const racingDc = new PolyfillRTCDataChannel(racingRaw);
    assert.equal(racingDc.readyState, 'open');
    assert.equal(racingDc.id, 5);
  });
});
