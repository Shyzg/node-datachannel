import assert from 'node:assert/strict';
import { after, describe, test } from 'node:test';
import * as nodeDataChannel from '../src/lib/index.ts';

describe('Module Definition', () => {
  after(() => {
    nodeDataChannel.cleanup();
  });

  test('Module Defined', () => {
    assert.ok(nodeDataChannel);
    assert.ok(nodeDataChannel.initLogger);
    assert.ok(nodeDataChannel.PeerConnection);
    assert.equal(typeof nodeDataChannel.PeerConnection, 'function');
    assert.ok(nodeDataChannel.DataChannel);
    assert.equal(typeof nodeDataChannel.DataChannel, 'function');
  });

  test('initLogger works with 1 arg, undefined callback, and custom callback', () => {
    assert.doesNotThrow(() => nodeDataChannel.initLogger('Warning'));
    assert.doesNotThrow(() => nodeDataChannel.initLogger('Warning', undefined));

    const messages: string[] = [];
    assert.doesNotThrow(() =>
      nodeDataChannel.initLogger('Debug', (_level, msg) => {
        messages.push(msg);
      }),
    );

    const pc = new nodeDataChannel.PeerConnection('LogPeer', { iceServers: [] });
    pc.close();
    assert.doesNotThrow(() => nodeDataChannel.initLogger('None'));
    assert.throws(
      () => nodeDataChannel.initLogger('Warn' as unknown as nodeDataChannel.LogLevel),
      /Unknown LogLevel/,
    );
    assert.doesNotThrow(() => nodeDataChannel.initLogger('Warning'));
  });
});

describe('PeerConnection Classes', () => {
  after(() => {
    nodeDataChannel.cleanup();
  });

  test('Create PeerConnection and validate states/options', () => {
    const peer = new nodeDataChannel.PeerConnection('Peer', {
      iceServers: ['stun:stun.l.google.com:19302'],
    });
    assert.ok(peer);
    assert.ok(peer.onStateChange);
    assert.ok(peer.createDataChannel);

    const dcUndef = peer.createDataChannel('undef-opt', undefined);
    assert.equal(dcUndef.getLabel(), 'undef-opt');
    assert.doesNotThrow(() => peer.setLocalDescription(undefined, { iceUfrag: 'abcd' }));

    dcUndef.close();
    peer.close();
    assert.equal(peer.signalingState(), 'closed');

    assert.throws(
      () => new nodeDataChannel.PeerConnection('BadSize', { iceServers: [], maxMessageSize: -1 }),
      /maxMessageSize must be positive/,
    );
    assert.throws(
      () => new nodeDataChannel.PeerConnection('BadMtu', { iceServers: [], mtu: -1 }),
      /mtu must be positive/,
    );
  });

  test('Create Data Channel', () => {
    const peer = new nodeDataChannel.PeerConnection('Peer', {
      iceServers: ['stun:stun.l.google.com:19302'],
    });
    const dc = peer.createDataChannel('test', { protocol: 'test-protocol' });
    assert.ok(dc);
    assert.notEqual(dc.getId(), undefined);
    assert.equal(dc.getProtocol(), 'test-protocol');
    assert.equal(dc.getLabel(), 'test');
    assert.ok(dc.onOpen);
    assert.ok(dc.onMessage);

    dc.close();
    peer.close();
  });
});
