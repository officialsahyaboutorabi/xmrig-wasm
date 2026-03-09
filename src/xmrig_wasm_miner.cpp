/*
 * XMRig WebAssembly Miner - Full Implementation
 * Integrates Stratum client with hashing
 */

#include <emscripten.h>
#include <emscripten/bind.h>
#include <cstdint>
#include <cstring>
#include <string>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
#include <sstream>
#include <iomanip>

#include "stratum_client.h"

// Simple RandomX implementation (placeholder - will be replaced with real RandomX)
class SimpleRandomX {
public:
    void init(const uint8_t* seed, size_t seedSize) {
        for (size_t i = 0; i < 256; i++) {
            cache[i] = i;
        }
        for (size_t i = 0; i < seedSize && i < 256; i++) {
            cache[i] ^= seed[i];
        }
        for (int round = 0; round < 8; round++) {
            for (int i = 0; i < 256; i++) {
                cache[i] = (cache[i] * 1103515245 + 12345) & 0xFF;
                cache[i] ^= cache[(i + 1) % 256];
            }
        }
    }
    
    void hash(const uint8_t* data, size_t size, uint8_t* out) {
        uint64_t state[4] = {0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL, 
                             0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL};
        
        // Simple mixing (NOT real RandomX - just for testing)
        for (size_t i = 0; i < size; i += 64) {
            uint64_t block[8] = {0};
            size_t blockSize = (size - i) < 64 ? (size - i) : 64;
            memcpy(block, data + i, blockSize);
            
            for (int j = 0; j < 8; j++) {
                state[j % 4] ^= block[j];
                state[j % 4] = (state[j % 4] << 1) | (state[j % 4] >> 63);
                state[j % 4] += cache[(i + j) % 256];
            }
        }
        
        // Finalize with cache
        for (int i = 0; i < 4; i++) {
            state[i] ^= cache[i * 4] | ((uint64_t)cache[i * 4 + 1] << 8) | 
                       ((uint64_t)cache[i * 4 + 2] << 16) | ((uint64_t)cache[i * 4 + 3] << 24);
        }
        
        memcpy(out, state, 32);
    }

private:
    uint8_t cache[256];
};

// Global state
static std::atomic<bool> g_running{false};
static std::atomic<double> g_hashrate{0.0};
static std::atomic<uint64_t> g_totalHashes{0};
static std::atomic<uint64_t> g_sharesFound{0};
static int g_threads = 2;

static xmrig::StratumClient* g_stratum = nullptr;
static xmrig::Job g_currentJob;
static std::mutex g_jobMutex;

// Hex conversion helpers
std::string hexToBytes(const std::string& hex) {
    std::string bytes;
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byteString = hex.substr(i, 2);
        char byte = (char) std::stoi(byteString, nullptr, 16);
        bytes.push_back(byte);
    }
    return bytes;
}

std::string bytesToHex(const uint8_t* bytes, size_t len) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; i++) {
        ss << std::setw(2) << (int)bytes[i];
    }
    return ss.str();
}

uint64_t hexToUint64(const std::string& hex) {
    return std::stoull(hex.substr(0, 16), nullptr, 16);
}

