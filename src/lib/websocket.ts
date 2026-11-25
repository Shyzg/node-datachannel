import nodeDataChannel from './node-datachannel.ts';
import type { Channel, WebSocketConfiguration } from './types.ts';

export interface WebSocket extends Channel {
  open(url: string): void;
  forceClose(): void;
  remoteAddress(): string | undefined;
  path(): string | undefined;
}
export const WebSocket: {
  new (config?: WebSocketConfiguration): WebSocket;
} = nodeDataChannel.WebSocket;
