/*
 * Stratum Client for WebAssembly
 * Connects to mining pool via WebSocket
 */

#ifndef STRATUM_CLIENT_H
#define STRATUM_CLIENT_H

#include <emscripten.h>
#include <emscripten/websocket.h>
#include <string>
#include <functional>
#include <atomic>
#include <mutex>
#include "json/simple_json.h"

namespace xmrig {

struct Job {
    std::string id;
    std::string blob;
    std::string target;
    uint64_t height = 0;
    uint64_t difficulty = 0;
    std::string seedHash;
    
    bool isValid() const {
        return !blob.empty() && !target.empty();
    }
};

class StratumClient {
public:
    using JobCallback = std::function<void(const Job&)>;
    using ConnectedCallback = std::function<void()>;
    using DisconnectedCallback = std::function<void()>;
    using ErrorCallback = std::function<void(const std::string&)>;

    StratumClient();
    ~StratumClient();

    bool connect(const std::string& url);
    void disconnect();
    bool isConnected() const;

    void login(const std::string& wallet, const std::string& worker, 
               const std::string& password = "x");

    void submit(const std::string& jobId, const std::string& nonce, 
                const std::string& result);

    void onJob(JobCallback cb) { jobCallback_ = cb; }
    void onConnected(ConnectedCallback cb) { connectedCallback_ = cb; }
    void onDisconnected(DisconnectedCallback cb) { disconnectedCallback_ = cb; }
    void onError(ErrorCallback cb) { errorCallback_ = cb; }

    const Job& currentJob() const { return currentJob_; }

private:
    EMSCRIPTEN_WEBSOCKET_T socket_;
    std::atomic<bool> connected_{false};
    std::atomic<bool> authenticated_{false};
    
    std::string poolUrl_;
    std::string wallet_;
    std::string worker_;
    
    Job currentJob_;
    int64_t rpcId_;
    
    JobCallback jobCallback_;
    ConnectedCallback connectedCallback_;
    DisconnectedCallback disconnectedCallback_;
    ErrorCallback errorCallback_;

    static EM_BOOL onOpen(int eventType, const EmscriptenWebSocketOpenEvent* e, void* userData);
    static EM_BOOL onMessage(int eventType, const EmscriptenWebSocketMessageEvent* e, void* userData);
    static EM_BOOL onError(int eventType, const EmscriptenWebSocketErrorEvent* e, void* userData);
    static EM_BOOL onClose(int eventType, const EmscriptenWebSocketCloseEvent* e, void* userData);

    void handleMessage(const std::string& data);
    void handleResponse(const simple_json::Value& response);
    void handleNotification(const simple_json::Value& notification);
    void parseJob(const simple_json::Value& params);

    void send(const simple_json::Value& msg);
    void sendRaw(const std::string& json);
};

} // namespace xmrig

#endif // STRATUM_CLIENT_H
