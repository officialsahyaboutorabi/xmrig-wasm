/*
 * RandomX WASM Integration
 * 
 * Uses interpreted mode with software AES (no JIT, no assembly).
 * This is the only mode supported in WebAssembly sandbox.
 */

#include "randomx_wasm.h"
#include "xmrig/src/3rdparty/randomx/src/randomx.h"
#include <cstring>
#include <cstdio>

namespace xmrig_wasm {

// Global RandomX state (thread-local for safety)
static thread_local randomx_cache* g_cache = nullptr;
static thread_local randomx_vm* g_vm = nullptr;

bool randomx_wasm_init(const uint8_t* seed, size_t seedSize) {
    // Clean up any existing state
    randomx_wasm_destroy();
    
    if (!seed || seedSize == 0) {
        return false;
    }
    
    // Use default flags:
    // - No RANDOMX_FLAG_JIT (not supported in WASM)
    // - No RANDOMX_FLAG_HARD_AES (use software AES)
    // - No RANDOMX_FLAG_FULL_MEM (use light mode with cache)
    // - No RANDOMX_FLAG_LARGE_PAGES (not available in WASM)
    randomx_flags flags = RANDOMX_FLAG_DEFAULT;
    
    // Allocate cache (256 MB for RandomX)
    g_cache = randomx_alloc_cache(flags);
    if (!g_cache) {
        return false;
    }
    
    // Initialize cache with the seed
    randomx_init_cache(g_cache, seed, seedSize);
    
    // Create VM in light mode (uses cache, not full 2GB+ dataset)
    // This is critical for WASM where memory is limited
    g_vm = randomx_create_vm(flags, g_cache, nullptr);
    if (!g_vm) {
        randomx_release_cache(g_cache);
        g_cache = nullptr;
        return false;
    }
    
    return true;
}

void randomx_wasm_hash(const uint8_t* data, size_t size, uint8_t* output) {
    if (!g_vm || !output) {
        if (output) {
            memset(output, 0, 32);
        }
        return;
    }
    
    randomx_calculate_hash(g_vm, data, size, output);
}

void randomx_wasm_destroy() {
    if (g_vm) {
        randomx_destroy_vm(g_vm);
        g_vm = nullptr;
    }
    if (g_cache) {
        randomx_release_cache(g_cache);
        g_cache = nullptr;
    }
}

bool randomx_wasm_is_initialized() {
    return g_vm != nullptr && g_cache != nullptr;
}

} // namespace xmrig_wasm

// C API implementations
extern "C" {

int randomx_wasm_init_c(const uint8_t* seed, size_t seedSize) {
    return xmrig_wasm::randomx_wasm_init(seed, seedSize) ? 1 : 0;
}

void randomx_wasm_hash_c(const uint8_t* data, size_t size, uint8_t* output) {
    xmrig_wasm::randomx_wasm_hash(data, size, output);
}

void randomx_wasm_destroy_c() {
    xmrig_wasm::randomx_wasm_destroy();
}

int randomx_wasm_is_initialized_c() {
    return xmrig_wasm::randomx_wasm_is_initialized() ? 1 : 0;
}

}
