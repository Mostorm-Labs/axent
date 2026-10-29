#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include <ixwebsocket/IXGetFreePort.h>
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketMessage.h>
#include <ixwebsocket/IXWebSocketMessageType.h>
#include <ixwebsocket/IXWebSocketServer.h>
#include <nlohmann/json.hpp>

#include "core/runtime/transport/transport.hpp"

namespace axent::transport {

using axtp::AxtpWireMode;
using axtp::Byte;
using axtp::IByteSink;
using axtp::ITransport;
using axtp::RpcEncoding;
using axtp::TransportKind;
using axtp::TransportProfile;

class WebSocketTransport : public ITransport {
public:
    explicit WebSocketTransport(std::uint16_t port, const char* address = "127.0.0.1")
        : _port(port)
        , _address(address != nullptr ? address : "127.0.0.1") {}

    ~WebSocketTransport() override {
        close();
    }

    void bind(IByteSink& sink) override {
        _sink = &sink;
    }

    void open() override {
        close();
        _netInitialized = ix::initNetSystem();
        const auto listenPort = _port == 0 ? ix::getFreePort() : static_cast<int>(_port);
        if (listenPort <= 0) {
            return;
        }
        _localPort = static_cast<std::uint16_t>(listenPort);
        _server = std::make_unique<ix::WebSocketServer>(listenPort, _address);
        _server->disablePerMessageDeflate();
        _server->setOnClientMessageCallback(
            [this](std::shared_ptr<ix::ConnectionState> connectionState,
                   ix::WebSocket& webSocket,
                   const ix::WebSocketMessagePtr& message) {
                if (!message) {
                    return;
                }
                if (message->type == ix::WebSocketMessageType::Open) {
                    auto client = findClient(webSocket);
                    {
                        std::lock_guard<std::mutex> lock(_clientsMutex);
                        const bool firstClient = _clients.empty();
                        const auto id = connectionId(connectionState);
                        removeReplyTargetLocked(id);
                        _clients[id] = std::move(client);
                        const auto reply_target = nextReplyTargetLocked();
                        _replyTargets[id] = reply_target;
                        _connectionIdsByReplyTarget[reply_target] = id;
                        _pendingHelloClients.push(id);
                        if (firstClient) {
                            // The runtime adapter owns one shared AXTP session.  Only the
                            // empty-to-connected transition starts a new session generation;
                            // extra WebSocket peers join the active session.
                            _helloText.clear();
                            _connectionGeneration.fetch_add(1, std::memory_order_relaxed);
                        }
                        _hasConnection.store(true);
                    }
                    runConnectionChangedHookForTesting();
                    return;
                }
                if (message->type == ix::WebSocketMessageType::Close ||
                    message->type == ix::WebSocketMessageType::Error) {
                    {
                        std::lock_guard<std::mutex> lock(_clientsMutex);
                        const auto id = connectionId(connectionState);
                        removeReplyTargetLocked(id);
                        _clients.erase(id);
                        _hasConnection.store(!_clients.empty());
                    }
                    runConnectionChangedHookForTesting();
                    return;
                }
                if (message->type != ix::WebSocketMessageType::Message || message->binary) {
                    return;
                }
                const auto id = connectionId(connectionState);
                std::uint64_t reply_target = 0;
                {
                    std::lock_guard<std::mutex> lock(_clientsMutex);
                    if (const auto target = _replyTargets.find(id); target != _replyTargets.end()) {
                        reply_target = target->second;
                    }
                }
                bool overflow=false;
                {
                    std::lock_guard<std::mutex> lock(_rxMutex);
                    overflow=message->str.size()>65536 || _rxMessages.size()>=1024 ||
                        message->str.size()>kQueueBytes-_rxBytes;
                    if(!overflow){_rxBytes+=message->str.size();_rxMessages.push(ReceivedMessage{id, reply_target, message->str});}
                }
                if(overflow){webSocket.close(1013,"control request backpressure");return;}
                runMessageQueuedHookForTesting();
            });
        const bool started = _server->listenAndStart();
        _open.store(started);
        if (!started) {
            _server.reset();
            _localPort = 0;
        }
    }

    void close() override {
        if (_server) {
            _server->stop();
            _server.reset();
        }
        _localPort = 0;
        _hasConnection.store(false);
        {
            std::lock_guard<std::mutex> lock(_clientsMutex);
            _clients.clear();
            _replyTargets.clear();
            _connectionIdsByReplyTarget.clear();
            std::queue<std::string> empty;
            _pendingHelloClients.swap(empty);
            _helloText.clear();
        }
        {
            std::lock_guard<std::mutex> lock(_rxMutex);
            std::queue<ReceivedMessage> empty;
            _rxMessages.swap(empty);
            _rxBytes=0;
        }
        _open.store(false);
        if (_netInitialized) {
            ix::uninitNetSystem();
            _netInitialized = false;
        }
    }

