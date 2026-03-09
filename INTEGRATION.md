# XMRig WASM Integration Guide

## Overview

This project provides a complete Monero (RandomX) miner compiled to WebAssembly that can run in web browsers. It uses the real RandomX algorithm with software AES in light mode (256MB cache).

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     Web Browser                             │
│  ┌─────────────────┐  ┌─────────────────────────────────┐  │
│  │  JavaScript API │  │      WebAssembly Module         │  │
│  │  (xmrig-web-    │  │  ┌───────────────────────────┐  │  │
│  │   miner.js)     │◄─┼──┤    XMRig Core + RandomX   │  │  │
│  │                 │  │  │  - Stratum Client         │  │  │
│  │  Event-driven   │  │  │  - RandomX (interpreted)  │  │  │
│  │  interface      │  │  │  - WebSocket support      │  │  │
│  └─────────────────┘  │  └───────────────────────────┘  │  │
│                       └─────────────────────────────────┘  │
│                              │                              │
│                    Web Workers (pthreads)                   │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
                    WebSocket Mining Pool
```

## Build Output

After building, the `dist/` folder contains:

```
dist/
├── xmrig-wasm.wasm      # WebAssembly binary (230KB)
├── xmrig-wasm.js        # Emscripten glue code (55KB)
├── xmrig-web-miner.js   # High-level JavaScript API
├── index.html           # Full-featured demo
├── embed-example.html   # Simple embed example
├── test.html            # Basic test interface
└── README.md            # Documentation
```

## Integration Methods

### Method 1: High-Level JavaScript API (Recommended)

```html
<!DOCTYPE html>
<html>
<head>
    <script src="xmrig-web-miner.js"></script>
    <script src="xmrig-wasm.js"></script>
</head>
<body>
    <button onclick="startMiner()">Start Mining</button>
    <div id="status"></div>
    
    <script>
        const miner = new XMRigWebMiner();
        
        async function startMiner() {
            await miner.init();
            
            miner.addEventListener('stats', (e) => {
                document.getElementById('status').textContent = 
                    `Hashrate: ${e.detail.hashrate.toFixed(1)} H/s`;
            });
            
            await miner.start({
                pool: 'wss://wrxproxy.qzz.io',
                wallet: 'YOUR_WALLET_ADDRESS',
                threads: 2
            });
        }
    </script>
</body>
</html>
```

### Method 2: Direct C Call Interface

```javascript
// Load module
const Module = await XMRigModule();

// Start mining
Module.ccall('wasmStartMining', 'number', 
    ['string', 'string', 'number'],
    ['wss://pool.com', 'WALLET', 2]
);

// Get stats
const hashrate = Module.ccall('wasmGetHashrate', 'number', [], []);
const total = Module.ccall('wasmGetTotalHashes', 'number', [], []);

// Stop
Module.ccall('wasmStopMining', null, [], []);
```

### Method 3: Simple One-Liner

```javascript
// Automatically initializes and starts
const miner = await XMRigWeb.create({
    pool: 'wss://pool.com',
    wallet: 'YOUR_WALLET',
    threads: 4,
    autoStart: true
});

