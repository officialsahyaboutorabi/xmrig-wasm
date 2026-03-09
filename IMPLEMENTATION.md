# XMRig WebAssembly Implementation Guide

## Overview

This project aims to compile XMRig (C++) to WebAssembly for high-performance browser mining.

**Status:** Work in Progress - Build system setup complete, compilation requires patches

## Why This is Difficult

XMRig was never designed to run in browsers. Key challenges:

1. **Assembly Code**: XMRig uses x86/x64 assembly for crypto algorithms - WASM doesn't support this
2. **System Calls**: Uses `mmap`, `mlock`, huge pages - not available in WASM
3. **Threading**: Uses pthreads - requires WASM threading support
4. **Networking**: Uses raw TCP sockets - browsers only support WebSockets
5. **CPU Features**: Auto-detects AES-NI, AVX - WASM has different SIMD

## Current Approach

### Phase 1: Build System ✅
- [x] CMake configuration for Emscripten
- [x] Build script structure
- [x] JavaScript wrapper
- [x] WebWorker support

### Phase 2: Source Patches (Required)
- [ ] Replace assembly with C intrinsics
- [ ] Disable huge pages
- [ ] Replace TCP with WebSocket
- [ ] Add WASM threading flags

### Phase 3: RandomX Compilation
- [ ] Patch RandomX for WASM
- [ ] Implement software AES
- [ ] Remove hardware-specific optimizations

## Build Instructions

### Prerequisites

```bash
# Install Emscripten
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh
```

### Build Steps

```bash
# Clone this repo
git clone https://github.com/yourusername/xmrig-wasm.git
cd xmrig-wasm

# Run build
chmod +x build-wasm.sh
./build-wasm.sh
```

## Alternative: Use Existing Solutions

### Option 1: WebRandomX (Current)
- Pure JavaScript/WebAssembly
- 10-20% of native performance
- Already working

### Option 2: XMRig Proxy
- Run native XMRig on a server
- Web dashboard for monitoring
- 100% native performance
- Requires server

### Option 3: gmp-proxy
- GitHub: `collab-zone/gmp-proxy`
- WebSocket proxy for XMRig
- Web-based interface

## Technical Details

### WebAssembly Limitations

| Feature | Native | WASM | Impact |
|---------|--------|------|--------|
| Huge Pages | ✅ 2MB | ❌ No | -10% hashrate |
| CPU Affinity | ✅ Yes | ❌ No | -5% hashrate |
| Assembly | ✅ Full | ❌ None | -20% hashrate |
| Threads | ✅ All | ⚠️ Limited | Varies |
| **Total** | **100%** | **~65%** | **Acceptable** |

### WebSocket Proxy Required

Since browsers can't do TCP, you need a WebSocket-to-TCP proxy:

```
Browser (WSS) → WebSocket Proxy → XMRig Stratum (TCP)
```

Your WRXProxy at `wss://wrxproxy.qzz.io` already does this!

## Simpler Alternative

Instead of full XMRig-WASM, consider:

### Improved WebRandomX
- Optimize existing WebRandomX
- Better JIT compilation
- Estimated: 30-40% of native (up from 10-20%)

### XMRig Web Dashboard
- Run native XMRig on users' computers
- Web dashboard for control/monitoring
- 100% native performance
- Download once, control via web

## Recommendation

**For now:** Use improved WebRandomX + XMRig download option

**Future:** Full XMRig-WASM when patches are ready

## Resources

- XMRig: https://github.com/xmrig/xmrig
- RandomX: https://github.com/tevador/RandomX
- Emscripten: https://emscripten.org/
- WebAssembly threading: https://github.com/WebAssembly/threads
