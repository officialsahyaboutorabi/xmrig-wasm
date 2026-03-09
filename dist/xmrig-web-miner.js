/**
 * XMRig WebAssembly Miner - JavaScript API
 * 
 * Usage:
 *   const miner = new XMRigWebMiner();
 *   await miner.init();
 *   await miner.start({
 *     pool: 'wss://wrxproxy.qzz.io',
 *     wallet: 'YOUR_WALLET_ADDRESS',
 *     threads: 2
 *   });
 */

class XMRigWebMiner extends EventTarget {
    constructor() {
        super();
        this.module = null;
        this.isRunning = false;
        this.statsInterval = null;
        this.config = {
            pool: 'wss://wrxproxy.qzz.io',
            wallet: '',
            worker: 'web-miner',
            threads: 2
        };
        this.stats = {
            hashrate: 0,
            totalHashes: 0,
            sharesFound: 0,
            difficulty: 0,
            connectionStatus: 0 // 0=disconnected, 1=connecting, 2=connected
        };
    }

    /**
     * Initialize the WASM module
     */
    async init() {
        if (this.module) return;

        return new Promise((resolve, reject) => {
            // Check if XMRigModule is available
            if (typeof XMRigModule === 'undefined') {
                reject(new Error('XMRigModule not loaded. Include xmrig-wasm.js first.'));
                return;
            }

            XMRigModule()
                .then(module => {
                    this.module = module;
                    this._setupCallbacks();
                    this._emit('ready');
                    resolve();
                })
                .catch(reject);
        });
    }

    /**
     * Start mining
     */
    async start(options = {}) {
        if (!this.module) {
            await this.init();
        }

        if (this.isRunning) {
            throw new Error('Miner is already running');
        }

        this.config = { ...this.config, ...options };

        if (!this.config.wallet || this.config.wallet.length < 95) {
            throw new Error('Valid Monero wallet address required (95 characters)');
        }

        this._emit('starting', this.config);

        const result = this.module.ccall('wasmStartMining', 'number', 
            ['string', 'string', 'number'],
            [this.config.pool, this.config.wallet, this.config.threads]
        );

        if (result !== 0) {
            const errors = {
                '-1': 'Miner already running',
                '-2': 'Failed to connect to pool',
                '-3': 'No job received from pool'
            };
            throw new Error(errors[result] || `Failed to start (code: ${result})`);
        }

        this.isRunning = true;
        this._startStatsLoop();
        this._emit('started');

        return true;
    }

    /**
     * Stop mining
     */
    stop() {
        if (!this.isRunning) return;

        this.module.ccall('wasmStopMining', null, [], []);
        this.isRunning = false;
        
        if (this.statsInterval) {
            clearInterval(this.statsInterval);
            this.statsInterval = null;
        }

        this._updateStats();
        this._emit('stopped');
    }

    /**
     * Get current statistics
     */
    getStats() {
        if (this.module && this.isRunning) {
            this._updateStats();
        }
        return { ...this.stats };
    }

    /**
     * Test RandomX implementation
     */
    async testRandomX(seedHex, dataHex) {
        if (!this.module) {
            await this.init();
        }

        return new Promise((resolve, reject) => {
            const result = this.module.ccall('wasmRandomXTest', 'number',
                ['string', 'string'],
                [seedHex, dataHex]
            );

            if (result === 0) {
                resolve(true);
            } else {
                reject(new Error('RandomX test failed'));
            }
        });
    }

    /**
     * Check if miner is running
     */
    get running() {
        return this.isRunning;
    }

    // Private methods
    _setupCallbacks() {
        // Expose global callbacks for WASM
        window.onJobReceived = (height, difficulty, seedHash) => {
            this.stats.difficulty = difficulty;
            this._emit('job', { height, difficulty, seedHash });
        };

        window.onShareFound = (nonce) => {
            this.stats.sharesFound++;
            this._emit('share', { nonce });
        };
    }

    _startStatsLoop() {
        this.statsInterval = setInterval(() => {
            this._updateStats();
        }, 1000);
    }

    _updateStats() {
        if (!this.module) return;

        this.stats.hashrate = this.module.ccall('wasmGetHashrate', 'number', [], []);
        this.stats.totalHashes = this.module.ccall('wasmGetTotalHashes', 'number', [], []);
        this.stats.sharesFound = this.module.ccall('wasmGetSharesFound', 'number', [], []);
        this.stats.connectionStatus = this.module.ccall('wasmGetConnectionStatus', 'number', [], []);

        this._emit('stats', this.stats);
    }

    _emit(eventName, data) {
        this.dispatchEvent(new CustomEvent(eventName, { detail: data }));
    }
}

// Simple API for quick integration
const XMRigWeb = {
    miner: null,

    async create(options = {}) {
        const miner = new XMRigWebMiner();
        await miner.init();
        
        if (options.autoStart) {
            await miner.start(options);
        }
        
        this.miner = miner;
        return miner;
    },

    get isRunning() {
        return this.miner?.running || false;
    },

    get stats() {
        return this.miner?.getStats() || null;
    }
};

// Export for module systems
if (typeof module !== 'undefined' && module.exports) {
    module.exports = { XMRigWebMiner, XMRigWeb };
}