// Access stats anytime
console.log(XMRigWeb.stats.hashrate);
```

## API Reference

### XMRigWebMiner Class

#### Methods

| Method | Description | Returns |
|--------|-------------|---------|
| `init()` | Load WASM module | Promise |
| `start(options)` | Start mining | Promise |
| `stop()` | Stop mining | void |
| `getStats()` | Get current stats | Object |
| `testRandomX(seed, data)` | Test RandomX | Promise |

#### Options

```javascript
{
    pool: 'wss://pool.example.com',    // WebSocket pool URL
    wallet: '47...',                    // Monero wallet address
    worker: 'web-miner',                // Worker name (optional)
    threads: 2                          // Number of threads
}
```

#### Events

```javascript
miner.addEventListener('ready', () => {});
miner.addEventListener('starting', (e) => { e.detail.pool });
miner.addEventListener('started', () => {});
miner.addEventListener('stopped', () => {});
miner.addEventListener('job', (e) => { e.detail.height });
miner.addEventListener('share', () => {});
miner.addEventListener('stats', (e) => { e.detail.hashrate });
```

#### Stats Object

```javascript
{
    hashrate: 25.5,        // Hashes per second
    totalHashes: 15420,    // Total hashes computed
    sharesFound: 3,        // Valid shares submitted
    difficulty: 480000,    // Current job difficulty
    connectionStatus: 2    // 0=disconnected, 1=connecting, 2=connected
}
```

## Server Requirements

### For Hosting the Miner

1. **HTTPS Required** - WebAssembly threads require secure context
2. **COOP/COEP Headers** - Required for SharedArrayBuffer:
   ```
   Cross-Origin-Opener-Policy: same-origin
   Cross-Origin-Embedder-Policy: require-corp
   ```

3. **MIME Types**:
   ```
   .wasm  -> application/wasm
   .js    -> application/javascript
   ```

### Example nginx Configuration

```nginx
server {
    listen 443 ssl;
    server_name miner.example.com;
    
    # Required for SharedArrayBuffer
    add_header Cross-Origin-Opener-Policy "same-origin" always;
    add_header Cross-Origin-Embedder-Policy "require-corp" always;
    
    location / {
        root /var/www/xmrig-wasm/dist;
        try_files $uri $uri/ =404;
    }
    
    location ~ \.wasm$ {
        add_header Content-Type "application/wasm";
    }
}
```

## Pool Compatibility

The miner uses WebSocket connections. Compatible pools:

| Pool | URL Format | Notes |
|------|------------|-------|
| WRXProxy | `wss://wrxproxy.qzz.io` | WebSocket proxy |
| Custom | `wss://pool:port` | Requires WS support |

## Performance Expectations

| Device | Threads | Expected Hashrate |
|--------|---------|-------------------|
| Desktop (i7) | 4 | 30-50 H/s |
| Desktop (i7) | 8 | 50-80 H/s |
| Laptop (i5) | 2 | 10-20 H/s |
| Mobile | 1-2 | 5-15 H/s |

*Note: Performance varies based on browser JS engine and available CPU.

## Browser Compatibility

| Browser | Status | Notes |
|---------|--------|-------|
| Chrome 92+ | ✅ Full | Best performance |
| Firefox 90+ | ✅ Full | Good performance |
| Safari 15+ | ⚠️ Limited | No thread support yet |
| Edge 92+ | ✅ Full | Same as Chrome |

## RandomX Implementation Details

- **Mode**: Interpreted (JIT not available in WASM)
- **AES**: Software implementation (no hardware AES)
- **Memory**: Light mode (256MB cache per thread)
- **Argon2**: Reference implementation (no AVX2/SSSE3)

## Security Considerations

1. **Wallet Address** - Only sent to configured pool
2. **No Data Collection** - No telemetry or tracking
3. **Open Source** - Full code auditable
4. **User Consent** - Always inform users about mining

## Troubleshooting

### "SharedArrayBuffer is not defined"
- Ensure COOP/COEP headers are set
- Use HTTPS (required for threads)

### "WASM module failed to load"
- Check MIME type for .wasm files
- Verify file paths are correct

### "Cannot connect to pool"
- Verify pool supports WebSocket
- Check firewall/proxy settings
- Try different pool

### Low hashrate
- Increase thread count (up to CPU cores)
- Close other browser tabs
- Use Chrome for best performance

## Development

### Rebuilding

```bash
cd build
emcmake cmake ..
emmake make -j4
```

### Files Structure

```
xmrig-wasm/
├── src/
│   ├── xmrig_wasm_miner.cpp    # Main WASM module
│   ├── stratum_client.cpp      # Stratum/WebSocket client
│   ├── randomx_wasm.cpp        # RandomX integration
│   └── xmrig/                  # XMRig source (RandomX)
├── build/                      # Build directory
├── dist/                       # Output files
└── CMakeLists.txt
```

## License

GPL v3 - Same as XMRig and RandomX
