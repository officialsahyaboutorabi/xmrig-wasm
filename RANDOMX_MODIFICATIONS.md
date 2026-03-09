# RandomX Modifications for WebAssembly

This document lists the modifications made to RandomX source code to enable WebAssembly compatibility.

## Files Modified

### 1. src/common.hpp
**Change**: Added WebAssembly detection to disable JIT compilation

```cpp
#if defined(__EMSCRIPTEN__) || defined(__wasm__) || defined(__wasm32__) || defined(__wasm64__)
    // WebAssembly does not support JIT compilation
    #define RANDOMX_HAVE_COMPILER 0
    class JitCompilerFallback;
    using JitCompiler = JitCompilerFallback;
#elif defined(_M_X64) || defined(__x86_64__)
    // ... rest of platform detection
```

### 2. src/intrin_portable.h
**Changes**:
- Disable hardware AES for WebAssembly
- Added WASM checks to `__AES__` and `__CRYPTO__` blocks

```cpp
#if defined(__AES__) && !defined(__EMSCRIPTEN__) && !defined(__wasm__)
    // WebAssembly doesn't support hardware AES even if __AES__ is defined
    #define rx_aesenc_vec_i128 _mm_aesenc_si128
    #define rx_aesdec_vec_i128 _mm_aesdec_si128
    #define HAVE_AES 1
#endif
```

```cpp
#if defined(__CRYPTO__) && !defined(__EMSCRIPTEN__) && !defined(__wasm__)
    // PowerPC crypto - not for WASM
```

### 3. src/soft_aes.h
**Change**: Conditional hardware AES based on HAVE_AES flag

```cpp
template<bool soft>
inline rx_vec_i128 aesenc(rx_vec_i128 in, rx_vec_i128 key) {
#if HAVE_AES
    return soft ? soft_aesenc(in, key) : rx_aesenc_vec_i128(in, key);
#else
    return soft_aesenc(in, key);
#endif
}
```

### 4. src/virtual_machine.cpp
**Change**: Wrap hardware AES test with HAVE_AES check

```cpp
#if !defined(__riscv) && HAVE_AES
    if (!softAes) {
        // Test hardware AES
    }
#endif
```

### 5. src/randomx.cpp
**Changes**:
- Conditional include of compiled VM headers
- Wrap all JIT switch cases with `#if RANDOMX_HAVE_COMPILER`
- Wrap x86-specific Argon2 optimizations

```cpp
#if RANDOMX_HAVE_COMPILER
#include "vm_compiled.hpp"
#include "vm_compiled_light.hpp"
#endif
```

```cpp
#if RANDOMX_HAVE_COMPILER
case RANDOMX_FLAG_JIT:
    // JIT VM creation
    break;
#endif
```

```cpp
#if defined(__x86_64__) || defined(_M_X64)
if (randomx_argon2_impl_avx2() != nullptr && cpu.hasAvx2()) {
    flags |= RANDOMX_FLAG_ARGON2_AVX2;
}
#endif
```

### 6. src/dataset.hpp
**Change**: Wrap AVX2/SSSE3 Argon2 paths with platform check

```cpp
inline randomx_argon2_impl* selectArgonImpl(randomx_flags flags) {
#if defined(__x86_64__) || defined(_M_X64)
    if (flags & RANDOMX_FLAG_ARGON2_AVX2) {
        return randomx_argon2_impl_avx2();
    }
    if (flags & RANDOMX_FLAG_ARGON2_SSSE3) {
        return randomx_argon2_impl_ssse3();
    }
#endif
    return &randomx_argon2_fill_segment_ref;
}
```

## Why These Changes?

| Feature | Issue in WASM | Solution |
|---------|--------------|----------|
| JIT Compilation | Not supported in WASM sandbox | Use interpreted mode only |
| Hardware AES | No AES-NI instructions | Software AES implementation |
| AVX2/SSSE3 | SIMD not available | Reference Argon2 implementation |
| Self-modifying code | Not allowed | Disable JIT completely |

## Result

After modifications, RandomX compiles and runs in WebAssembly using:
- ✅ Interpreted VM (no JIT)
- ✅ Software AES
- ✅ Reference Argon2 (no AVX2/SSSE3)
- ✅ Light mode (256MB cache)
- ✅ Proper memory alignment

## Performance Impact

| Mode | Hashrate (desktop) | Memory |
|------|-------------------|--------|
| Native JIT + HW AES | 2000-5000 H/s | 2GB+ dataset |
| WASM Interpreted + Soft AES | 30-80 H/s | 256MB cache |

The WASM version is ~50x slower but still functional for:
- Browser-based mining demos
- Educational purposes
- Testing pool connections
- Low-difficulty coins