// Mining thread
static void miningThread(int threadId) {
    SimpleRandomX rx;
    uint8_t hash[32];
    
    // Initialize RandomX with seed hash from job
    {
        std::lock_guard<std::mutex> lock(g_jobMutex);
        if (!g_currentJob.seedHash.empty()) {
            std::string seedBytes = hexToBytes(g_currentJob.seedHash);
            rx.init(reinterpret_cast<const uint8_t*>(seedBytes.data()), 
                    seedBytes.length());
        } else {
            uint8_t defaultSeed[32] = {0};
            rx.init(defaultSeed, 32);
        }
    }
    
    uint32_t nonce = threadId * 0x10000000;
    uint64_t localHashes = 0;
    auto startTime = std::chrono::steady_clock::now();
    
    while (g_running) {
        xmrig::Job job;
        {
            std::lock_guard<std::mutex> lock(g_jobMutex);
            if (!g_currentJob.isValid()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            job = g_currentJob;
        }
        
        // Convert blob to bytes
        std::string blobBytes = hexToBytes(job.blob);
        if (blobBytes.length() < 43) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        
        // Set nonce in blob (bytes 39-42)
        *reinterpret_cast<uint32_t*>(&blobBytes[39]) = nonce++;
        
        // Calculate hash
        rx.init(reinterpret_cast<const uint8_t*>(hexToBytes(job.seedHash).data()), 32);
        rx.hash(reinterpret_cast<const uint8_t*>(blobBytes.data()), 
                blobBytes.length(), hash);
        
        localHashes++;
        g_totalHashes.fetch_add(1);
        
        // Check if hash meets target
        uint64_t hashValue = *reinterpret_cast<uint64_t*>(hash);
        uint64_t targetValue = hexToUint64(job.target);
        
        if (hashValue < targetValue) {
            // Share found!
            std::string nonceHex = bytesToHex(reinterpret_cast<uint8_t*>(&nonce), 4);
            std::string resultHex = bytesToHex(hash, 32);
            
            EM_ASM_({
                console.log("[WASM Miner] SHARE FOUND! Nonce:", UTF8ToString($0));
            }, nonceHex.c_str());
            
            // Submit share
            if (g_stratum) {
                g_stratum->submit(job.id, nonceHex, resultHex);
            }
            
            g_sharesFound.fetch_add(1);
        }
        
        // Update hashrate
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

// Job callback
static void onNewJob(const xmrig::Job& job) {
    std::lock_guard<std::mutex> lock(g_jobMutex);
    g_currentJob = job;
    
    EM_ASM_({
        console.log("[WASM Miner] New job received, height:", $0, "diff:", $1);
        if (typeof window !== 'undefined' && window.onJobReceived) {
            window.onJobReceived($0, $1);
        }
    }, (int)job.height, (int)job.difficulty);
}

// C API for JavaScript
extern "C" {

EMSCRIPTEN_KEEPALIVE
int wasmStartMining(const char* pool, const char* wallet, int threads) {
    if (g_running) {
        return -1;
    }
    
    // Create stratum client
    if (!g_stratum) {
        g_stratum = new xmrig::StratumClient();
        
        g_stratum->onJob(onNewJob);
        g_stratum->onConnected([]() {
            EM_ASM({ console.log("[WASM Miner] Connected to pool"); });
        });
        g_stratum->onDisconnected([]() {
            EM_ASM({ console.log("[WASM Miner] Disconnected from pool"); });
        });
        g_stratum->onError([](const std::string& err) {
            EM_ASM_({
                console.error("[WASM Miner] Pool error:", UTF8ToString($0));
            }, err.c_str());
        });
    }
    
    // Connect to pool
    if (!g_stratum->connect(pool)) {
        return -2;
    }
    
    // Wait for connection and login
    // (In real implementation, this should be async)
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    std::string worker = "wasm-miner-" + std::to_string(std::rand() % 1000);
    g_stratum->login(wallet, worker);
    
    // Start mining threads
    g_threads = threads;
    g_running = true;
    
    for (int i = 0; i < threads; i++) {
        std::thread(miningThread, i).detach();
    }
    
    EM_ASM_({
        console.log("[WASM Miner] Started with " + $0 + " threads", $1);
    }, threads, threads);
    
    return 0;
}

EMSCRIPTEN_KEEPALIVE
void wasmStopMining() {
    g_running = false;
    
    if (g_stratum) {
        g_stratum->disconnect();
    }
    
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

EMSCRIPTEN_KEEPALIVE
int wasmGetConnectionStatus() {
    if (!g_stratum) return 0;
    if (g_stratum->isConnected()) return 2;
    return 1;
}

} // extern "C"

// Embind
EMSCRIPTEN_BINDINGS(xmrig_wasm) {
    emscripten::function("startMining", &wasmStartMining, 
                         emscripten::allow_raw_pointers());
    emscripten::function("stopMining", &wasmStopMining);
    emscripten::function("getHashrate", &wasmGetHashrate);
    emscripten::function("getTotalHashes", &wasmGetTotalHashes);
    emscripten::function("getSharesFound", &wasmGetSharesFound);
    emscripten::function("isRunning", &wasmIsRunning);
    emscripten::function("getConnectionStatus", &wasmGetConnectionStatus);
}

int main() {
    EM_ASM({
        console.log("[XMRig-WASM] Module loaded successfully");
        console.log("[XMRig-WASM] Ready to mine");
    });
    return 0;
}
