/*
 * Minimal RandomX WebAssembly Miner
 * Simplified implementation that compiles and runs
 */

#include <emscripten.h>
#include <emscripten/bind.h>
#include <cstdint>
#include <cstring>
#include <string>
#include <atomic>
#include <chrono>
#include <thread>

#define RANDOMX_HASH_SIZE 32

class SimpleRandomX {
public:
    void init(const uint8_t* seed, size_t seedSize) {
        for (size_t i = 0; i < 256; i++) {
            cache[i] = i;
        }
        for (size_t i = 0; i < seedSize && i < 256; i++) {
            cache[i] ^= seed[i];
        }
        for (int round = 0; round < 4; round++) {
            for (int i = 0; i < 256; i++) {
                cache[i] = (cache[i] * 1103515245 + 12345) & 0xFF;
                cache[i] ^= cache[(i + 1) % 256];
            }
        }
    }
    
    void hash(const uint8_t* data, size_t size, uint8_t* out) {
        uint64_t state[4] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a};
        
        for (size_t i = 0; i < size; i += 64) {
            uint64_t block[8] = {0};
            size_t blockSize = (size - i) < 64 ? (size - i) : 64;
            memcpy(block, data + i, blockSize);
            
            for (int j = 0; j < 8; j++) {
                state[j % 4] ^= block[j];
                state[j % 4] = (state[j % 4] << 1) | (state[j % 4] >> 63);
            }
        }
        
        for (int i = 0; i < 4; i++) {
            state[i] ^= cache[i * 4] | (cache[i * 4 + 1] << 8) | 
                       (cache[i * 4 + 2] << 16) | (cache[i * 4 + 3] << 24);
        }
        
        memcpy(out, state, RANDOMX_HASH_SIZE);
    }

private:
    uint8_t cache[256];
};

static std::atomic<bool> g_running{false};
static std::atomic<double> g_hashrate{0.0};
static std::atomic<uint64_t> g_totalHashes{0};
static std::atomic<uint64_t> g_sharesFound{0};
static int g_threads = 2;
static std::string g_pool;
static std::string g_wallet;

static void miningThread(int threadId, uint64_t startNonce) {
    SimpleRandomX rx;
    
    uint8_t seed[64];
    memset(seed, 0, 64);
    memcpy(seed, g_wallet.c_str(), g_wallet.size() < 64 ? g_wallet.size() : 64);
    rx.init(seed, 64);
    
    uint64_t nonce = startNonce;
    uint64_t localHashes = 0;
    auto startTime = std::chrono::steady_clock::now();
    
    uint8_t blob[76] = {0};
    memcpy(blob, &nonce, 8);
    
    while (g_running) {
        *(uint64_t*)(blob + 39) = nonce++;
        
        uint8_t hash[RANDOMX_HASH_SIZE];
        rx.hash(blob, sizeof(blob), hash);
        
        localHashes++;
        g_totalHashes.fetch_add(1);
        
        uint64_t* hashVal = (uint64_t*)hash;
        if (*hashVal < 0x00000000FFFFFFFFULL) {
            g_sharesFound.fetch_add(1);
            EM_ASM_({
                if (typeof window !== 'undefined' && window.onShareFound) {
                    window.onShareFound($0);
                }
                console.log("[WASM Miner] Share found!");
            }, threadId);
        }
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration<double>(now - startTime).count();
        if (elapsed >= 1.0) {
            double rate = localHashes / elapsed;
            g_hashrate.store(rate);
            localHashes = 0;
            startTime = now;
        }
    }
}

extern "C" {

EMSCRIPTEN_KEEPALIVE
int wasmStartMining(const char* pool, const char* wallet, int threads) {
    if (g_running) {
        return -1;
    }
    
    g_pool = pool;
    g_wallet = wallet;
    g_threads = threads;
    g_running = true;
    g_totalHashes = 0;
    g_sharesFound = 0;
    
    for (int i = 0; i < threads; i++) {
        std::thread(miningThread, i, i * 0x100000000ULL).detach();
    }
    
    EM_ASM_({
        console.log("[WASM Miner] Started with " + $0 + " threads", $1);
    }, threads, threads);
    
    return 0;
}

EMSCRIPTEN_KEEPALIVE
void wasmStopMining() {
    g_running = false;
    EM_ASM({ console.log("[WASM Miner] Stopping..."); });
}

EMSCRIPTEN_KEEPALIVE
double wasmGetHashrate() {
    return g_hashrate.load();
}

EMSCRIPTEN_KEEPALIVE
uint64_t wasmGetTotalHashes() {
    return g_totalHashes.load();
}

EMSCRIPTEN_KEEPALIVE
uint64_t wasmGetSharesFound() {
    return g_sharesFound.load();
}

EMSCRIPTEN_KEEPALIVE
bool wasmIsRunning() {
    return g_running.load();
}

} // extern "C"

// Embind using std::string
EMSCRIPTEN_BINDINGS(minimal_miner) {
    emscripten::function("startMining", &wasmStartMining, 
                         emscripten::allow_raw_pointers());
    emscripten::function("stopMining", &wasmStopMining);
    emscripten::function("getHashrate", &wasmGetHashrate);
    emscripten::function("getTotalHashes", &wasmGetTotalHashes);
    emscripten::function("getSharesFound", &wasmGetSharesFound);
    emscripten::function("isRunning", &wasmIsRunning);
}

int main() {
    EM_ASM({
        console.log("[WASM Miner] Module loaded");
        console.log("[WASM Miner] Ready to mine");
    });
    return 0;
}
