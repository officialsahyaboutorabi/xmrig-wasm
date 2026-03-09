#!/bin/bash
# Build script for XMRig WebAssembly

set -e

echo "========================================="
echo "  XMRig WebAssembly Build Script"
echo "========================================="
echo ""

# Check for Emscripten
if [ -z "$EMSDK" ]; then
    echo "⚠️  Emscripten not found!"
    echo "Please install and activate Emscripten:"
    echo "  git clone https://github.com/emscripten-core/emsdk.git"
    echo "  cd emsdk"
    echo "  ./emsdk install latest"
    echo "  ./emsdk activate latest"
    echo "  source ./emsdk_env.sh"
    exit 1
fi

echo "✓ Emscripten found: $EMSDK"
echo ""

# Create build directory
mkdir -p build
cd build

# Clone XMRig if not exists
if [ ! -d "xmrig" ]; then
    echo "Cloning XMRig..."
    git clone --depth 1 https://github.com/xmrig/xmrig.git
fi

# Clone RandomX
if [ ! -d "xmrig/src/3rdparty/randomx" ]; then
    echo "Cloning RandomX..."
    git clone --depth 1 https://github.com/tevador/RandomX.git xmrig/src/3rdparty/randomx
fi

# Apply patches
echo "Applying WASM patches..."
cd xmrig

# Patch 1: Disable assembly code (not supported in WASM)
find . -name "*.cpp" -o -name "*.h" | xargs sed -i 's/#include <immintrin.h>/\/\/ #include <immintrin.h>/g' 2>/dev/null || true

# Patch 2: Disable huge pages
grep -r "madvise\|mlock\|MAP_HUGETLB" --include="*.cpp" --include="*.h" -l | xargs sed -i 's/madvise(\([^)]*\));/\/\/ madvise(\1);/g' 2>/dev/null || true

cd ..

# Configure with CMake
echo "Configuring with CMake..."
emcmake cmake xmrig \
    -DCMAKE_BUILD_TYPE=Release \
    -DWITH_OPENCL=OFF \
    -DWITH_CUDA=OFF \
    -DWITH_HWLOC=OFF \
    -DWITH_TLS=OFF \
    -DWITH_HTTP=OFF \
    -DBUILD_STATIC=ON \
    -DCMAKE_CXX_FLAGS="-s USE_PTHREADS=1 -s PTHREAD_POOL_SIZE=8" \
    -DCMAKE_C_FLAGS="-s USE_PTHREADS=1 -s PTHREAD_POOL_SIZE=8"

# Build
echo "Building..."
emmake make -j$(nproc)

# Link to WASM
echo "Linking WebAssembly module..."
emcc -O3 \
    -s WASM=1 \
    -s EXPORTED_FUNCTIONS='["_main", "_startMining", "_stopMining", "_getHashrate", "_setThreads"]' \
    -s EXPORTED_RUNTIME_METHODS='["ccall", "cwrap"]' \
    -s USE_PTHREADS=1 \
    -s PTHREAD_POOL_SIZE=8 \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s INITIAL_MEMORY=256MB \
    -s MAXIMUM_MEMORY=1GB \
    -s MODULARIZE=1 \
    -s EXPORT_NAME="XMRigModule" \
    -s ENVIRONMENT=web,worker \
    --bind \
    src/libxmrig.a \
    -o ../dist/xmrig-wasm.js

cd ..

echo ""
echo "========================================="
echo "✅ Build Complete!"
echo "========================================="
echo ""
echo "Output files:"
echo "  dist/xmrig-wasm.js"
echo "  dist/xmrig-wasm.wasm"
echo ""