    void disconnectClients() {
        const auto clients = activeClients();
        for (const auto& client : clients) if (client) client->close(1013, "control backpressure; resynchronize");
    }

    void poll() override {
        if (!_open.load() || _sink == nullptr) {
            return;
        }
        if (_polling.exchange(true)) {
            return;
        }

        struct PollGuard {
            explicit PollGuard(std::atomic<bool>& polling)
                : polling_(polling) {}
            ~PollGuard() { polling_.store(false); }
            std::atomic<bool>& polling_;
        } guard(_polling);

        sendCachedHelloToPendingClients();

        while (true) {
            ReceivedMessage message;
            {
                std::lock_guard<std::mutex> lock(_rxMutex);
                if (_rxMessages.empty()) {
                    _dispatchConnectionId.clear();
                    _dispatchReplyTarget = 0;
                    return;
                }
                message = std::move(_rxMessages.front());
                _rxBytes-=message.text.size();
                _rxMessages.pop();
            }
            runBeforeDispatchHookForTesting();
            _dispatchConnectionId = message.connectionId;
            _dispatchReplyTarget = message.replyTarget;
            struct DispatchGuard {
                DispatchGuard(std::string& connectionId, std::uint64_t& replyTarget)
                    : connectionId_(connectionId)
                    , replyTarget_(replyTarget) {}
                ~DispatchGuard() {
                    connectionId_.clear();
                    replyTarget_ = 0;
                }
                std::string& connectionId_;
                std::uint64_t& replyTarget_;
            } dispatchGuard(_dispatchConnectionId, _dispatchReplyTarget);
            _sink->onBytes(
                reinterpret_cast<const Byte*>(message.text.data()), message.text.size());
        }
    }

    void sendBytes(const Byte* data, std::size_t size) override {
        if (!_server || data == nullptr || size == 0) {
            return;
        }
        const std::string text(reinterpret_cast<const char*>(data), size);

        if (isHello(text)) {
            std::vector<std::shared_ptr<ix::WebSocket>> clients;
            {
                std::lock_guard<std::mutex> lock(_clientsMutex);
                _helloText = text;
                clients = takePendingHelloClientsLocked();
            }
            if (!clients.empty()) {
                sendText(clients, text);
                return;
            }
        }

        if (!_dispatchConnectionId.empty()) {
            auto client = clientFor(_dispatchConnectionId);
            if (client) {
                sendBounded(client,text);
            }
            return;
        }

        sendText(activeClients(), text);
    }

    std::uint64_t currentReplyTarget() const override {
        return _dispatchReplyTarget;
    }

    void sendBytesTo(std::uint64_t replyTarget,
                     const Byte* data,
                     std::size_t size) override {
        if (!_server || replyTarget == 0 || data == nullptr || size == 0) {
            return;
        }
        std::shared_ptr<ix::WebSocket> client;
        {
            std::lock_guard<std::mutex> lock(_clientsMutex);
            const auto id = _connectionIdsByReplyTarget.find(replyTarget);
            if (id == _connectionIdsByReplyTarget.end()) {
                return;
            }
            const auto current = _clients.find(id->second);
            if (current != _clients.end()) {
                client = current->second;
            }
        }
        if (client) {
            const std::string text(reinterpret_cast<const char*>(data), size);
            sendBounded(client,text);
        }
    }

    TransportProfile profile() const override {
        TransportProfile profile;
        profile.kind = TransportKind::WebSocket;
        profile.wireMode = AxtpWireMode::WebSocketJsonRpc;
        profile.defaultRpcEncoding = RpcEncoding::Json;
        profile.messageOriented = true;
        profile.supportsTextMessage = true;
        profile.supportsBinaryMessage = false;
        return profile;
    }

    std::uint16_t localPort() const {
        return _localPort;
    }

    bool hasConnection() const {
        return _server != nullptr && _hasConnection.load();
    }

    std::uint64_t connectionGeneration() const {
        return _connectionGeneration.load(std::memory_order_relaxed);
    }

    void setBeforeDispatchHookForTesting(std::function<void()> hook) {
        _beforeDispatchHookForTesting = std::move(hook);
    }

    void setConnectionChangedHookForTesting(std::function<void()> hook) {
        _connectionChangedHookForTesting = std::move(hook);
    }

    void setMessageQueuedHookForTesting(std::function<void()> hook) {
        _messageQueuedHookForTesting = std::move(hook);
    }

private:
    struct ReceivedMessage {
        std::string connectionId;
        std::uint64_t replyTarget = 0;
        std::string text;
    };

    static std::string connectionId(const std::shared_ptr<ix::ConnectionState>& connectionState) {
        return connectionState ? connectionState->getId() : std::string();
    }

    void removeReplyTargetLocked(const std::string& id) {
        const auto target = _replyTargets.find(id);
        if (target == _replyTargets.end()) {
            return;
        }
        _connectionIdsByReplyTarget.erase(target->second);
        _replyTargets.erase(target);
    }

