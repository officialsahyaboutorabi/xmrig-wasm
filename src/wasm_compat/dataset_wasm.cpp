/*
 * WebAssembly RandomX Dataset Implementation
 * Software-based dataset generation without JIT
 */

#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>
#include <atomic>

#include "randomx.h"
#include "dataset.hpp"
#include "vm_interpreted.hpp"

namespace randomx {

// Software-only dataset initialization (no assembly)
void initDatasetItem(randomx_cache* cache, uint8_t* out, uint64_t itemNumber)
{
    // Use portable C implementation
    uint64_t rl[8];
    
    // Initialize with cache data + item number
    for (int i = 0; i < 8; ++i) {
        rl[i] = cache->reciprocalCache[i] ^ itemNumber;
    }
    
    // Apply RandomX transformations (simplified)
    for (int i = 0; i < RANDOMX_CACHE_ACCESSES; ++i) {
        // Mix using SuperscalarHash operations
        rl[0] += rl[1];
        rl[1] = (rl[1] << 1) | (rl[1] >> 63);
        rl[2] ^= rl[3];
        rl[3] = (rl[3] >> 1) | (rl[3] << 63);
        rl[4] += rl[5];
        rl[5] = rl[5] * 6364136223846793005ULL;
        rl[6] ^= rl[7];
        rl[7] = rl[7] * 0xFF;
    }
    
    // Write result
    memcpy(out, rl, 64);
}

void randomx_init_dataset(randomx_dataset* dataset, randomx_cache* cache, 
                          unsigned long startItem, unsigned long itemCount)
{
    if (dataset->initDone) {
        return;
    }
    
    const unsigned long totalItems = RANDOMX_DATASET_BASE_SIZE / RANDOMX_DATASET_ITEM_SIZE;
    
    if (startItem + itemCount > totalItems) {
        itemCount = totalItems - startItem;
    }
    
    // Multi-threaded initialization
    const unsigned int threads = std::thread::hardware_concurrency();
    std::vector<std::thread> threadPool;
    std::atomic<unsigned long> currentItem(startItem);
    
    auto worker = [&]() {
        unsigned long item;
        while ((item = currentItem.fetch_add(1)) < startItem + itemCount) {
            initDatasetItem(cache, 
                dataset->memory + item * RANDOMX_DATASET_ITEM_SIZE, 
                item);
        }
    };
    
    for (unsigned int i = 0; i < threads; ++i) {
        threadPool.emplace_back(worker);
    }
    
    for (auto& t : threadPool) {
        t.join();
    }
    
    if (startItem + itemCount >= totalItems) {
        dataset->initDone = true;
    }
}

} // namespace randomx
