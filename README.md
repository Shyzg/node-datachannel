# @shyzg/node-datachannel

Node.js native N-API bindings for [libdatachannel](https://github.com/paullouisageneau/libdatachannel) (`v0.24.8`) — lightweight WebRTC DataChannels, Media Tracks (RTP/RTCP), `DataChannelStream`, WebSockets, and a browser-compatible WebRTC polyfill.

## Features

- **WebRTC DataChannels** — `PeerConnection`, `DataChannel`, `DataChannelStream` (Node.js `Duplex` stream with backpressure), and `IceUdpMuxListener`.
- **Media Tracks & RTP/RTCP** — `Audio`, `Video`, `Track`, `RtpPacketizationConfig`, `RtpPacketizer`, `H264RtpPacketizer`, `H265RtpPacketizer`, `AV1RtpPacketizer`, `RtcpSrReporter`, `RtcpNackResponder`, `RtcpReceivingSession`, and `PacingHandler`.
- **WebSockets** — native `WebSocket` client and `WebSocketServer` with TLS (`wss://`) support.
- **WebRTC Polyfill** — `@shyzg/node-datachannel/polyfill` exposing `RTCPeerConnection`, `RTCDataChannel`, `RTCSessionDescription`, `RTCIceCandidate`, `RTCCertificate`, `RTCSctpTransport`, `RTCDtlsTransport`, `RTCIceTransport`, and WebRTC event classes.
- **Native ESM + CJS compatibility** — ships clean ES modules with TypeScript declarations (`.d.ts`) and works with both `import` and `require()` on Node.js 24+.

## Requirements

This package builds the native addon from source via `cmake-js` during installation:

- **Node.js** `>= 24.0.0`
- **CMake** `>= 3.21`
- **C++17 compiler** (GCC, Clang, or MSVC)
- **Git** (used by CMake `FetchContent` to fetch `libdatachannel`)
- **OpenSSL** development headers (unless building with `USE_GNUTLS=1` or `USE_MBEDTLS=1`)

## Installation

```bash
npm install @shyzg/node-datachannel
# or
pnpm add @shyzg/node-datachannel
```

### Optional Build Flags

You can pass CMake options via `cmake-js` when rebuilding from source:

| Flag                          | Description                                                 |
| ----------------------------- | ----------------------------------------------------------- |
| `USE_NICE=1`                  | Use `libnice` instead of the default `libjuice` ICE backend |
| `USE_GNUTLS=1`                | Use GnuTLS instead of OpenSSL                               |
| `USE_MBEDTLS=1`               | Use Mbed TLS instead of OpenSSL                             |
| `NO_MEDIA=1`                  | Build without media transport / RTP / RTCP support          |
| `NO_WEBSOCKET=1`              | Build without WebSocket client/server support               |
| `OPENSSL_USE_STATIC_LIBS=OFF` | Link dynamically against system OpenSSL                     |

Example:

```bash
npx cmake-js rebuild --CDNO_MEDIA=1 --CDNO_WEBSOCKET=1
```

## Usage

### Native PeerConnection & DataChannel

```ts
import { PeerConnection, cleanup, initLogger } from '@shyzg/node-datachannel';

initLogger('Warning');

const peer1 = new PeerConnection('peer1', { iceServers: [] });
const peer2 = new PeerConnection('peer2', { iceServers: [] });

peer1.onLocalDescription((sdp, type) => peer2.setRemoteDescription(sdp, type));
peer1.onLocalCandidate((candidate, mid) => peer2.addRemoteCandidate(candidate, mid));
peer2.onLocalDescription((sdp, type) => peer1.setRemoteDescription(sdp, type));
peer2.onLocalCandidate((candidate, mid) => peer1.addRemoteCandidate(candidate, mid));

peer2.onDataChannel((dc) => {
  dc.onMessage((msg) => {
    console.log('peer2 received:', msg);
    dc.sendMessage('pong');
  });
});

const dc1 = peer1.createDataChannel('chat');
dc1.onOpen(() => dc1.sendMessage('ping'));
dc1.onMessage((msg) => {
  console.log('peer1 received:', msg);
  peer1.close();
  peer2.close();
  cleanup();
});
```

> Call `cleanup()` on shutdown after closing active peers/sockets to release `libdatachannel` threads cleanly.

### DataChannelStream

Wrap any `DataChannel` in a Node.js `Duplex` stream:

```ts
import { DataChannelStream } from '@shyzg/node-datachannel';

const stream = new DataChannelStream(dc, { textMode: false });
stream.on('data', (chunk) => console.log('received bytes:', chunk.length));
stream.write(Buffer.from('hello world'));
```

### WebSocket & WebSocketServer

```ts
import { WebSocket, WebSocketServer } from '@shyzg/node-datachannel';

const server = new WebSocketServer({ bindAddress: '127.0.0.1', port: 8080 });
server.onClient((client) => {
  client.onMessage((msg) => client.sendMessage(`echo: ${msg}`));
});

const ws = new WebSocket();
ws.onOpen(() => ws.sendMessage('hello'));
ws.onMessage((msg) => {
  console.log('client received:', msg);
  ws.close();
  server.stop();
});
ws.open('ws://127.0.0.1:8080');
```

### WebRTC Polyfill

Use the browser-compatible WebRTC API directly or pass it to libraries such as `simple-peer`:

```ts
import polyfill, { RTCPeerConnection } from '@shyzg/node-datachannel/polyfill';

const pc = new RTCPeerConnection({ iceServers: [] });
const dc = pc.createDataChannel('events');

// Example with simple-peer:
// const peer = new SimplePeer({ initiator: true, wrtc: polyfill });
```

CommonJS consumers on Node.js 24+ can also load the package with `require()`:

```js
const ndc = require('@shyzg/node-datachannel');
const polyfill = require('@shyzg/node-datachannel/polyfill');
```

## Development

```bash
pnpm install            # installs dependencies and compiles the native addon
pnpm run build          # compiles C++ addon and emits TypeScript output to dist/
pnpm run build:ts       # builds TypeScript output only
pnpm run typecheck      # runs tsc --noEmit
pnpm run lint           # runs Biome check (lint + format)
pnpm test               # runs the node:test suite
pnpm run format:cpp     # formats src/cpp with clang-format
```

## License

This package is licensed under the [Apache-2.0 License](./LICENSE).

Note: At build time, CMake fetches [libdatachannel](https://github.com/paullouisageneau/libdatachannel), which is licensed under the **MPL-2.0** license.
