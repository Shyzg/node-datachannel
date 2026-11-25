import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { after, describe, mock, test } from 'node:test';
import * as nodeDataChannel from '../src/lib/index.ts';

describe('Websocket', { timeout: 30_000 }, () => {
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

      serverSocket.onError((err) => {
        serverClosed.reject(new Error(`Server WebSocket error: ${err}`));
      });

      serverSocket.onClosed(() => {
        webServerOnClosedMock();
        serverSocket.close();
        serverClosed.resolve();
      });
    });

    clientSocket.onOpen(() => {
      clientOnOpenMock();
      clientSocket.sendMessage('Hello');
    });

    clientSocket.onMessage((message) => {
      clientOnMessageMock(message);
    });

    clientSocket.onError((err) => {
      clientClosed.reject(new Error(`Client WebSocket error: ${err}`));
    });

    clientSocket.onClosed(() => {
      clientOnClosedMock();
      clientClosed.resolve();
    });

    clientSocket.open(`ws://127.0.0.1:${serverPort}`);

    try {
      await Promise.all([serverClosed.promise, clientClosed.promise]);
    } finally {
      clientSocket.close();
      webSocketServer.stop();
    }

    assert.equal(clientOnMessageMock.mock.calls[0]?.arguments[0], 'reply to Hello');
    assert.equal(clientOnOpenMock.mock.calls.length, 1);
    assert.equal(clientOnMessageMock.mock.calls.length, 1);
    assert.equal(clientOnClosedMock.mock.calls.length, 1);

    assert.equal(webServerOnClientMock.mock.calls.length, 1);
    assert.equal(webServerOnMessageMock.mock.calls[0]?.arguments[0], 'Hello');
    assert.equal(webServerOnOpenMock.mock.calls.length, 1);
    assert.equal(webServerOnMessageMock.mock.calls.length, 1);
    assert.equal(webServerOnClosedMock.mock.calls.length, 1);
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
    client.onError((err) => {
      binaryEcho.reject(new Error(`Binary echo client error: ${err}`));
      clientClosed.reject(new Error(`Binary echo client error: ${err}`));
    });
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

    try {
      const echoed = await binaryEcho.promise;
      assert.deepEqual(Array.from(echoed), [10, 20, 30, 40]);

      server.stop();
      await clientClosed.promise;
    } finally {
      server.stop();
      client.forceClose();
    }
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
    ws.onError((err) => {
      opened.reject(new Error(`WebSocket error: ${err}`));
      closed.reject(new Error(`WebSocket error: ${err}`));
    });
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
    try {
      await opened.promise;
      await closed.promise;
    } finally {
      ws.forceClose();
      server.stop();
    }

    const srv2 = new nodeDataChannel.WebSocketServer({ bindAddress: '127.0.0.1', port: 0 });
    const c2 = new nodeDataChannel.WebSocket();
    c2.open(`ws://127.0.0.1:${srv2.port()}`);
    const busyUntil = Date.now() + 80;
    while (Date.now() < busyUntil) {}
    const t0 = Date.now();
    assert.doesNotThrow(() => nodeDataChannel.cleanup());
    assert.ok(Date.now() - t0 < 3000);
  });

  test('supports TLS (wss://) with CA verification and rejects untrusted certificates', async (t) => {
    const findOpenSsl = (): string | null => {
      const candidates =
        process.platform === 'win32'
          ? [
              'openssl',
              'C:\\Program Files\\Git\\usr\\bin\\openssl.exe',
              'C:\\vcpkg\\installed\\x64-windows-static\\tools\\openssl\\openssl.exe',
            ]
          : ['openssl'];
      for (const candidate of candidates) {
        try {
          execFileSync(candidate, ['version'], { stdio: 'ignore' });
          return candidate;
        } catch {
          // Try next candidate
        }
      }
      return null;
    };

    const opensslBin = findOpenSsl();
    if (!opensslBin) {
      t.skip('openssl CLI not available');
      return;
    }

    const tmpDir = fs.mkdtempSync(path.join(os.tmpdir(), 'ndc-ws-tls-'));
    const confFile = path.join(tmpDir, 'openssl.cnf');
    const keyFile = path.join(tmpDir, 'key.pem');
    const certFile = path.join(tmpDir, 'cert.pem');
    try {
      fs.writeFileSync(
        confFile,
        [
          '[req]',
          'distinguished_name = dn',
          'x509_extensions = v3_ext',
          'prompt = no',
          '',
          '[dn]',
          'CN = 127.0.0.1',
          '',
          '[v3_ext]',
          'subjectAltName = IP:127.0.0.1,DNS:localhost',
          '',
        ].join('\n'),
      );

      execFileSync(
        opensslBin,
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
          '-config',
          confFile,
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
      try {
        srv.onClient((socket) => {
          socket.onMessage((msg) => socket.sendMessage(`tls:${String(msg)}`));
        });

        const connectOnce = (opts: nodeDataChannel.WebSocketConfiguration) =>
          new Promise<string>((resolve, reject) => {
            let settled = false;
            const settle = (fn: () => void): void => {
              if (settled) return;
              settled = true;
              fn();
            };

            const client = new nodeDataChannel.WebSocket(opts);
            client.onError((err) => {
              client.forceClose();
              settle(() => reject(new Error(err)));
            });
            client.onClosed(() => {
              settle(() => reject(new Error('TLS WebSocket closed before receiving reply')));
            });
            client.onOpen(() => client.sendMessage('ping'));
            client.onMessage((msg) => {
              client.close();
              settle(() => resolve(String(msg)));
            });
            client.open(`wss://127.0.0.1:${srv.port()}`);
          });

        if (process.platform === 'win32') {
          assert.throws(
            () => new nodeDataChannel.WebSocket({ caCertificatePemFile: certFile }),
            /not supported on Windows/i,
          );
          const strictClient = new nodeDataChannel.WebSocket();
          assert.throws(
            () => strictClient.open(`wss://127.0.0.1:${srv.port()}`),
            /not supported on Windows/i,
          );
          strictClient.close();
          assert.equal(await connectOnce({ disableTlsVerification: true }), 'tls:ping');
        } else {
          assert.equal(await connectOnce({ caCertificatePemFile: certFile }), 'tls:ping');
          assert.equal(await connectOnce({ disableTlsVerification: true }), 'tls:ping');
          await assert.rejects(() => connectOnce({}), /TLS/i);
        }
      } finally {
        srv.stop();
      }
    } finally {
      fs.rmSync(tmpDir, { recursive: true, force: true });
    }
  });

  test('cleans up no-callback sockets on close, delivers pre-close messages, replaces onClient, and bounds timeouts', async () => {
    assert.throws(
      () => new nodeDataChannel.WebSocket({ connectionTimeout: Number.MAX_SAFE_INTEGER }),
      /connectionTimeout/,
    );
    assert.throws(
      () => new nodeDataChannel.WebSocket({ pingInterval: Number.MAX_SAFE_INTEGER }),
      /pingInterval/,
    );
    assert.throws(
      () => new nodeDataChannel.WebSocketServer({ connectionTimeout: Number.MAX_SAFE_INTEGER }),
      /connectionTimeout/,
    );

    const srv = new nodeDataChannel.WebSocketServer({ bindAddress: '127.0.0.1', port: 0 });
    const port = srv.port();

    try {
      // Verify onClient replaces previous handler instead of accumulating listeners
      let firstHandlerCalls = 0;
      const acceptedDeferred = Promise.withResolvers<nodeDataChannel.WebSocket>();
      srv.onClient(() => {
        firstHandlerCalls++;
      });
      srv.onClient((ws) => {
        acceptedDeferred.resolve(ws);
      });

      const c1 = new nodeDataChannel.WebSocket();
      try {
        await new Promise<void>((resolve, reject) => {
          c1.onError((err) => reject(new Error(err)));
          c1.onOpen(resolve);
          c1.open(`ws://127.0.0.1:${port}`);
        });
        const accepted = await acceptedDeferred.promise;
        await new Promise((r) => setTimeout(r, 20));
        assert.equal(firstHandlerCalls, 0);

        // Closing a socket with no registered callbacks must release mChannelPtr immediately
        accepted.close();
        assert.throws(() => accepted.onOpen(() => {}), /destroyed WebSocket/);
      } finally {
        c1.forceClose();
      }

      // Pre-close message delivery works regardless of onClosed vs onMessage registration order
      for (const order of ['onMessage-first', 'onClosed-first'] as const) {
        const got: string[] = [];
        const closed = Promise.withResolvers<void>();
        srv.onClient((ws) => {
          setTimeout(() => {
            if (order === 'onMessage-first') {
              ws.onMessage((m) => got.push(String(m)));
              ws.onClosed(() => closed.resolve());
            } else {
              ws.onClosed(() => closed.resolve());
              ws.onMessage((m) => got.push(String(m)));
            }
          }, 50);
        });

        const cli = new nodeDataChannel.WebSocket();
        cli.onError((err) => closed.reject(new Error(`Client error (${order}): ${err}`)));
        cli.onOpen(() => {
          cli.sendMessage(`payload-${order}`);
          setTimeout(() => cli.close(), 15);
        });
        cli.open(`ws://127.0.0.1:${port}`);
        try {
          await closed.promise;
        } finally {
          cli.forceClose();
        }
        assert.deepEqual(got, [`payload-${order}`]);
      }
    } finally {
      srv.stop();
    }
  });
});
