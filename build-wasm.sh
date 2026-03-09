#!/bin/bash
# XMRig WebAssembly Build Script

set -e

echo "========================================="
echo "  XMRig WebAssembly Build"
echo "========================================="
echo ""

# Source Emscripten environment
if [ -f ~/emsdk/emsdk_env.sh ]; then
    source ~/emsdk/emsdk_env.sh
else
    echo "❌ Emscripten not found! Install with:"
    echo "  git clone https://github.com/emscripten-core/emsdk.git"
    echo "  cd emsdk && ./emsdk install latest && ./emsdk activate latest"
    exit 1
fi

echo "✓ Emscripten: $(emcc --version | head -1)"
echo ""

# Create build directory
mkdir -p build
cd build

echo "=== Configuring with CMake ==="
emcmake cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS="-O3 -flto -pthread -matomics" \
    -DCMAKE_C_FLAGS="-O3 -flto -pthread -matomics"

echo ""
echo "=== Building ==="
emmake make -j$(sysctl -n hw.ncpu) 2>&1 | tee build.log

echo ""
echo "========================================="

if [ -f "../dist/xmrig-wasm.js" ]; then
    echo "✅ Build successful!"
    echo ""
    echo "Output files:"
    ls -lh ../dist/
    echo ""
    echo "File sizes:"
    du -h ../dist/*
else
    echo "❌ Build failed - check build.log"
    exit 1
fi

echo ""
echo "========================================="
echo "Next steps:"
echo "  1. cd ~/xmrig-wasm"
echo "  2. python3 -m http.server 8080"
echo "  3. Open http://localhost:8080/demo/"
echo "========================================="
