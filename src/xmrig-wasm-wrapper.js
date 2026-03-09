/**
 * XMRig WebAssembly Wrapper
 * JavaScript interface for XMRig-WASM
 */

class XMRigWASM {
    constructor(config = {}) {
        this.config = {
            pool: config.pool || 'wss://wrxproxy.qzz.io',
            wallet: config.wallet || '',
            worker: config.worker || 'web-miner-' + Math.random().toString(36).substr(2, 6),
            threads: config.threads || navigator.hardwareConcurrency || 4,
            password: config.password || 'x',
            ...config
        };
        
        this.module = null;
        this.worker = null;
        this.isRunning = false;
        this.hashrate = 0;
        this.totalHashes = 0;
        this.sharesAccepted = 0;
        this.sharesRejected = 0;
        
        this.onHashrate = null;
        this.onShareAccepted = null;
        this.onShareRejected = null;
        this.onError = null;
    }
    
    async init() {
        console.log('[XMRig-WASM] Initializing...');
        
        // Load WASM module
        const wasmResponse = await fetch('xmrig-wasm.wasm');
        const wasmBinary = await wasmResponse.arrayBuffer();
        
        // Import the WASM module
        const { default: XMRigModule } = await import('./xmrig-wasm.js');
        
        this.module = await XMRigModule({
            wasmBinary: wasmBinary,
            print: (text) => this.handleOutput(text),
            printErr: (text) => this.handleError(text),
            locateFile: (path) => {
                if (path.endsWith('.wasm')) return 'xmrig-wasm.wasm';
                return path;
            }
        });
        
        console.log('[XMRig-WASM] Initialized successfully');
        return true;
    }
    
    start() {
        if (this.isRunning) {
            console.warn('[XMRig-WASM] Already running');
            return;
        }
        
        console.log('[XMRig-WASM] Starting miner...');
        console.log(`  Pool: ${this.config.pool}`);
        console.log(`  Worker: ${this.config.worker}`);
        console.log(`  Threads: ${this.config.threads}`);
        
        // Create mining worker
        this.worker = new Worker('xmrig-worker.js');
        this.worker.postMessage({
            type: 'start',
            config: this.config
        });
        
        this.worker.onmessage = (e) => {
            this.handleWorkerMessage(e.data);
        };
        
        this.isRunning = true;
    }
    
    stop() {
        if (!this.isRunning) return;
        
        console.log('[XMRig-WASM] Stopping miner...');
        
        if (this.worker) {
            this.worker.postMessage({ type: 'stop' });
            this.worker.terminate();
            this.worker = null;
        }
        
        this.isRunning = false;
        this.hashrate = 0;
    }
    
    handleWorkerMessage(data) {
        switch (data.type) {
            case 'hashrate':
                this.hashrate = data.hashrate;
                this.totalHashes = data.totalHashes;
                if (this.onHashrate) {
                    this.onHashrate(data.hashrate, data.totalHashes);
                }
                break;
                
            case 'shareAccepted':
                this.sharesAccepted++;
                if (this.onShareAccepted) {
                    this.onShareAccepted(this.sharesAccepted, this.sharesRejected);
                }
                break;
                
            case 'shareRejected':
                this.sharesRejected++;
                if (this.onShareRejected) {
                    this.onShareRejected(this.sharesAccepted, this.sharesRejected);
                }
                break;
                
            case 'error':
                console.error('[XMRig-WASM]', data.error);
                if (this.onError) {
                    this.onError(data.error);
                }
                break;
                
            case 'log':
                console.log('[XMRig-WASM]', data.message);
                break;
        }
    }
    
    handleOutput(text) {
        // Parse XMRig output
        if (text.includes('speed')) {
            // Parse hashrate
            const match = text.match(/(\d+\.?\d*)\s*H\/s/);
            if (match) {
                this.hashrate = parseFloat(match[1]);
            }
        }
        console.log('[XMRig]', text);
    }
    
    handleError(text) {
        console.error('[XMRig]', text);
    }
    
    getStats() {
        return {
            isRunning: this.isRunning,
            hashrate: this.hashrate,
            totalHashes: this.totalHashes,
            sharesAccepted: this.sharesAccepted,
            sharesRejected: this.sharesRejected,
            threads: this.config.threads
        };
    }
}

// Export for use
if (typeof module !== 'undefined' && module.exports) {
    module.exports = XMRigWASM;
} else {
    window.XMRigWASM = XMRigWASM;
}
