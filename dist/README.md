# XMRig WebAssembly Miner

A complete Monero (RandomX) miner running in WebAssembly with real RandomX algorithm implementation.

## Features

- ✅ **Real RandomX Algorithm** - Not a placeholder, actual RandomX implementation
- ✅ **WebSocket Stratum** - Connects to mining pools via WebSocket
- ✅ **Threading Support** - Multi-threaded mining using Web Workers
- ✅ **Light Mode** - Uses 256MB cache instead of 2GB+ dataset
- ✅ **Software AES** - No hardware AES required
- ✅ **Embeddable API** - Easy JavaScript integration

## Files

| File | Description |
|------|-------------|
| `xmrig-wasm.wasm` | WebAssembly binary (~230KB) |
| `xmrig-wasm.js` | Emscripten glue code |
| `xmrig-web-miner.js` | High-level JavaScript API |
| `index.html` | Full-featured demo |
| `test.html` | Simple test interface |

## Quick Start

### Basic Usage

```html
<script src="xmrig-web-miner.js"></script>
<script src="xmrig-wasm.js"></script>
<script>
const miner = new XMRigWebMiner();

// Initialize
await miner.init();

// Start mining
await miner.start({
    pool: 'wss://wrxproxy.qzz.io',
    wallet: 'YOUR_WALLET_ADDRESS',
    threads: 2
});

// Listen for events
miner.addEventListener('share', () => {
    console.log('Share found!');
});

miner.addEventListener('stats', (e) => {
    console.log('Hashrate:', e.detail.hashrate);
});

// Stop mining
miner.stop();
</script>
```

### Simple API

```javascript
// One-liner to start mining
const miner = await XMRigWeb.create({
    pool: 'wss://your-pool.com',
    wallet: 'YOUR_WALLET',
    threads: 4,
    autoStart: true
});

// Check stats
console.log(XMRigWeb.stats);
```

## Configuration

| Option | Description | Default |
|--------|-------------|---------|
| `pool` | WebSocket pool URL | Required |
| `wallet` | Monero wallet address | Required |
| `worker` | Worker name | "web-miner" |
| `threads` | Number of threads | 2 |

## Events

| Event | Description | Detail |
|-------|-------------|--------|
| `ready` | WASM loaded | - |
| `starting` | Starting to mine | Config object |
| `started` | Mining started | - |
| `stopped` | Mining stopped | - |
| `job` | New job received | `{height, difficulty, seedHash}` |
| `share` | Share found | `{nonce}` |
| `stats` | Stats update | Stats object |

## Pool Compatibility

The miner connects to pools via WebSocket. Compatible with:
- WRXProxy (wss://wrxproxy.qzz.io)
- Any pool supporting WebSocket stratum

## Technical Details

### RandomX Implementation

- **Mode**: Interpreted (no JIT - not supported in WASM)
- **AES**: Software implementation
- **Memory**: Light mode (256MB cache per thread)
- **Performance**: ~10-50 H/s per thread (browser dependent)

### Threading

Uses Emscripten pthreads with Web Workers:
- SharedArrayBuffer required
- Proper COOP/COEP headers needed

## Testing

Open `index.html` in a browser and click "Run RandomX Test" to verify the implementation.

## Security Notes

- Wallet address is only sent to the configured pool
- No data collection or telemetry
- Open source - audit the code

## License

Same as XMRig - GPL v3
