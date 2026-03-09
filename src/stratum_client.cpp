/*
 * Stratum Client Implementation
 */

#include "stratum_client.h"
#include <cstring>
#include <sstream>

namespace xmrig {

StratumClient::StratumClient() 
    : socket_(0)
    , rpcId_(0)
{
}

StratumClient::~StratumClient() {
    disconnect();
}

bool StratumClient::connect(const std::string& url) {
    if (connected_) {
        return true;
    }

    poolUrl_ = url;
    
    std::string wsUrl = url;
    if (wsUrl.find("://") == std::string::npos) {
        wsUrl = "wss://" + wsUrl;
    }

    EmscriptenWebSocketCreateAttributes attr;
    emscripten_websocket_init_create_attributes(&attr);
    attr.url = wsUrl.c_str();
    
    socket_ = emscripten_websocket_new(&attr);
    if (socket_ <= 0) {
        if (errorCallback_) {
            errorCallback_("Failed to create WebSocket");
        }
        return false;
    }

    emscripten_websocket_set_onopen_callback(socket_, this, onOpen);
    emscripten_websocket_set_onmessage_callback(socket_, this, onMessage);
    emscripten_websocket_set_onerror_callback(socket_, this, onError);
    emscripten_websocket_set_onclose_callback(socket_, this, onClose);

    return true;
}

void StratumClient::disconnect() {
    if (socket_ > 0) {
        emscripten_websocket_close(socket_, 1000, "User disconnect");
        emscripten_websocket_delete(socket_);
        socket_ = 0;
    }
    connected_ = false;
    authenticated_ = false;
}

bool StratumClient::isConnected() const {
    return connected_;
}

void StratumClient::login(const std::string& wallet, const std::string& worker, 
                          const std::string& password) {
    wallet_ = wallet;
    worker_ = worker;

    std::stringstream json;
    json << "{";
    json << "\"id\":" << (++rpcId_) << ",";
    json << "\"jsonrpc\":\"2.0\",";
    json << "\"method\":\"login\",";
    json << "\"params\":{";
    json << "\"login\":\"" << wallet << "\",";
    json << "\"pass\":\"" << password << "\",";
    json << "\"agent\":\"xmrig-wasm/1.0\",";
    json << "\"rigid\":\"" << worker << "\"";
    json << "}}";
    
    sendRaw(json.str());
}

void StratumClient::submit(const std::string& jobId, const std::string& nonce, 
                           const std::string& result) {
    if (!connected_ || !authenticated_) {
        return;
    }

    std::stringstream json;
    json << "{";
    json << "\"id\":" << (++rpcId_) << ",";
    json << "\"jsonrpc\":\"2.0\",";
    json << "\"method\":\"submit\",";
    json << "\"params\":{";
    json << "\"id\":\"" << jobId << "\",";
    json << "\"job_id\":\"" << jobId << "\",";
    json << "\"nonce\":\"" << nonce << "\",";
    json << "\"result\":\"" << result << "\"";
    json << "}}";
    
    sendRaw(json.str());
}

EM_BOOL StratumClient::onOpen(int eventType, const EmscriptenWebSocketOpenEvent* e, 
                               void* userData) {
    StratumClient* client = static_cast<StratumClient*>(userData);
    client->connected_ = true;
    
    EM_ASM({ console.log("[Stratum] WebSocket connected"); });
    
    if (client->connectedCallback_) {
        client->connectedCallback_();
    }
    
    return EM_TRUE;
}

EM_BOOL StratumClient::onMessage(int eventType, const EmscriptenWebSocketMessageEvent* e, 
                                  void* userData) {
    StratumClient* client = static_cast<StratumClient*>(userData);
    
    if (e->isText) {
        std::string message(reinterpret_cast<const char*>(e->data), e->numBytes);
        client->handleMessage(message);
    }
    
    return EM_TRUE;
}

EM_BOOL StratumClient::onError(int eventType, const EmscriptenWebSocketErrorEvent* e, 
                                void* userData) {
    StratumClient* client = static_cast<StratumClient*>(userData);
    
    EM_ASM({ console.error("[Stratum] WebSocket error"); });
    
    if (client->errorCallback_) {
        client->errorCallback_("WebSocket error");
    }
    
    return EM_TRUE;
}

EM_BOOL StratumClient::onClose(int eventType, const EmscriptenWebSocketCloseEvent* e, 
                                void* userData) {
    StratumClient* client = static_cast<StratumClient*>(userData);
    client->connected_ = false;
    client->authenticated_ = false;
    
    EM_ASM_({
        console.log("[Stratum] WebSocket closed, code:", $0);
    }, e->code);
    
    if (client->disconnectedCallback_) {
        client->disconnectedCallback_();
    }
    
    return EM_TRUE;
}

void StratumClient::handleMessage(const std::string& data) {
    simple_json::Parser parser;
    simple_json::Value root;
    
    if (!parser.parse(data, root)) {
        EM_ASM({ console.error("[Stratum] Failed to parse JSON"); });
        return;
    }
    
    if (root.isMember("method")) {
        handleNotification(root);
    } else if (root.isMember("result")) {
        handleResponse(root);
    }
}

void StratumClient::handleResponse(const simple_json::Value& response) {
    const simple_json::Value& result = response["result"];
    
    if (result.isMember("status")) {
        std::string status = result["status"].asString();
        if (status == "OK") {
            authenticated_ = true;
            EM_ASM({ console.log("[Stratum] Login successful"); });
            
            if (result.isMember("job")) {
                parseJob(result["job"]);
            }
        }
    }
}

void StratumClient::handleNotification(const simple_json::Value& notification) {
    std::string method = notification["method"].asString();
    
    if (method == "job") {
        parseJob(notification["params"]);
    }
}

void StratumClient::parseJob(const simple_json::Value& params) {
    currentJob_.id = params["job_id"].asString();
    currentJob_.blob = params["blob"].asString();
    currentJob_.target = params["target"].asString();
    currentJob_.seedHash = params["seed_hash"].asString();
    
    if (params.isMember("height")) {
        currentJob_.height = params["height"].asUInt64();
    }
    
    if (currentJob_.target.length() >= 8) {
        std::string targetPrefix = currentJob_.target.substr(0, 8);
        uint32_t targetValue = std::strtoul(targetPrefix.c_str(), nullptr, 16);
        if (targetValue > 0) {
            currentJob_.difficulty = 0xFFFFFFFFULL / targetValue;
        }
    }
    
    EM_ASM_({
        console.log("[Stratum] New job - Height:", $0, "Diff:", $1);
    }, (int)currentJob_.height, (int)currentJob_.difficulty);
    
    if (jobCallback_) {
        jobCallback_(currentJob_);
    }
}

void StratumClient::sendRaw(const std::string& json) {
    if (!connected_) {
        return;
    }
    
    emscripten_websocket_send_utf8_text(socket_, json.c_str());
}

} // namespace xmrig
