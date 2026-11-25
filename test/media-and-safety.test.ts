import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import dgram from 'node:dgram';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { after, describe, test } from 'node:test';
import { Worker } from 'node:worker_threads';
import defaultExport, * as ndc from '../src/lib/index.ts';

type AnyCtor = new (...args: unknown[]) => unknown;

describe('Media wrappers and native safety', () => {
  after(() => {
    ndc.cleanup();
  });

  test('rejects direct internal constructors and spoofed addTrack inputs without crashing', async () => {
    assert.throws(
      () => new (ndc.DataChannel as unknown as AnyCtor)(),
      /DataChannel cannot be constructed/,
    );
    assert.throws(() => new (ndc.Track as unknown as AnyCtor)(), /Track cannot be constructed/);
    assert.throws(
      () => new (ndc.WebSocket as unknown as AnyCtor)(null, {}),
      /Invalid internal WebSocket/,
    );

    const pc = new ndc.PeerConnection('SafetyPeer', { iceServers: [] });

    assert.throws(() => pc.setLocalDescription('answer'), /Unexpected local description type/);

    const dc = pc.createDataChannel('dc');

    assert.throws(
      () => pc.addTrack({ 'media-type-video': true } as unknown as ndc.Video),
      /Unknown media type/,
    );
    (dc as unknown as Record<string, unknown>)['media-type-video'] = true;
    assert.throws(() => pc.addTrack(dc as unknown as ndc.Video), /Unknown media type/);

    const spoofedDc = pc.createDataChannel('spoof-proto');
    Object.setPrototypeOf(spoofedDc, ndc.Video.prototype);
    assert.throws(() => pc.addTrack(spoofedDc as unknown as ndc.Video), /Unknown media type/);
    assert.throws(
      () => pc.addTrack(Object.create(ndc.Video.prototype) as ndc.Video),
      /Unknown media type/,
    );
    Object.setPrototypeOf(spoofedDc, ndc.DataChannel.prototype);
    spoofedDc.close();

    const validCfg = new ndc.RtpPacketizationConfig(1, 'c', 96, 90000);
    const pkt = new ndc.RtpPacketizer(validCfg);
    const spoofedCfg = new ndc.PeerConnection('SpoofCfg', { iceServers: [] });
    Object.setPrototypeOf(spoofedCfg, ndc.RtpPacketizationConfig.prototype);
    assert.throws(
      () => new ndc.RtpPacketizer(spoofedCfg as unknown as ndc.RtpPacketizationConfig),
      /RtpPacketizationConfig/,
    );
    Object.setPrototypeOf(spoofedCfg, ndc.RtpPacketizer.prototype);
    assert.throws(() => pkt.addToChain(spoofedCfg as unknown as ndc.MediaHandler), /MediaHandler/);
    Object.setPrototypeOf(spoofedCfg, ndc.PeerConnection.prototype);
    spoofedCfg.close();

    assert.equal(dc.getId(), -1);
    assert.equal(pc.remoteFingerprint(), null);

    assert.throws(
      () => new ndc.RtpPacketizationConfig(1, 'c', 96, 2 ** 32),
      /clockRate must be positive/,
    );
    assert.throws(
      () =>
        new ndc.PeerConnection('BadProxy', {
          iceServers: [],
          proxyServer: {
            type: 'socks5' as unknown as ndc.ProxyServerType,
            ip: '127.0.0.1',
            port: 1080,
          },
        }),
      /proxyServer\.type/,
    );
    assert.throws(
      () => pc.createDataChannel('bad-lt', { maxPacketLifeTime: -1 }),
      /maxPacketLifeTime/,
    );
    assert.throws(() => ndc.setSctpSettings({ recvBufferSize: -1 }), /recvBufferSize/);

    assert.throws(() => dc.sendMessage(null as unknown as string), /String expected/);
    assert.throws(
      () => dc.sendMessageBinary(new DataView(new ArrayBuffer(4)) as unknown as Uint8Array),
      /DataChannel not open/,
    );

    const closeEvents: string[] = [];
    const pcClosed = Promise.withResolvers<void>();
    pc.onIceStateChange((s) => closeEvents.push(`ice:${s}`));
    pc.onStateChange((s) => {
      closeEvents.push(`pc:${s}`);
      if (s === 'closed') pcClosed.resolve();
    });
    dc.close();
    pc.close();
    assert.equal(pc.rtt(), -1);
    await pcClosed.promise;
    assert.ok(closeEvents.includes('ice:closed'));
    assert.ok(closeEvents.includes('pc:closed'));
  });

  test('Video defaults to SendOnly, and Video/Audio replaceSSRC uses (oldSsrc, newSsrc) order', () => {
    const defaultVideo = new ndc.Video();
    assert.equal(defaultVideo.direction(), 'SendOnly');

    const video = new ndc.Video('video', 'SendOnly');
    video.addH264Codec(96);
    video.addSSRC(1111, 'cname-v');
    assert.equal(video.hasSSRC(1111), true);
    assert.equal(video.getCNameForSsrc(1111), 'cname-v');
    assert.equal(video.getCNameForSsrc(9999), null);

    video.replaceSSRC(1111, 2222, 'cname-v2');
    assert.equal(video.hasSSRC(1111), false);
    assert.equal(video.hasSSRC(2222), true);
    const ssrcs = video.getSSRCs();
    assert.equal(ssrcs.constructor.name, 'Uint32Array');
    assert.deepEqual(Array.from(ssrcs), [2222]);

    const audio = new ndc.Audio('audio', 'SendOnly');
    audio.addOpusCodec(111);
    audio.addSSRC(3333, 'cname-a');
    audio.replaceSSRC(3333, 4444, 'cname-a2');
    assert.equal(audio.hasSSRC(3333), false);
    assert.equal(audio.hasSSRC(4444), true);
  });

  test('RTP packetizers, PacingHandler validation, RTCP handlers, and default exports work', () => {
    assert.equal(ndc.getLibraryVersion(), '0.24.8');
    assert.equal(defaultExport.H264RtpPacketizer, ndc.H264RtpPacketizer);
    assert.equal(defaultExport.AV1RtpPacketizer, ndc.AV1RtpPacketizer);

    const cfg = new ndc.RtpPacketizationConfig(1234, 'stream', 96, 90000, undefined);
    assert.throws(
      () => new ndc.RtpPacketizationConfig(1234, 'stream', 96, 0),
      /clockRate must be positive/,
    );

    const rtp = new ndc.RtpPacketizer(cfg);
    const h264 = new ndc.H264RtpPacketizer('StartSequence', cfg);
    const h265 = new ndc.H265RtpPacketizer('StartSequence', cfg);
    const av1 = new ndc.AV1RtpPacketizer('Obu', cfg);
    const sr = new ndc.RtcpSrReporter(cfg);
    const nack = new ndc.RtcpNackResponder();
    const recv = new ndc.RtcpReceivingSession();
    const pacing = new ndc.PacingHandler(1_000_000, 10);

    assert.equal(h264.rtpConfig, cfg);
    assert.equal(h265.rtpConfig, cfg);
    assert.equal(av1.rtpConfig, cfg);
    assert.equal(sr.rtpConfig, cfg);

    assert.throws(() => new ndc.PacingHandler(0, 10), /positive finite/);
    assert.throws(() => new ndc.PacingHandler(Number.NaN, 10), /positive finite/);
    assert.throws(() => new ndc.PacingHandler(1_000_000, 0), /sendInterval must be positive/);
    assert.throws(
      () => new ndc.PacingHandler(1_000_000, Number.MAX_SAFE_INTEGER),
      /sendInterval must be positive/,
    );
    assert.throws(() => new ndc.RtcpNackResponder(-1), /between 1 and 65535/);
    assert.throws(() => new ndc.RtcpNackResponder(0), /between 1 and 65535/);
    assert.throws(() => new ndc.RtcpNackResponder(65536), /between 1 and 65535/);
    assert.throws(() => new ndc.RtcpNackResponder(1e12), /between 1 and 65535/);
    assert.throws(
      () => new ndc.H264RtpPacketizer('StartSequence', cfg, 2),
      /maxFragmentSize must be at least 3/,
    );
    assert.throws(
      () => new ndc.H265RtpPacketizer('StartSequence', cfg, 3),
      /maxFragmentSize must be at least 4/,
    );
    assert.throws(
      () => new ndc.AV1RtpPacketizer('Obu', cfg, 1),
      /maxFragmentSize must be at least 2/,
    );

    rtp.addToChain(sr);
    sr.addToChain(nack);
    nack.addToChain(pacing);
    h264.addToChain(recv);

    const pc = new ndc.PeerConnection('MediaPeer', { iceServers: [] });
    const video = new ndc.Video('video', 'SendOnly');
    video.addH264Codec(96);
    const track = pc.addTrack(video);
    track.setMediaHandler(rtp);
    track.close();
    pc.close();
  });

  test('IceUdpMuxListener exposes port/address properties and stops during cleanup', () => {
    const port = 43210;
    const listener = new ndc.IceUdpMuxListener(port, '127.0.0.1');
    assert.equal(listener.port, port);
    assert.equal(listener.address, '127.0.0.1');

    assert.throws(() => new ndc.IceUdpMuxListener(port, '127.0.0.1'));

    ndc.cleanup();
    const listener2 = new ndc.IceUdpMuxListener(port, '127.0.0.1');
    assert.equal(listener2.port, port);
    listener2.stop();
  });

  test('works across sequential and concurrent worker_threads without crashing', async () => {
    const addonPath = path.resolve(process.cwd(), 'build/Release/node_datachannel.node');
    const runWorker = (): Promise<number> =>
      new Promise((resolve, reject) => {
        const w = new Worker(
          `
          const { workerData } = require('node:worker_threads');
          const native = require(workerData.addonPath);
          const pc = new native.PeerConnection('WorkerPeer', { iceServers: [] });
          const dc = pc.createDataChannel('worker-dc');
          const v = new native.Video();
          const t = pc.addTrack(v);
          t.close();
          dc.close();
          pc.close();
          `,
          { eval: true, workerData: { addonPath } },
        );
        w.on('error', reject);
        w.on('exit', resolve);
      });

    assert.equal(await runWorker(), 0);
    assert.equal(await runWorker(), 0);
    assert.deepEqual(await Promise.all([runWorker(), runWorker()]), [0, 0]);

    const unclosedExit = await new Promise<number>((resolve, reject) => {
      const w = new Worker(
        `
        const { workerData } = require('node:worker_threads');
        const native = require(workerData.addonPath);
        const pc = new native.PeerConnection('UnclosedPeer', { iceServers: [] });
        pc.onStateChange(() => {});
        const dc = pc.createDataChannel('unclosed-dc');
        dc.onMessage(() => {});
        process.exit(0);
        `,
        { eval: true, workerData: { addonPath } },
      );
      w.on('error', reject);
      w.on('exit', resolve);
    });
    assert.equal(unclosedExit, 0);

    const loggerExit = await new Promise<number>((resolve, reject) => {
      const w = new Worker(
        `
        const { workerData } = require('node:worker_threads');
        const native = require(workerData.addonPath);
        native.initLogger('Warning', () => {});
        `,
        { eval: true, workerData: { addonPath } },
      );
      w.on('error', reject);
      w.on('exit', resolve);
    });
    assert.equal(loggerExit, 0);
  });

  test('preserves FIFO callback order, handles already-closed channels, reports gathering state, and sends media track packets', async () => {
    const orderEvents: string[] = [];
    const gatheringStates: string[] = [];
    const gatheringDone = Promise.withResolvers<void>();
    const pcOrder = new ndc.PeerConnection('OrderPeer', { iceServers: [] });
    pcOrder.onLocalCandidate(() => {
      if (!orderEvents.includes('cand')) orderEvents.push('cand');
    });
    pcOrder.onLocalDescription(() => {
      orderEvents.push('desc');
    });
    pcOrder.onGatheringStateChange((s) => {
      gatheringStates.push(s);
      if (s === 'complete') gatheringDone.resolve();
    });
    pcOrder.createDataChannel('order-dc');
    try {
      await gatheringDone.promise;
    } finally {
      pcOrder.close();
    }
    assert.equal(orderEvents[0], 'desc');
    assert.ok(gatheringStates.includes('gathering'));
    assert.ok(!gatheringStates.includes('in-progress'));

    const p1 = new ndc.PeerConnection('Fifo1', { iceServers: [] });
    const p2 = new ndc.PeerConnection('Fifo2', { iceServers: [] });
    p1.onLocalDescription((s, t) => p2.setRemoteDescription(s, t));
    p2.onLocalDescription((s, t) => p1.setRemoteDescription(s, t));
    p1.onLocalCandidate((c, m) => p2.addRemoteCandidate(c, m));
    p2.onLocalCandidate((c, m) => p1.addRemoteCandidate(c, m));

    const dcEvents: string[] = [];
    const dcDone = Promise.withResolvers<void>();
    p2.onDataChannel((dc2) => {
      dc2.onClosed(() => {
        dcEvents.push('closed');
        dcDone.resolve();
      });
      dc2.onMessage((msg) => {
        dcEvents.push(`msg:${String(msg)}`);
      });
    });

    const dc1 = p1.createDataChannel('fifo');
    dc1.onOpen(() => {
      dc1.sendMessage('final-payload');
      dc1.close();
      const end = Date.now() + 20;
      while (Date.now() < end) {}
    });

    await dcDone.promise;
    assert.deepEqual(dcEvents, ['msg:final-payload', 'closed']);
    p1.close();
    p2.close();

    const ac1 = new ndc.PeerConnection('AC1', { iceServers: [] });
    const ac2 = new ndc.PeerConnection('AC2', { iceServers: [] });
    ac1.onLocalDescription((s, t) => ac2.setRemoteDescription(s, t));
    ac2.onLocalDescription((s, t) => ac1.setRemoteDescription(s, t));
    ac1.onLocalCandidate((c, m) => ac2.addRemoteCandidate(c, m));
    ac2.onLocalCandidate((c, m) => ac1.addRemoteCandidate(c, m));

    const lateClosed = Promise.withResolvers<void>();
    ac2.onDataChannel((dc2) => {
      setTimeout(() => {
        dc2.onMessage(() => {});
        dc2.onClosed(() => lateClosed.resolve());
      }, 80);
    });
    const acDc = ac1.createDataChannel('quick-close');
    acDc.onOpen(() => acDc.close());
    await lateClosed.promise;
    ac1.close();
    ac2.close();

    const m1 = new ndc.PeerConnection('Media1', { iceServers: [] });
    const m2 = new ndc.PeerConnection('Media2', { iceServers: [] });
    m1.onLocalDescription((s, t) => m2.setRemoteDescription(s, t));
    m2.onLocalDescription((s, t) => m1.setRemoteDescription(s, t));
    m1.onLocalCandidate((c, m) => m2.addRemoteCandidate(c, m));
    m2.onLocalCandidate((c, m) => m1.addRemoteCandidate(c, m));

    const receivedRtp = Promise.withResolvers<Buffer>();
    const sendVideo = new ndc.Video('video', 'SendOnly');
    sendVideo.addH264Codec(96);
    sendVideo.addSSRC(42, 'video-send');
    const sendTrack = m1.addTrack(sendVideo);

    m2.onTrack((recvTrack) => {
      recvTrack.onMessage((pkt) => {
        receivedRtp.resolve(pkt);
      });
    });

    const rtpPacket = Buffer.alloc(16);
    rtpPacket[0] = 0x80;
    rtpPacket[1] = 96;
    rtpPacket.writeUInt16BE(1, 2);
    rtpPacket.writeUInt32BE(90000, 4);
    rtpPacket.writeUInt32BE(42, 8);
    rtpPacket.write('PING', 12, 'ascii');

    sendTrack.onOpen(() => {
      sendTrack.sendMessageBinary(rtpPacket);
    });
    m1.setLocalDescription();

    const gotPacket = await receivedRtp.promise;
    assert.equal(gotPacket.subarray(12).toString('ascii'), 'PING');
    sendTrack.close();
    m1.close();
    m2.close();
  });

  test('rejects MediaHandler cycles, integer wraparound/NaN, and IceServer credentials without relayType', () => {
    const cfg = new ndc.RtpPacketizationConfig(1234, 'stream', 96, 90000);
    const h1 = new ndc.RtpPacketizer(cfg);
    const h2 = new ndc.RtcpSrReporter(cfg);
    const h3 = new ndc.RtcpNackResponder();

    assert.throws(() => h1.addToChain(h1), /cycle/i);
    h1.addToChain(h2);
    assert.throws(() => h2.addToChain(h1), /cycle/i);
    assert.throws(() => h1.addToChain(h2), /cycle/i);
    h3.addToChain(h2);
    assert.throws(() => h1.addToChain(h3), /cycle/i);

    assert.throws(() => new ndc.RtpPacketizationConfig(1, 'c', 256, 90000), /0 and 255/);
    assert.throws(() => new ndc.RtpPacketizationConfig(1, 'c', -1, 90000), /0 and 255/);
    assert.throws(() => {
      cfg.playoutDelayId = -1;
    }, /0 and 255/);
    assert.throws(() => {
      cfg.playoutDelayMin = 70000;
    }, /0 and 65535/);
    assert.throws(() => {
      cfg.playoutDelayMax = Number.NaN;
    }, /0 and 65535/);
    assert.throws(() => new ndc.RtcpNackResponder(Number.NaN), /between 1 and 65535/);
    assert.throws(() => new ndc.RtcpNackResponder(Number.POSITIVE_INFINITY), /between 1 and 65535/);
    assert.throws(() => new ndc.PacingHandler(1_000_000, Number.POSITIVE_INFINITY), /positive/);

    const badMedia = new ndc.Video('v', 'SendOnly');
    assert.throws(() => badMedia.addH264Codec(-1), /between 0 and 127/);
    assert.throws(() => badMedia.addH264Codec(128), /between 0 and 127/);
    assert.throws(() => badMedia.addH264Codec(Number.NaN), /between 0 and 127/);
    assert.throws(() => badMedia.addRTXCodec(-5, 96, 90000), /between 0 and 127/);
    assert.throws(() => badMedia.addRTXCodec(97, 200, 90000), /between 0 and 127/);
    const badAudio = new ndc.Audio('a', 'SendOnly');
    assert.throws(() => badAudio.addOpusCodec(-1), /between 0 and 127/);
    assert.throws(() => badAudio.addAudioCodec(128, 'opus'), /between 0 and 127/);

    assert.throws(
      () =>
        new ndc.PeerConnection('MissingRelayType', {
          iceServers: [{ hostname: 'turn.example.com', port: 3478, username: 'u', password: 'p' }],
        }),
      /relayType/,
    );
  });

  test('exercises Video/Audio SDP helpers, Track controls, IceUdpMuxListener STUN callback, and multi-worker cleanup', async () => {
    const video = new ndc.Video('vid0', 'SendRecv');
    video.addVideoCodec(96, 'H264');
    video.addRTXCodec(97, 96, 90000);
    video.setBitrate(500);
    assert.equal(video.mid(), 'vid0');
    assert.equal(video.hasPayloadType(96), true);
    assert.equal(video.hasPayloadType(97), true);
    assert.equal(video.getBitrate(), 500);
    video.parseSdpLine('a=rtpmap:98 VP8/90000');
    assert.equal(video.hasPayloadType(98), true);
    video.removeFormat('VP8');
    assert.equal(video.hasPayloadType(98), false);
    const sdp = video.generateSdp('\r\n', 'IP4 0.0.0.0', 9);
    assert.ok(sdp.includes('a=mid:vid0'));
    assert.ok(sdp.includes('b=AS:500'));

    const audio = new ndc.Audio('aud0', 'SendOnly');
    audio.addAudioCodec(111, 'opus');
    assert.equal(audio.hasPayloadType(111), true);
    audio.setDirection('Inactive');
    assert.equal(audio.direction(), 'Inactive');

    const pc = new ndc.PeerConnection('TrackCtrl', { iceServers: [] });
    const track = pc.addTrack(video);
    assert.equal(track.mid(), 'vid0');
    assert.equal(track.type(), 'video');
    assert.equal(track.direction(), 'SendRecv');
    assert.equal(track.isClosed(), false);
    assert.ok(track.maxMessageSize() > 0);
    assert.throws(() => track.requestBitrate(-1), /0 and 4294967295/);
    assert.equal(typeof track.requestBitrate(300_000), 'boolean');
    assert.equal(typeof track.requestKeyframe(), 'boolean');
    track.close();
    assert.equal(track.isClosed(), true);
    pc.close();

    const muxPort = 43219;
    const listener = new ndc.IceUdpMuxListener(muxPort, '127.0.0.1');
    const stunReq = Promise.withResolvers<ndc.IceUdpMuxRequest>();
    listener.onUnhandledStunRequest((req) => stunReq.resolve(req));

    const uname = Buffer.from('loc1:rem1', 'ascii');
    const paddedLen = Math.ceil(uname.length / 4) * 4;
    const integrityLen = 24;
    const stunPkt = Buffer.alloc(20 + 4 + paddedLen + integrityLen);
    stunPkt.writeUInt16BE(0x0001, 0);
    stunPkt.writeUInt16BE(4 + paddedLen + integrityLen, 2);
    stunPkt.writeUInt32BE(0x2112a442, 4);
    stunPkt.writeUInt16BE(0x0006, 20);
    stunPkt.writeUInt16BE(uname.length, 22);
    uname.copy(stunPkt, 24);
    const miOffset = 20 + 4 + paddedLen;
    stunPkt.writeUInt16BE(0x0008, miOffset);
    stunPkt.writeUInt16BE(20, miOffset + 2);

    const udp = dgram.createSocket('udp4');
    try {
      await new Promise<void>((resolve, reject) =>
        udp.send(stunPkt, muxPort, '127.0.0.1', (err) => (err ? reject(err) : resolve())),
      );
      const gotStun = await stunReq.promise;
      assert.equal(gotStun.localUfrag, 'loc1');
      assert.equal(gotStun.ufrag, 'rem1');
      assert.equal(gotStun.host, '127.0.0.1');
      assert.ok(gotStun.port > 0);
    } finally {
      udp.close();
      listener.stop();
    }

    const addonPath = path.resolve(process.cwd(), 'build/Release/node_datachannel.node');
    const activePc = new ndc.PeerConnection('MainActive', { iceServers: [] });
    const t0 = Date.now();
    const workerExit = await new Promise<number>((resolve, reject) => {
      const w = new Worker(
        `
        const { workerData } = require('node:worker_threads');
        const native = require(workerData.addonPath);
        const pc = new native.PeerConnection('WorkerTmp', { iceServers: [] });
        pc.close();
        native.cleanup();
        `,
        { eval: true, workerData: { addonPath } },
      );
      w.on('error', reject);
      w.on('exit', resolve);
    });
    assert.equal(workerExit, 0);
    assert.ok(Date.now() - t0 < 3000);
    activePc.close();
  });

  test('sends H.264 frames end-to-end through chained RTP/RTCP/Pacing handlers', async () => {
    const m1 = new ndc.PeerConnection('PacketizerSend', { iceServers: [] });
    const m2 = new ndc.PeerConnection('PacketizerRecv', { iceServers: [] });
    m1.onLocalDescription((s, t) => m2.setRemoteDescription(s, t));
    m2.onLocalDescription((s, t) => m1.setRemoteDescription(s, t));
    m1.onLocalCandidate((c, m) => m2.addRemoteCandidate(c, m));
    m2.onLocalCandidate((c, m) => m1.addRemoteCandidate(c, m));

    const ssrc = 0x12345678;
    const sendVideo = new ndc.Video('video', 'SendOnly');
    sendVideo.addH264Codec(96);
    sendVideo.addSSRC(ssrc, 'h264-stream');
    const sendTrack = m1.addTrack(sendVideo);

    const cfg = new ndc.RtpPacketizationConfig(ssrc, 'h264-stream', 96, 90000);
    const packetizer = new ndc.H264RtpPacketizer('StartSequence', cfg, 100);
    const srReporter = new ndc.RtcpSrReporter(cfg);
    const nackResponder = new ndc.RtcpNackResponder(64);
    const pacing = new ndc.PacingHandler(10_000_000, 5);
    packetizer.addToChain(srReporter);
    srReporter.addToChain(nackResponder);
    nackResponder.addToChain(pacing);
    sendTrack.setMediaHandler(packetizer);

    const receivedPackets: Buffer[] = [];
    const gotFragments = Promise.withResolvers<void>();
    m2.onTrack((recvTrack) => {
      recvTrack.setMediaHandler(new ndc.RtcpReceivingSession());
      recvTrack.onMessage((pkt) => {
        // Ignore RTCP control packets (PT 200..207) and collect RTP packets for PT 96
        if (pkt.length >= 12 && (pkt[1] & 0x7f) === 96) {
          receivedPackets.push(pkt);
          if (receivedPackets.length >= 2) gotFragments.resolve();
        }
      });
    });

    sendTrack.onOpen(() => {
      // Single 180-byte Annex-B IDR NAL unit -> fragmented into >= 2 RTP FU-A packets at maxFragmentSize=100
      const nal = Buffer.alloc(184, 0xab);
      nal[0] = 0x00;
      nal[1] = 0x00;
      nal[2] = 0x00;
      nal[3] = 0x01;
      nal[4] = 0x65;
      sendTrack.sendMessageBinary(nal);
    });
    m1.setLocalDescription();

    await gotFragments.promise;
    assert.ok(receivedPackets.length >= 2);
    for (const pkt of receivedPackets) {
      assert.equal(pkt.readUInt32BE(8), ssrc);
    }

    sendTrack.close();
    m1.close();
    m2.close();
  });

  test('covers PeerConnection/DataChannel config branches and concurrent worker cleanup', async () => {
    const pcConfig = new ndc.PeerConnection('ConfigPeer', {
      iceServers: [
        { hostname: '127.0.0.1', port: 3478 },
        {
          hostname: '127.0.0.1',
          port: 3478,
          username: 'u',
          password: 'p',
          relayType: 'TurnUdp',
        },
        {
          hostname: '127.0.0.1',
          port: 3478,
          username: 'u',
          password: 'p',
          relayType: 'TurnTcp',
        },
        {
          hostname: '127.0.0.1',
          port: 5349,
          username: 'u',
          password: 'p',
          relayType: 'TurnTls',
        },
      ],
      proxyServer: { type: 'Http', ip: '127.0.0.1', port: 8080 },
      portRangeBegin: 40000,
      portRangeEnd: 40100,
      iceTransportPolicy: 'all',
      enableIceTcp: true,
      mtu: 1200,
    });

    const negDc = pcConfig.createDataChannel('negotiated', {
      negotiated: true,
      id: 42,
      unordered: true,
      maxRetransmits: 2,
    });
    assert.equal(negDc.getId(), 42);

    const timedDc = pcConfig.createDataChannel('timed', {
      maxPacketLifeTime: 250,
    });
    assert.equal(timedDc.getLabel(), 'timed');

    assert.throws(
      () =>
        pcConfig.createDataChannel('exclusive', {
          maxPacketLifeTime: 100,
          maxRetransmits: 1,
        }),
      /exclusive/,
    );
    negDc.close();
    timedDc.close();
    pcConfig.close();

    // Custom PEM certificate + key and remoteFingerprint when openssl is available
    let hasOpenssl = false;
    try {
      execFileSync('openssl', ['version'], { stdio: 'ignore' });
      hasOpenssl = true;
    } catch {
      hasOpenssl = false;
    }
    if (hasOpenssl) {
      const tmpDir = fs.mkdtempSync(path.join(os.tmpdir(), 'ndc-pc-cert-'));
      const keyFile = path.join(tmpDir, 'key.pem');
      const certFile = path.join(tmpDir, 'cert.pem');
      try {
        execFileSync(
          'openssl',
          [
            'req',
            '-x509',
            '-newkey',
            'ec',
            '-pkeyopt',
            'ec_paramgen_curve:prime256v1',
            '-keyout',
            keyFile,
            '-out',
            certFile,
            '-days',
            '1',
            '-nodes',
            '-subj',
            '/CN=localhost',
          ],
          { stdio: 'ignore' },
        );
        const p1 = new ndc.PeerConnection('Cert1', {
          iceServers: [],
          certificatePemFile: certFile,
          keyPemFile: keyFile,
        });
        const p2 = new ndc.PeerConnection('Cert2', { iceServers: [] });
        p1.onLocalDescription((s, t) => p2.setRemoteDescription(s, t));
        p2.onLocalDescription((s, t) => p1.setRemoteDescription(s, t));
        p1.onLocalCandidate((c, m) => p2.addRemoteCandidate(c, m));
        p2.onLocalCandidate((c, m) => p1.addRemoteCandidate(c, m));
        const opened = Promise.withResolvers<void>();
        const dc = p1.createDataChannel('fp');
        dc.onOpen(() => opened.resolve());
        await opened.promise;
        const fp = p2.remoteFingerprint();
        assert.ok(fp);
        assert.equal(fp.algorithm, 'sha-256');
        assert.ok(fp.value.length > 0);
        dc.close();
        p1.close();
        p2.close();
      } finally {
        fs.rmSync(tmpDir, { recursive: true, force: true });
      }
    }

    // Concurrent multi-worker create/close/cleanup stress test (must not hit 10s timeout)
    const addonPath = path.resolve(process.cwd(), 'build/Release/node_datachannel.node');
    const spawnRaceWorker = (iterations: number): Promise<number> =>
      new Promise((resolve, reject) => {
        const w = new Worker(
          `
          const { workerData } = require('node:worker_threads');
          const native = require(workerData.addonPath);
          for (let i = 0; i < workerData.iterations; i++) {
            const pc = new native.PeerConnection('Race' + i, { iceServers: [] });
            pc.close();
            native.cleanup();
          }
          `,
          { eval: true, workerData: { addonPath, iterations } },
        );
        w.on('error', reject);
        w.on('exit', resolve);
      });

    const tStart = Date.now();
    const exits = await Promise.all([spawnRaceWorker(4), spawnRaceWorker(4), spawnRaceWorker(4)]);
    assert.deepEqual(exits, [0, 0, 0]);
    assert.ok(Date.now() - tStart < 8000);
  });
});
