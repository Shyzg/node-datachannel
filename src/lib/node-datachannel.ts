import { createRequire } from 'node:module';

const require = createRequire(import.meta.url);

function loadNativeAddon() {
  try {
    return require('../../build/Release/node_datachannel.node');
  } catch (err) {
    if ((err as NodeJS.ErrnoException)?.code === 'MODULE_NOT_FOUND') {
      return require('../../build/Debug/node_datachannel.node');
    }
    throw err;
  }
}

const nodeDataChannel = loadNativeAddon();
export default nodeDataChannel;
