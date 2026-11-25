import { EventEmitter } from 'node:events';
import nodeDataChannel from './node-datachannel.ts';
import type { WebSocketServerConfiguration } from './types.ts';
import type { WebSocket } from './websocket.ts';

interface NativeWebSocketServer {
  port(): number;
  stop(): void;
  onClient(cb: (client: WebSocket) => void): void;
}

export class WebSocketServer extends EventEmitter {
  #server: NativeWebSocketServer | null;
  #clients = new Set<WeakRef<WebSocket>>();
  #registry = new FinalizationRegistry<WeakRef<WebSocket>>((ref) => {
    this.#clients.delete(ref);
  });

  constructor(options?: WebSocketServerConfiguration) {
    super();
    if (!nodeDataChannel.WebSocketServer) {
      throw new Error('WebSocket support is disabled in this native build');
    }
    this.#server = new nodeDataChannel.WebSocketServer(options) as NativeWebSocketServer;

    this.#server.onClient((client: WebSocket) => {
      const ref = new WeakRef(client);
      this.#clients.add(ref);
      this.#registry.register(client, ref, ref);
      this.emit('client', client);
    });
  }

  port(): number {
    return this.#server?.port() || 0;
  }

  stop(): void {
    this.#server?.stop();
    this.#server = null;
    for (const ref of this.#clients) {
      this.#registry.unregister(ref);
      ref.deref()?.close();
    }
    this.#clients.clear();
    this.removeAllListeners();
  }

  onClient(cb: (clientSocket: WebSocket) => void): void {
    if (!this.#server) return;
    this.removeAllListeners('client');
    this.on('client', cb);
  }
}
