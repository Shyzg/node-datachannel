import assert from 'node:assert/strict';
import { after, describe, mock, test } from 'node:test';
import * as nodeDataChannel from '../src/lib/index.ts';

describe('P2P', { timeout: 30_000 }, () => {
  after(() => {
    nodeDataChannel.cleanup();
  });

  test('P2P Test', async () => {
    const done = Promise.withResolvers<void>();

    const peer1 = new nodeDataChannel.PeerConnection('Peer1', {
      iceServers: [],
    });
    const peer2 = new nodeDataChannel.PeerConnection('Peer2', {
      iceServers: [],
    });

    const p1StateMock = mock.fn();
    const p1GatheringStateMock = mock.fn();
    const p2StateMock = mock.fn();
    const p2GatheringStateMock = mock.fn();
    const p1SDPMock = mock.fn();
    const p1CandidateMock = mock.fn();
    const p2SDPMock = mock.fn();
    const p2CandidateMock = mock.fn();

    const p1DCMock = mock.fn();
    const p1DCMessageMock = mock.fn<(msg: string | Buffer) => void>();
    const p2DCMock = mock.fn();
    const p2DCMessageMock = mock.fn<(msg: string | Buffer) => void>();

    peer1.onStateChange((state) => {
      p1StateMock(state);
      if (state === 'failed') {
        done.reject(new Error('Peer1 entered failed state'));
      }
    });
    peer1.onGatheringStateChange(p1GatheringStateMock);
    peer1.onLocalDescription((sdp, type) => {
      p1SDPMock();
      peer2.setRemoteDescription(sdp, type);
    });
    peer1.onLocalCandidate((candidate, mid) => {
      p1CandidateMock();
      peer2.addRemoteCandidate(candidate, mid);
    });

    peer2.onStateChange((state) => {
      p2StateMock(state);
      if (state === 'failed') {
        done.reject(new Error('Peer2 entered failed state'));
      }
    });
    peer2.onGatheringStateChange(p2GatheringStateMock);
    peer2.onLocalDescription((sdp, type) => {
      p2SDPMock();
      peer1.setRemoteDescription(sdp, type);
    });
    peer2.onLocalCandidate((candidate, mid) => {
      p2CandidateMock();
      peer1.addRemoteCandidate(candidate, mid);
    });
    peer2.onDataChannel((dc) => {
      p2DCMock();
      dc.onMessage((msg) => {
        p2DCMessageMock(msg);
        dc.sendMessage('Hello From Peer2');
      });
    });

    const dc1 = peer1.createDataChannel('test-p2p');
    dc1.onOpen(() => {
      p1DCMock();
      dc1.sendMessage('Hello From Peer1');
    });
    dc1.onMessage((msg) => {
      p1DCMessageMock(msg);
      done.resolve();
    });

    try {
      await done.promise;
    } finally {
      peer1.close();
      peer2.close();
    }

    assert.ok(p1StateMock.mock.calls.length >= 1);
    assert.ok(p1GatheringStateMock.mock.calls.length >= 1);
    assert.ok(p2StateMock.mock.calls.length >= 1);
    assert.ok(p2GatheringStateMock.mock.calls.length >= 1);

    assert.equal(p1SDPMock.mock.calls.length, 1);
    assert.equal(p2SDPMock.mock.calls.length, 1);

    assert.ok(p1CandidateMock.mock.calls.length >= 1);
    assert.ok(p2CandidateMock.mock.calls.length >= 1);

    assert.equal(p1DCMock.mock.calls.length, 1);
    assert.equal(p1DCMessageMock.mock.calls.length, 1);
    assert.equal(p1DCMessageMock.mock.calls[0]?.arguments[0], 'Hello From Peer2');
    assert.equal(p2DCMock.mock.calls.length, 1);
    assert.equal(p2DCMessageMock.mock.calls.length, 1);
    assert.equal(p2DCMessageMock.mock.calls[0]?.arguments[0], 'Hello From Peer1');
  });
});
