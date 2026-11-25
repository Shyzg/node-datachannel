import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { after, describe, mock, test } from 'node:test';
import * as nodeDataChannel from '../src/lib/index.ts';

describe('Websocket', () => {
  after(() => {
    nodeDataChannel.cleanup();
  });

  test('can create a websocket client and connect to a server', async () => {
    const webSocketServer = new nodeDataChannel.WebSocketServer({
      bindAddress: '127.0.0.1',
      port: 0,
      maxMessageSize: undefined,
    });
    const serverPort = webSocketServer.port();
    assert.ok(serverPort > 0);

    const clientSocket = new nodeDataChannel.WebSocket({
      maxMessageSize: undefined,
    });

    const clientOnOpenMock = mock.fn();
    const clientOnMessageMock = mock.fn<(msg: string | Buffer) => void>();
    const clientOnClosedMock = mock.fn();

    const webServerOnClientMock = mock.fn();
    const webServerOnOpenMock = mock.fn();
    const webServerOnMessageMock = mock.fn<(msg: string | Buffer) => void>();
    const webServerOnClosedMock = mock.fn();

    const serverClosed = Promise.withResolvers<void>();
    const clientClosed = Promise.withResolvers<void>();

    webSocketServer.onClient((serverSocket) => {
      webServerOnClientMock();

      serverSocket.onOpen(() => {
        webServerOnOpenMock();
      });

      serverSocket.onMessage((message) => {
        webServerOnMessageMock(message);
        serverSocket.sendMessage(`reply to ${message}`);
        serverSocket.close();
      });

      serverSocket.onClosed(() => {
        webServerOnClosedMock();
        serverSocket.close();
        serverClosed.resolve();
      });
    });

    clientSocket.open(`ws://127.0.0.1:${serverPort}`);
    clientSocket.onOpen(() => {
      clientOnOpenMock();
      clientSocket.sendMessage('Hello');
    });

    clientSocket.onMessage((message) => {
      clientOnMessageMock(message);
    });

    clientSocket.onClosed(() => {
      clientOnClosedMock();
      clientClosed.resolve();
    });

    await Promise.all([serverClosed.promise, clientClosed.promise]);

    assert.equal(clientOnMessageMock.mock.calls[0]?.arguments[0], 'reply to Hello');
    assert.equal(clientOnOpenMock.mock.calls.length, 1);
    assert.equal(clientOnMessageMock.mock.calls.length, 1);
    assert.equal(clientOnClosedMock.mock.calls.length, 1);

    assert.equal(webServerOnClientMock.mock.calls.length, 1);
    assert.equal(webServerOnMessageMock.mock.calls[0]?.arguments[0], 'Hello');
    assert.equal(webServerOnOpenMock.mock.calls.length, 1);
    assert.equal(webServerOnMessageMock.mock.calls.length, 1);
    assert.equal(webServerOnClosedMock.mock.calls.length, 1);

    clientSocket.close();
    webSocketServer.stop();
  });

  test('validates negative maxMessageSize and cleans up unopened sockets on close', () => {
    assert.throws(
      () => new nodeDataChannel.WebSocket({ maxMessageSize: -1 }),
      /maxMessageSize must be positive/,
    );
    assert.throws(
      () => new nodeDataChannel.WebSocketServer({ maxMessageSize: -1 }),
      /maxMessageSize must be positive/,
    );

    const unopened = new nodeDataChannel.WebSocket();
    unopened.onClosed(() => {});
    assert.doesNotThrow(() => unopened.close());

    assert.throws(
      () =>
        new nodeDataChannel.WebSocket({
          proxyServer: {
            type: 'socks5' as unknown as nodeDataChannel.ProxyServerType,
            ip: '127.0.0.1',
            port: 1080,
          },
        }),
      /proxyServer\.type/,
    );
  });

  test('supports binary messages, forceClose, and WebSocketServer.stop closing active clients', async () => {
    const server = new nodeDataChannel.WebSocketServer({
      bindAddress: '127.0.0.1',
      port: 0,
    });
    const port = server.port();

    const binaryEcho = Promise.withResolvers<Buffer>();
    const clientClosed = Promise.withResolvers<void>();

    server.onClient((srvSocket) => {
      srvSocket.onMessage((msg) => {
        if (Buffer.isBuffer(msg)) {
          srvSocket.sendMessageBinary(msg);
        }
      });
    });

    const client = new nodeDataChannel.WebSocket();
    client.onClosed(() => {
      clientClosed.resolve();
    });
    client.onMessage((msg) => {
      if (Buffer.isBuffer(msg)) {
        binaryEcho.resolve(msg);
      }
    });
    client.onOpen(() => {
      client.sendMessageBinary(new Uint8Array([10, 20, 30, 40]));
    });
    client.open(`ws://127.0.0.1:${port}`);

    const echoed = await binaryEcho.promise;
    assert.deepEqual(Array.from(echoed), [10, 20, 30, 40]);

    server.stop();
    await clientClosed.promise;
    client.forceClose();
  });

  test('recovers after failed open(), validates negative timeouts/protocols, and cleans up queued clients quickly', async () => {
    assert.throws(
      () => new nodeDataChannel.WebSocket({ connectionTimeout: -1 }),
      /connectionTimeout/,
    );
    assert.throws(() => new nodeDataChannel.WebSocket({ pingInterval: -1 }), /pingInterval/);
    assert.throws(
      () => new nodeDataChannel.WebSocket({ maxOutstandingPings: -1 }),
      /maxOutstandingPings/,
    );
    assert.throws(
      () => new nodeDataChannel.WebSocket({ protocols: [123 as unknown as string] }),
      /protocols entries must be strings/,
    );
    assert.throws(
      () => new nodeDataChannel.WebSocketServer({ connectionTimeout: -1 }),
      /connectionTimeout/,
    );

    const server = new nodeDataChannel.WebSocketServer({ bindAddress: '127.0.0.1', port: 0 });
    const port = server.port();
    const ws = new nodeDataChannel.WebSocket();
    assert.throws(() => ws.open('http://invalid-scheme.example'), /Invalid WebSocket scheme/);

    const opened = Promise.withResolvers<void>();
    const closed = Promise.withResolvers<void>();
    let closedBeforeOpen = false;
    ws.onClosed(() => {
      if (!ws.isOpen()) closedBeforeOpen = true;
      closed.resolve();
    });
    ws.onOpen(() => {
      opened.resolve();
      ws.close();
    });
    await new Promise((r) => setTimeout(r, 20));
    assert.equal(closedBeforeOpen, false);

    ws.open(`ws://127.0.0.1:${port}`);
    await opened.promise;
    await closed.promise;
    server.stop();

    const srv2 = new nodeDataChannel.WebSocketServer({ bindAddress: '127.0.0.1', port: 0 });
    const c2 = new nodeDataChannel.WebSocket();
    c2.open(`ws://127.0.0.1:${srv2.port()}`);
    const busyUntil = Date.now() + 80;
    while (Date.now() < busyUntil) {}
    const t0 = Date.now();
    assert.doesNotThrow(() => nodeDataChannel.cleanup());
    assert.ok(Date.now() - t0 < 3000);
  });

  test('supports TLS (wss://) with CA verification and rejects untrusted certificates', async () => {
    const tmpDir = fs.mkdtempSync(path.join(os.tmpdir(), 'ndc-ws-tls-'));
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
          '/CN=127.0.0.1',
          '-addext',
          'subjectAltName=IP:127.0.0.1,DNS:localhost',
        ],
        { stdio: 'ignore' },
      );

      const srv = new nodeDataChannel.WebSocketServer({
        bindAddress: '127.0.0.1',
        port: 0,
        enableTls: true,
        certificatePemFile: certFile,
        keyPemFile: keyFile,
      });
      srv.onClient((socket) => {
        socket.onMessage((msg) => socket.sendMessage(`tls:${String(msg)}`));
      });

      const connectOnce = (opts: nodeDataChannel.WebSocketConfiguration) =>
        new Promise<string>((resolve, reject) => {
          const client = new nodeDataChannel.WebSocket(opts);
          client.onError((err) => {
            client.forceClose();
            reject(new Error(err));
          });
          client.onOpen(() => client.sendMessage('ping'));
          client.onMessage((msg) => {
            client.close();
            resolve(String(msg));
          });
          client.open(`wss://127.0.0.1:${srv.port()}`);
        });

      assert.equal(await connectOnce({ caCertificatePemFile: certFile }), 'tls:ping');
      assert.equal(await connectOnce({ disableTlsVerification: true }), 'tls:ping');
      await assert.rejects(() => connectOnce({}), /TLS/i);
      srv.stop();
    } finally {
      fs.rmSync(tmpDir, { recursive: true, force: true });
    }
  });
});
