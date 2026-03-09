/**
 * XMRig WebWorker
 * Runs the actual mining in a background thread
 */

self.importScripts('xmrig-wasm.js');

let miner = null;
let isRunning = false;

self.onmessage = function(e) {
    const { type, config } = e.data;
    
    switch (type) {
        case 'start':
            startMining(config);
            break;
        case 'stop':
            stopMining();
            break;
    }
};

async function startMining(config) {
    if (isRunning) return;
    
    self.postMessage({ type: 'log', message: 'Worker starting...' });
    
    // Initialize WASM module
    const module = await XMRigModule({
        print: (text) => {
            self.postMessage({ type: 'log', message: text });
            parseOutput(text);
        },
        printErr: (text) => {
            self.postMessage({ type: 'error', error: text });
        }
    });
    
    // Start mining with configuration
    const args = [
        'xmrig',
        '-o', config.pool.replace('wss://', 'ws://').replace('ws://', ''),  // Remove protocol
        '-u', config.wallet,
        '-p', config.password,
        '-t', config.threads.toString(),
        '--worker-id', config.worker,
        '--donate-level', '1',
        '--print-time', '10',
        '--algo', 'rx/0'
    ];
    
    module.callMain(args);
    isRunning = true;
}

function stopMining() {
    isRunning = false;
    self.postMessage({ type: 'log', message: 'Worker stopping...' });
}

function parseOutput(text) {
    // Parse hashrate
    const hashrateMatch = text.match(/speed\s+10s\/60s\/15m\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)\s+H\/s/);
    if (hashrateMatch) {
        self.postMessage({
            type: 'hashrate',
            hashrate: parseFloat(hashrateMatch[1]),
            totalHashes: 0  // Would need to track this separately
        });
    }
    
    // Parse accepted shares
    if (text.includes('accepted')) {
        self.postMessage({ type: 'shareAccepted' });
    }
    
    // Parse rejected shares
    if (text.includes('rejected')) {
        self.postMessage({ type: 'shareRejected' });
    }
}