    std::uint64_t nextReplyTargetLocked() {
        while (true) {
            const auto target = _nextReplyTarget++;
            if (target != 0 && _connectionIdsByReplyTarget.count(target) == 0) {
                return target;
            }
        }
    }

    void runBeforeDispatchHookForTesting() {
        if (_beforeDispatchHookForTesting) {
            _beforeDispatchHookForTesting();
        }
    }

    void runConnectionChangedHookForTesting() {
        if (_connectionChangedHookForTesting) {
            _connectionChangedHookForTesting();
        }
    }

    void runMessageQueuedHookForTesting() {
        if (_messageQueuedHookForTesting) {
            _messageQueuedHookForTesting();
        }
    }

    std::shared_ptr<ix::WebSocket> findClient(ix::WebSocket& webSocket) const {
        if (!_server) {
            return {};
        }
        for (const auto& client : _server->getClients()) {
            if (client && client.get() == &webSocket) {
                return client;
            }
        }
        return {};
    }

    std::shared_ptr<ix::WebSocket> clientFor(const std::string& id) const {
        std::lock_guard<std::mutex> lock(_clientsMutex);
        const auto client = _clients.find(id);
        return client == _clients.end() ? std::shared_ptr<ix::WebSocket>{} : client->second;
    }

    std::vector<std::shared_ptr<ix::WebSocket>> activeClients() const {
        std::vector<std::shared_ptr<ix::WebSocket>> clients;
        std::lock_guard<std::mutex> lock(_clientsMutex);
        clients.reserve(_clients.size());
        for (const auto& [id, client] : _clients) {
            (void)id;
            if (client) {
                clients.push_back(client);
            }
        }
        return clients;
    }

    std::vector<std::shared_ptr<ix::WebSocket>> takePendingHelloClientsLocked() {
        std::vector<std::shared_ptr<ix::WebSocket>> clients;
        while (!_pendingHelloClients.empty()) {
            const auto id = std::move(_pendingHelloClients.front());
            _pendingHelloClients.pop();
            const auto client = _clients.find(id);
            if (client != _clients.end() && client->second) {
                clients.push_back(client->second);
            }
        }
        return clients;
    }

    void sendCachedHelloToPendingClients() {
        std::string hello;
        std::vector<std::shared_ptr<ix::WebSocket>> clients;
        {
            std::lock_guard<std::mutex> lock(_clientsMutex);
            if (_helloText.empty() || _pendingHelloClients.empty()) {
                return;
            }
            hello = _helloText;
            clients = takePendingHelloClientsLocked();
        }
        sendText(clients, hello);
    }

    static void sendText(const std::vector<std::shared_ptr<ix::WebSocket>>& clients,
                         const std::string& text) {
        for (const auto& client : clients) {
            if (client) {
                sendBounded(client,text);
            }
        }
    }

    static void sendBounded(const std::shared_ptr<ix::WebSocket>& client,const std::string& text) {
        if(text.size()>kQueueBytes || client->bufferedAmount()>kQueueBytes-text.size())
            client->close(1013,"control event backpressure; resynchronize");
        else if(!client->sendText(text).success)client->close(1013,"control send failed; resynchronize");
    }

    static bool isHello(const std::string& text) {
        try {
            const auto message = nlohmann::json::parse(text);
            return message.is_object() && message.value("op", -1) == 0;
        } catch (const std::exception&) {
            return false;
        }
    }

    std::uint16_t _port = 0;
    std::uint16_t _localPort = 0;
    std::string _address;
    std::unique_ptr<ix::WebSocketServer> _server;
    IByteSink* _sink = nullptr;
    std::atomic<bool> _open{false};
    std::atomic<bool> _hasConnection{false};
    std::atomic<bool> _polling{false};
    std::atomic<std::uint64_t> _connectionGeneration{0};
    bool _netInitialized = false;
    mutable std::mutex _clientsMutex;
    std::map<std::string, std::shared_ptr<ix::WebSocket>> _clients;
    std::map<std::string, std::uint64_t> _replyTargets;
    std::map<std::uint64_t, std::string> _connectionIdsByReplyTarget;
    std::uint64_t _nextReplyTarget = 1;
    std::function<void()> _beforeDispatchHookForTesting;
    std::function<void()> _connectionChangedHookForTesting;
    std::function<void()> _messageQueuedHookForTesting;
    // Additional peers receive the cached Hello directly, without making the
    // shared runtime adapter reset the active SID.
    std::queue<std::string> _pendingHelloClients;
    std::string _helloText;
    mutable std::mutex _rxMutex;
    std::queue<ReceivedMessage> _rxMessages;
    static constexpr std::size_t kQueueBytes=4U*1024U*1024U;
    std::size_t _rxBytes=0;
    std::string _dispatchConnectionId;
    std::uint64_t _dispatchReplyTarget = 0;
};

}  // namespace axent::transport
