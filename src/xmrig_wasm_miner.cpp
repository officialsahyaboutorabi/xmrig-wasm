/*
 * XMRig WebAssembly Miner - Full Implementation with Real RandomX
 * Integrates Stratum client with real RandomX hashing
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
#include <memory>

#include "stratum_client.h"
#include "randomx_wasm.h"

// Global state
static std::atomic<bool> g_running{false};
static std::atomic<double> g_hashrate{0.0};
static std::atomic<uint64_t> g_totalHashes{0};
static std::atomic<uint64_t> g_sharesFound{0};
static int g_threads = 2;

static xmrig::StratumClient* g_stratum = nullptr;
static xmrig::Job g_currentJob;
static std::mutex g_jobMutex;

// Current RandomX seed hash
static std::string g_currentSeedHash;
static std::mutex g_seedMutex;

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

// Reverse byte order (big-endian to little-endian)
uint64_t swap64(uint64_t val) {
    return ((val & 0x00000000000000FFULL) << 56) |
           ((val & 0x000000000000FF00ULL) << 40) |
           ((val & 0x0000000000FF0000ULL) << 24) |
           ((val & 0x00000000FF000000ULL) << 8)  |
           ((val & 0x000000FF00000000ULL) >> 8)  |
           ((val & 0x0000FF0000000000ULL) >> 24) |
           ((val & 0x00FF000000000000ULL) >> 40) |
           ((val & 0xFF00000000000000ULL) >> 56);
}

// Initialize RandomX with current seed hash
bool initRandomXForCurrentSeed() {
    std::lock_guard<std::mutex> lock(g_seedMutex);
    
    if (g_currentSeedHash.empty()) {
        EM_ASM({ console.error("[WASM Miner] No seed hash available"); });
        return false;
    }
    
    // Convert seed hash from hex to bytes
    std::string seedBytes = hexToBytes(g_currentSeedHash);
    if (seedBytes.length() != 32) {
        EM_ASM_({
            console.error("[WASM Miner] Invalid seed hash length:", $0);
        }, (int)seedBytes.length());
        return false;
    }
    
    EM_ASM_({
        console.log("[WASM Miner] Initializing RandomX with seed hash:", UTF8ToString($0));
    }, g_currentSeedHash.c_str());
    
    // Initialize RandomX with the seed hash as key
    // This allocates ~256MB for the cache
    bool success = xmrig_wasm::randomx_wasm_init(
        reinterpret_cast<const uint8_t*>(seedBytes.data()), 
        seedBytes.length()
    );
    
    if (!success) {
        EM_ASM({ console.error("[WASM Miner] Failed to initialize RandomX!"); });
    }
    
    return success;
}

// Mining thread
static void miningThread(int threadId) {
    uint8_t hash[32];
    
    // Initialize RandomX for this thread
    if (!xmrig_wasm::randomx_wasm_is_initialized()) {
        if (!initRandomXForCurrentSeed()) {
            EM_ASM_({
                console.error("[WASM Miner] Thread", $0, "failed to initialize RandomX");
            }, threadId);
            return;
        }
    }
    
    uint32_t nonce = threadId * 0x10000000;
    uint64_t localHashes = 0;
    auto startTime = std::chrono::steady_clock::now();
    
    std::string lastSeedHash;
    
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
        
        // Check if seed hash changed (new block)
        {
            std::lock_guard<std::mutex> lock(g_seedMutex);
            if (g_currentSeedHash != lastSeedHash) {
                // Reinitialize RandomX with new seed
                xmrig_wasm::randomx_wasm_destroy();
                if (!initRandomXForCurrentSeed()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    continue;
                }
                lastSeedHash = g_currentSeedHash;
                
                EM_ASM_({
                    console.log("[WASM Miner] Thread", $0, "reinitialized for new block");
                }, threadId);
            }
        }
        
        // Convert blob to bytes
        std::string blobBytes = hexToBytes(job.blob);
        if (blobBytes.length() < 43) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        
        // Set nonce in blob (bytes 39-42)
        *reinterpret_cast<uint32_t*>(&blobBytes[39]) = nonce++;
        
        // Calculate hash using real RandomX
        xmrig_wasm::randomx_wasm_hash(
            reinterpret_cast<const uint8_t*>(blobBytes.data()), 
            blobBytes.length(), 
            hash
        );
        
        localHashes++;
        g_totalHashes.fetch_add(1);
        
        // Check if hash meets target (RandomX outputs are in little-endian)
        // Compare first 8 bytes (64 bits) of hash against target
        uint64_t hashValue = *reinterpret_cast<uint64_t*>(hash);
        uint64_t targetValue = hexToUint64(job.target);
        
        // Hashes are compared as little-endian values
        // A valid share has hash < target (as unsigned integers)
        if (hashValue < targetValue) {
            // Share found!
            uint32_t foundNonce = nonce - 1;
            std::string nonceHex = bytesToHex(reinterpret_cast<uint8_t*>(&foundNonce), 4);
            std::string resultHex = bytesToHex(hash, 32);
            
            EM_ASM_({
                console.log("[WASM Miner] SHARE FOUND! Nonce:", UTF8ToString($0));
                console.log("[WASM Miner] Hash:", UTF8ToString($1));
            }, nonceHex.c_str(), resultHex.c_str());
            
            // Submit share
            if (g_stratum) {
                g_stratum->submit(job.id, nonceHex, resultHex);
            }
            
            g_sharesFound.fetch_add(1);
        }
        
        // Update hashrate periodically
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration<double>(now - startTime).count();
        if (elapsed >= 1.0) {
            double rate = localHashes / elapsed;
            g_hashrate.store(rate);
            localHashes = 0;
            startTime = now;
        }
    }
    
    // Cleanup RandomX for this thread
    xmrig_wasm::randomx_wasm_destroy();
}

// Job callback
static void onNewJob(const xmrig::Job& job) {
    {
        std::lock_guard<std::mutex> lock(g_jobMutex);
        g_currentJob = job;
    }
    
    // Update seed hash
    {
        std::lock_guard<std::mutex> lock(g_seedMutex);
        g_currentSeedHash = job.seedHash;
    }
    
    EM_ASM_({
        console.log("[WASM Miner] New job received, height:", $0, 
                    "diff:", $1, "seed:", UTF8ToString($2));
        if (typeof window !== 'undefined' && window.onJobReceived) {
            window.onJobReceived($0, $1, UTF8ToString($2));
        }
    }, (int)job.height, (int)job.difficulty, job.seedHash.c_str());
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
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    std::string worker = "wasm-miner-" + std::to_string(std::rand() % 1000);
    g_stratum->login(wallet, worker);
    
    // Wait for first job to get seed hash
    int attempts = 0;
    while (g_currentSeedHash.empty() && attempts < 20) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        attempts++;
    }
    
    if (g_currentSeedHash.empty()) {
        EM_ASM({ console.error("[WASM Miner] No job received from pool"); });
        return -3;
    }
    
    // Start mining threads
    g_threads = threads;
    g_running = true;
    
    for (int i = 0; i < threads; i++) {
        std::thread(miningThread, i).detach();
    }
    
    EM_ASM_({
        console.log("[WASM Miner] Started with " + $0 + " threads using real RandomX");
    }, threads);
    
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

EMSCRIPTEN_KEEPALIVE
int wasmRandomXTest(const char* seedHex, const char* dataHex) {
    // Test function to verify RandomX is working
    std::string seedBytes = hexToBytes(std::string(seedHex));
    std::string dataBytes = hexToBytes(std::string(dataHex));
    uint8_t hash[32];
    
    EM_ASM({ console.log("[WASM Miner] Testing RandomX..."); });
    
    // Initialize with seed
    bool success = xmrig_wasm::randomx_wasm_init(
        reinterpret_cast<const uint8_t*>(seedBytes.data()),
        seedBytes.length()
    );
    
    if (!success) {
        EM_ASM({ console.error("[WASM Miner] RandomX init failed"); });
        return -1;
    }
    
    // Calculate hash
    xmrig_wasm::randomx_wasm_hash(
        reinterpret_cast<const uint8_t*>(dataBytes.data()),
        dataBytes.length(),
        hash
    );
    
    std::string hashHex = bytesToHex(hash, 32);
    
    EM_ASM_({
        console.log("[WASM Miner] RandomX test hash:", UTF8ToString($0));
    }, hashHex.c_str());
    
    xmrig_wasm::randomx_wasm_destroy();
    return 0;
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
    emscripten::function("testRandomX", &wasmRandomXTest,
                         emscripten::allow_raw_pointers());
}

int main() {
    EM_ASM({
        console.log("[XMRig-WASM] Module loaded successfully with RandomX support");
        console.log("[XMRig-WASM] Ready to mine");
    });
    return 0;
}
