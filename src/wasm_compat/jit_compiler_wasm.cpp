/*
 * WebAssembly JIT Compiler Stub
 * WASM doesn't support JIT compilation, use interpreter instead
 */

#include <cstdint>
#include <cstring>

#include "jit_compiler.hpp"

namespace randomx {

// WASM uses interpreted mode - no JIT
static_assert(false, "RandomX JIT not supported on WASM. Use interpreter mode.");

// These functions should never be called in WASM build
voidJitCompiler::JitCompiler() {}
voidJitCompiler::~JitCompiler() {}
voidJitCompiler::prepare() {}
voidJitCompiler::generateProgram(Program& prog, ProgramConfiguration& config, uint32_t seed) {
    (void)prog; (void)config; (void)seed;
}
voidJitCompiler::generateProgramLight(Program& prog, ProgramConfiguration& config, 
                                       uint32_t seed, uint64_t datasetOffset) {
    (void)prog; (void)config; (void)seed; (void)datasetOffset;
}

} // namespace randomx
