/*
 * WebAssembly Virtual Memory Implementation
 * Replaces Unix/Windows memory mapping for WASM
 */

#include <cstdlib>
#include <cstring>
#include <new>

#include "crypto/common/VirtualMemory.h"

namespace xmrig {

// Simple heap allocation for WASM - no huge pages
void *VirtualMemory::allocateExecutableMemory(size_t size)
{
    void *mem = std::aligned_alloc(4096, size);
    if (!mem) {
        throw std::bad_alloc();
    }
    std::memset(mem, 0xCC, size); // Fill with INT3 for safety
    return mem;
}

void *VirtualMemory::allocateLargePagesMemory(size_t size)
{
    // WASM doesn't support huge pages, use regular allocation
    return allocateExecutableMemory(size);
}

void VirtualMemory::flushInstructionCache(void *addr, size_t size)
{
    // No-op on WASM - no instruction cache control
    (void)addr;
    (void)size;
}

void VirtualMemory::protectExecutableMemory(void *addr, size_t size)
{
    // WASM memory is always executable in our configuration
    (void)addr;
    (void)size;
}

void VirtualMemory::unprotectExecutableMemory(void *addr, size_t size)
{
    // WASM memory is always writable in our configuration
    (void)addr;
    (void)size;
}

void VirtualMemory::freeExecutableMemory(void *addr, size_t size)
{
    std::free(addr);
}

bool VirtualMemory::isHugePagesAvailable()
{
    return false; // WASM doesn't support huge pages
}

size_t VirtualMemory::align(size_t pos, size_t align)
{
    return ((pos - 1) / align + 1) * align;
}

} // namespace xmrig
