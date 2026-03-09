/*
 * RandomX WASM Integration Header
 * 
 * Provides a simple C++ wrapper around RandomX library for WebAssembly.
 * Uses interpreted mode with software AES (no JIT, no hardware AES).
 */

#ifndef RANDOMX_WASM_H
#define RANDOMX_WASM_H

#include <cstdint>
#include <cstddef>

namespace xmrig_wasm {

// Initialize RandomX with a seed (typically the block hashing blob prefix)
// Returns true on success, false on failure
bool randomx_wasm_init(const uint8_t* seed, size_t seedSize);

// Calculate a RandomX hash
// Must call randomx_wasm_init first
void randomx_wasm_hash(const uint8_t* data, size_t size, uint8_t* output);

// Cleanup RandomX resources
void randomx_wasm_destroy();

// Check if RandomX is initialized
bool randomx_wasm_is_initialized();

} // namespace xmrig_wasm

// C API for external use (embind)
extern "C" {
    int randomx_wasm_init_c(const uint8_t* seed, size_t seedSize);
    void randomx_wasm_hash_c(const uint8_t* data, size_t size, uint8_t* output);
    void randomx_wasm_destroy_c();
    int randomx_wasm_is_initialized_c();
}

#endif // RANDOMX_WASM_H
