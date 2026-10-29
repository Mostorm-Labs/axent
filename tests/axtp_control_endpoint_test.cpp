#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketMessage.h>
#include <nlohmann/json.hpp>

#include "axent/core/adapter.hpp"
#include "axent/control/axtp_control_endpoint.hpp"
#include "axent/host/axent_host.hpp"

#include "../src/core/control_operation_internal.hpp"
#include "../src/transports/websocket/ix_websocket_transport.hpp"

namespace {

using namespace std::chrono_literals;

void require(bool condition, const char* message)
{
    if (!condition) {
        std::fprintf(stderr,"axtp_control_endpoint_test assertion failed: %s\n",message);std::fflush(stderr);
        throw std::runtime_error(message);
    }
}

class WebSocketProbe final {
public:
    explicit WebSocketProbe(std::uint16_t port)
    {
        socket_.setUrl("ws://127.0.0.1:" + std::to_string(port) + "/");
        socket_.setOnMessageCallback([this](const ix::WebSocketMessagePtr& message) {
            if (!message) {
                return;
            }
            if (message->type == ix::WebSocketMessageType::Open) {
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    connected_ = true;
                }
                ready_.notify_all();
                return;
            }
            if (message->type != ix::WebSocketMessageType::Message) {
                return;
            }
            {
                std::lock_guard<std::mutex> lock(mutex_);
                messages_.push(message->str);
            }
            ready_.notify_one();
        });
        socket_.start();
    }

    ~WebSocketProbe()
    {
        socket_.stop();
    }

    nlohmann::json next()
    {
        auto message = next_for(5s);
        if (!message.has_value()) {
            throw std::runtime_error("timed out waiting for AXTP WebSocket message");
        }
        return std::move(*message);
    }

    std::optional<nlohmann::json> next_for(std::chrono::milliseconds timeout)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!ready_.wait_for(lock, timeout, [this]() { return !messages_.empty(); })) {
            return std::nullopt;
        }
        auto message = std::move(messages_.front());
        messages_.pop();
        return nlohmann::json::parse(message);
    }

    void send(nlohmann::json message)
    {
        socket_.sendText(message.dump());
    }

    bool wait_for_connection(std::chrono::milliseconds timeout)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return ready_.wait_for(lock, timeout, [this]() { return connected_; });
    }

    void stop()
    {
        socket_.stop();
    }

private:
    std::mutex mutex_;
    std::condition_variable ready_;
    std::queue<std::string> messages_;
    bool connected_ = false;
    ix::WebSocket socket_;
};

axent::control::ControlRoute generated(std::uint32_t id, std::string name)
{
    return {id, std::move(name), false};
}

axent::control::ControlRoute private_route(std::uint32_t id, std::string name)
{
    return {id, std::move(name), true};
}

void expect_invalid_registration(axent::AxtpControlEndpoint& endpoint,
                                 axent::control::ControlRoute route)
{
    bool threw = false;
    try {
        auto ignored = endpoint.register_handler(
            std::move(route),
            [](const axent::control::ControlRequest&) {
                return axent::control::ControlResult{};
            });
        (void)ignored;
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    require(threw, "invalid control route registration should fail fast");
}

nlohmann::json request_message(const std::string& sid,
                               std::uint32_t id,
                               std::string method,
                               nlohmann::json params = nlohmann::json::object(),
                               nlohmann::json metadata = nullptr)
{
    nlohmann::json message = {
        {"sid", sid},
        {"op", 7},
        {"d", {{"id", id}, {"method", std::move(method)}, {"params", std::move(params)}}},
    };
    if (!metadata.is_null()) {
        message["m"] = std::move(metadata);
    }
    return message;
}

std::uint32_t response_code(const nlohmann::json& response)
{
    return response.at("d").at("status").at("code").get<std::uint32_t>();
}

class DeferredReplyTargetSink final : public axtp::IByteSink {
public:
    explicit DeferredReplyTargetSink(axent::transport::WebSocketTransport& transport)
        : transport_(transport)
    {
    }

    void onBytes(const axtp::Byte*, std::size_t) override
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            reply_target_ = transport_.currentReplyTarget();
            dispatched_ = true;
        }
        dispatched_cv_.notify_all();
    }

    bool wait_for_dispatch(std::chrono::milliseconds timeout)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return dispatched_cv_.wait_for(lock, timeout, [this]() { return dispatched_; });
    }

    std::uint64_t reply_target() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return reply_target_;
    }

private:
    axent::transport::WebSocketTransport& transport_;
    mutable std::mutex mutex_;
    std::condition_variable dispatched_cv_;
    std::uint64_t reply_target_ = 0;
    bool dispatched_ = false;
};

void exercise_queued_disconnect_reply_target()
{
    axent::transport::WebSocketTransport transport(0);
    std::mutex ordering_mutex;
    std::condition_variable ordering_cv;
    std::size_t connection_changes = 0;
    bool message_queued = false;
    bool dispatch_paused = false;
    bool release_dispatch = false;
    transport.setConnectionChangedHookForTesting([&]() {
        {
            std::lock_guard<std::mutex> lock(ordering_mutex);
            ++connection_changes;
        }
        ordering_cv.notify_all();
    });
    transport.setBeforeDispatchHookForTesting([&]() {
        std::unique_lock<std::mutex> lock(ordering_mutex);
        dispatch_paused = true;
        ordering_cv.notify_all();
        ordering_cv.wait(lock, [&]() { return release_dispatch; });
    });
    transport.setMessageQueuedHookForTesting([&]() {
        {
            std::lock_guard<std::mutex> lock(ordering_mutex);
            message_queued = true;
        }
        ordering_cv.notify_all();
    });
    DeferredReplyTargetSink sink(transport);
    transport.bind(sink);
    transport.open();
    require(transport.localPort() != 0, "queued disconnect transport should open");

    WebSocketProbe source_probe(transport.localPort());
    WebSocketProbe unrelated_probe(transport.localPort());
    require(source_probe.wait_for_connection(2s) && unrelated_probe.wait_for_connection(2s),
            "queued disconnect WebSocket clients should establish connections");
    {
        std::unique_lock<std::mutex> lock(ordering_mutex);
        require(ordering_cv.wait_for(lock, 2s, [&]() { return connection_changes >= 2; }),
                "both queued disconnect clients should connect");
    }
    source_probe.send({{"request", "queued-before-close"}});
    {
        std::unique_lock<std::mutex> lock(ordering_mutex);
        require(ordering_cv.wait_for(lock, 2s, [&]() { return message_queued; }),
                "source request should be queued before polling dispatch");
    }
    std::thread transport_poller([&]() { transport.poll(); });
    {
        std::unique_lock<std::mutex> lock(ordering_mutex);
        require(ordering_cv.wait_for(lock, 2s, [&]() { return dispatch_paused; }),
                "request should pause after queueing and before dispatch");
    }
    source_probe.stop();
    {
        std::unique_lock<std::mutex> lock(ordering_mutex);
        require(ordering_cv.wait_for(lock, 2s, [&]() { return connection_changes >= 3; }),
                "source close must remove its live reply target before dispatch");
        release_dispatch = true;
    }
    ordering_cv.notify_all();
    transport_poller.join();
    require(sink.wait_for_dispatch(2s), "queued request should dispatch after release");
    require(sink.reply_target() != 0,
            "queued request must retain its opaque reply target after source close");

    const nlohmann::json deferred_response = {{"deferred", true}};
    const auto text = deferred_response.dump();
    transport.sendBytesTo(
        sink.reply_target(), reinterpret_cast<const axtp::Byte*>(text.data()), text.size());
    require(!unrelated_probe.next_for(250ms).has_value(),
            "deferred response for a closed target must not reach another client");
    transport.close();
}

class DeferredEndpointRelayAdapter final : public axent::Adapter {
public:
    axent::AdapterMetadata metadata() const override
    {
        return {"deferred-endpoint-relay", "Deferred endpoint relay characterization", true, ""};
    }

    std::vector<axent::Capability> capabilities() const override
    {
        return {};
    }

    std::vector<axent::DeviceSnapshot> discover() override
    {
        return {
            device("relay-device-a", "endpoint/relay-a"),
            device("relay-device-b", "endpoint/relay-b"),
        };
    }

    axent::ControlResult call(const std::string&,
                              const std::string&,
                              const nlohmann::json&) override
    {
        return {axent::ControlStatus::InternalError, { {"error", "legacy call used"} }};
    }

    axent::ControlOperationPtr call_async(
        const axent::AdapterControlRequest& request,
        axent::ControlCallOptions) override
    {
        if (request.destination_endpoint_id == "endpoint/relay-a") {
            auto pending = std::make_shared<PendingCall>();
            {
                std::lock_guard<std::mutex> lock(mutex_);
                pending_a_ = pending;
                ++a_admissions_;
            }
            a_admitted_cv_.notify_all();
            return pending->source.operation();
        }

        if (request.destination_endpoint_id == "endpoint/relay-b") {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                b_admitted_ = true;
            }
            return axent::make_completed_control_operation(
                {axent::ControlStatus::Ok, { {"device", "B"} }});
        }

        return axent::make_completed_control_operation(
            {axent::ControlStatus::NotFound, nlohmann::json::object()});
    }

    axent::ControlResult start_firmware_update(
        const std::string&, const std::string&) override
    {
        return {axent::ControlStatus::Unavailable, nlohmann::json::object()};
    }

    bool wait_for_a_admissions(std::size_t count, std::chrono::milliseconds timeout)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return a_admitted_cv_.wait_for(lock, timeout, [this, count]() {
            return a_admissions_ >= count;
        });
    }

    bool b_admitted() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return b_admitted_;
    }

    void release_a()
    {
        std::shared_ptr<PendingCall> pending;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            pending.swap(pending_a_);
        }
        if (pending) {
            pending->source.complete({axent::ControlStatus::Ok, { {"device", "A"} }});
        }
    }

private:
    struct PendingCall {
        axent::ControlOperationSource source;
    };

    static axent::DeviceSnapshot device(std::string id, std::string endpoint_id)
    {
        axent::DeviceSnapshot snapshot;
        snapshot.id = std::move(id);
        snapshot.adapter = "deferred-endpoint-relay";
        snapshot.endpoint_id = std::move(endpoint_id);
        snapshot.connection.online = true;
        snapshot.connection.transport = "test";
        return snapshot;
    }

    mutable std::mutex mutex_;
    std::condition_variable a_admitted_cv_;
    std::size_t a_admissions_ = 0;
    bool b_admitted_ = false;
    std::shared_ptr<PendingCall> pending_a_;
};

void characterize_synchronous_endpoint_relay_bottleneck()
{
    axent::AxentHost host;
    DeferredEndpointRelayAdapter* adapter = nullptr;
    axent::AxentHostOptions host_options;
    host_options.enable_mock_adapter = false;
    host_options.enable_axtp_adapter = true;
    host_options.axtp_adapter_factory = [&adapter](axent::AxtpAdapterConfig) {
        auto relay_adapter = std::make_unique<DeferredEndpointRelayAdapter>();
        adapter = relay_adapter.get();
        return relay_adapter;
    };
    require(host.start(std::move(host_options)), "relay characterization Host should start");
    require(adapter != nullptr, "relay characterization adapter should be installed");

    axent::AxtpControlEndpoint endpoint;
    auto relay_token = endpoint.register_endpoint_handler(
        generated(0x1612, "cast.getStatus"),
        [&host](const axent::control::ControlRequest& request) {
            return host.call_endpoint({
                request.source_endpoint_id,
                request.destination_endpoint_id,
                request.method,
                request.params,
            });
        });

    try {
        require(endpoint.start() == axent::control::ControlStatus::success(),
                "relay characterization endpoint should start");
        WebSocketProbe probe(endpoint.local_port());
        const auto hello = probe.next();
        require(hello.at("op") == 0, "relay characterization must receive Hello");
        probe.send({{"sid", ""}, {"op", 2}, {"d", {{"randomSeed", 0xA11CE}}}});
        const auto identified = probe.next();
        require(identified.at("op") == 3, "relay characterization must Identify");
        const auto sid = identified.at("sid").get<std::string>();

        WebSocketProbe unrelated_probe(endpoint.local_port());
        const auto unrelated_hello = unrelated_probe.next();
        require(unrelated_hello.at("op") == 0,
                "unrelated relay client must receive its own Hello");
        unrelated_probe.send(
            {{"sid", ""}, {"op", 2}, {"d", {{"resumeSid", sid}}}});
        const auto unrelated_identified = unrelated_probe.next();
        require(unrelated_identified.at("sid") == sid,
                "unrelated relay client must join the shared session");

        probe.send(request_message(
            sid,
            901,
            "cast.getStatus",
            nlohmann::json::object(),
            {{"src", "endpoint/relay-client"}, {"dst", "endpoint/relay-a"}}));
        require(adapter->wait_for_a_admissions(1, 2s),
                "device A operation should be admitted before sending device B");

        adapter->release_a();
        const auto targeted_a_response = probe.next_for(2s);
        require(targeted_a_response.has_value() &&
                    targeted_a_response->at("d").at("id") == 901,
                "deferred response must return to its originating WebSocket client");
        require(!unrelated_probe.next_for(250ms).has_value(),
                "deferred response must not leak to another WebSocket client");

        probe.send(request_message(
            sid,
            903,
            "cast.getStatus",
            nlohmann::json::object(),
            {{"src", "endpoint/relay-client"}, {"dst", "endpoint/relay-a"}}));
        require(adapter->wait_for_a_admissions(2, 2s),
                "second device A operation should be admitted before sending device B");

        probe.send(request_message(
            sid,
            902,
            "cast.getStatus",
            nlohmann::json::object(),
            {{"src", "endpoint/relay-client"}, {"dst", "endpoint/relay-b"}}));
        const auto b_response = probe.next_for(500ms);
        require(adapter->b_admitted(),
                "device B must be admitted to its Adapter before device A is released");
        require(b_response.has_value(),
                "device B response must arrive before device A is released");
        require(b_response->at("d").at("id") == 902,
                "device B response must preserve its AXTP request ID");
        require(b_response->at("m") == nlohmann::json({{"src", "endpoint/relay-b"},
                                                          {"dst", "endpoint/relay-client"}}),
                "device B response must reverse AXTP Endpoint metadata");

        adapter->release_a();
        const auto a_response = probe.next_for(2s);
        require(a_response.has_value() && a_response->at("d").at("id") == 903,
                "device A response must complete after release");
        require(endpoint.stop() == axent::control::ControlStatus::success(),
                "relay characterization endpoint should stop cleanly");
    } catch (...) {
        adapter->release_a();
        endpoint.stop();
        host.stop();
        throw;
    }

    host.stop();
    (void)relay_token;
}

void exercise_endpoint_handler_status_and_cancellation()
{
    axent::AxtpControlEndpoint endpoint;
    axent::ControlOperationSource pending_source;
    std::mutex pending_mutex;
    std::condition_variable pending_cv;
    bool pending_started = false;

    auto token = endpoint.register_endpoint_handler(
        generated(0x1612, "cast.getStatus"),
        [&](const axent::control::ControlRequest& request) -> axent::ControlOperationPtr {
            if (request.params.value("pending", false)) {
                {
                    std::lock_guard<std::mutex> lock(pending_mutex);
                    pending_started = true;
                }
                pending_cv.notify_all();
                return pending_source.operation();
            }
            if (request.params.value("throw", false)) {
                throw std::runtime_error("endpoint handler failure");
            }
            if (request.params.value("null", false)) {
                return nullptr;
            }

            const auto status = request.params.value("status", "ok");
            axent::ControlStatus result_status = axent::ControlStatus::Ok;
            if (status == "accepted") result_status = axent::ControlStatus::Accepted;
            if (status == "not_found") result_status = axent::ControlStatus::NotFound;
            if (status == "not_supported") result_status = axent::ControlStatus::NotSupported;
            if (status == "forbidden") result_status = axent::ControlStatus::Forbidden;
            if (status == "invalid_argument") result_status = axent::ControlStatus::InvalidArgument;
            if (status == "busy") result_status = axent::ControlStatus::Busy;
            if (status == "unavailable") result_status = axent::ControlStatus::Unavailable;
            if (status == "internal_error") result_status = axent::ControlStatus::InternalError;
            return axent::make_completed_control_operation(
                {result_status, {{"legacyMetadata", request.destination_endpoint_id}}});
        });

    require(endpoint.start() == axent::control::ControlStatus::success(),
            "endpoint handler status endpoint should start");
    WebSocketProbe probe(endpoint.local_port());
    (void)probe.next();
    probe.send({{"sid", ""}, {"op", 2}, {"d", {{"randomSeed", 0xE11}}}});
    const auto identified = probe.next();
    const auto sid = identified.at("sid").get<std::string>();

    const std::vector<std::pair<std::string, std::uint32_t>> statuses = {
        {"ok", 0x0000}, {"accepted", 0x0000}, {"not_found", 0x000C},
        {"not_supported", 0x0003}, {"forbidden", 0x0009},
        {"invalid_argument", 0x000A}, {"busy", 0x0005},
        {"unavailable", 0x000F}, {"internal_error", 0x000E},
    };
    std::uint32_t request_id = 1100;
    for (const auto& [status, expected_code] : statuses) {
        probe.send(request_message(sid, request_id++, "cast.getStatus", {{"status", status}}));
        const auto response = probe.next();
        if (response_code(response) != expected_code) {
            throw std::runtime_error(
                "endpoint operation status mapping mismatch for " + status + ": got " +
                std::to_string(response_code(response)) + ", expected " +
                std::to_string(expected_code));
        }
        if (expected_code == 0) {
            require(response.at("d").at("result").at("legacyMetadata") == "",
                    "endpoint handler must accept requests without Endpoint metadata");
        }
    }

    probe.send(request_message(sid, request_id++, "cast.getStatus", {{"null", true}}));
    require(response_code(probe.next()) == 0x000E,
            "null endpoint operation must fail closed as InternalError");
    probe.send(request_message(sid, request_id++, "cast.getStatus", {{"throw", true}}));
    require(response_code(probe.next()) == 0x000E,
            "endpoint handler exception must fail closed as InternalError");

    probe.send(request_message(sid, request_id, "cast.getStatus", {{"pending", true}}));
    {
        std::unique_lock<std::mutex> lock(pending_mutex);
        require(pending_cv.wait_for(lock, 2s, [&]() { return pending_started; }),
                "pending endpoint operation should be admitted");
    }
    token.reset();
    require(pending_source.cancellation_requested(),
            "registration token reset must cancel pending endpoint operations");
    require(response_code(probe.next()) == 0x000F,
            "cancelled endpoint operation must report Unavailable");
    require(endpoint.stop() == axent::control::ControlStatus::success(),
            "endpoint handler status endpoint should stop");

    axent::AxtpControlEndpoint stop_endpoint;
    axent::ControlOperationSource stop_pending_source;
    std::mutex stop_mutex;
    std::condition_variable stop_cv;
    bool stop_pending_started = false;
    auto stop_token = stop_endpoint.register_endpoint_handler(
        generated(0x1612, "cast.getStatus"),
        [&](const axent::control::ControlRequest&) {
            {
                std::lock_guard<std::mutex> lock(stop_mutex);
                stop_pending_started = true;
            }
            stop_cv.notify_all();
            return stop_pending_source.operation();
        });
    require(stop_endpoint.start() == axent::control::ControlStatus::success(),
            "endpoint stop cancellation endpoint should start");
    {
        WebSocketProbe stop_probe(stop_endpoint.local_port());
        (void)stop_probe.next();
        stop_probe.send({{"sid", ""}, {"op", 2}, {"d", {{"randomSeed", 0xE12}}}});
        const auto stop_sid = stop_probe.next().at("sid").get<std::string>();
        stop_probe.send(request_message(stop_sid, 1200, "cast.getStatus"));
        std::unique_lock<std::mutex> lock(stop_mutex);
        require(stop_cv.wait_for(lock, 2s, [&]() { return stop_pending_started; }),
                "pending endpoint operation should be admitted before stop");
    }
    require(stop_endpoint.stop() == axent::control::ControlStatus::success(),
            "endpoint stop should complete with a pending endpoint operation");
    require(stop_pending_source.cancellation_requested(),
            "endpoint stop must cancel pending endpoint operations");
    (void)stop_token;
}

} // namespace

int main()
{
    std::set_terminate([] {
        if (auto error = std::current_exception()) {
            try { std::rethrow_exception(error); }
            catch (const std::exception& detail) {
                std::fprintf(stderr, "axtp_control_endpoint_test uncaught exception: %s\n", detail.what());
            } catch (...) {
                std::fprintf(stderr, "axtp_control_endpoint_test uncaught non-standard exception\n");
            }
        } else {
            std::fprintf(stderr, "axtp_control_endpoint_test terminated without an active exception\n");
        }
        std::fflush(stderr);
        std::abort();
    });
    using axent::control::ControlResult;
    using axent::control::ControlStatus;

    characterize_synchronous_endpoint_relay_bottleneck();
    exercise_queued_disconnect_reply_target();
    exercise_endpoint_handler_status_and_cancellation();

    require(ControlStatus::success().ok(), "success status should be ok");
    require(!ControlStatus::busy().ok(), "busy status should not be ok");
    require(ControlStatus::from_protocol_code(0x0555).code == 0x0555,
            "unknown protocol status code must be preserved");

    axent::AxtpControlEndpoint endpoint;
    std::atomic<int> handler_calls{0};
    std::mutex worker_id_mutex;
    std::thread::id worker_id;
    auto observe_worker = [&]() {
        std::lock_guard<std::mutex> lock(worker_id_mutex);
        if (worker_id == std::thread::id{}) {
            worker_id = std::this_thread::get_id();
        } else {
            require(worker_id == std::this_thread::get_id(),
                    "handlers and posted work must use one endpoint worker");
        }
    };

    auto status_token = endpoint.register_handler(
        generated(0x1612, "cast.getStatus"),
        [&](const axent::control::ControlRequest& request) {
            observe_worker();
            ++handler_calls;
            if (request.params.value("throwInvalid", false)) {
                throw std::invalid_argument("bad request");
            }
            if (request.params.contains("statusCode")) {
                return ControlResult{
                    ControlStatus::from_protocol_code(request.params.at("statusCode").get<std::uint32_t>()),
                    nlohmann::json::object(),
                };
            }
            if (request.params.value("publish", false)) {
                require(endpoint.publish_event(
                            generated(0x1601, "cast.sessionIncoming"),
                            {{"fromHandler", true}}) == ControlStatus::success(),
                        "handler should enqueue an event without synchronous send");
            }
            return ControlResult{
                ControlStatus::success(),
                {{"requestId", request.request_id},
                 {"method", request.method},
                 {"sourceEndpointId", request.source_endpoint_id},
                 {"destinationEndpointId", request.destination_endpoint_id}},
            };
        });
    auto private_token = endpoint.register_handler(
        private_route(0x7F23, "cast.subscribeEvents"),
        [](const axent::control::ControlRequest&) {
            return ControlResult{ControlStatus::success(), {{"accepted", true}}};
        });

    std::mutex blocking_mutex;
    std::condition_variable blocking_cv;
    bool handler_entered = false;
    bool release_handler = false;
    auto blocking_token = endpoint.register_handler(
        generated(0x1601, "cast.getSession"),
        [&](const axent::control::ControlRequest&) {
            observe_worker();
            std::unique_lock<std::mutex> lock(blocking_mutex);
            handler_entered = true;
            blocking_cv.notify_all();
            blocking_cv.wait(lock, [&]() { return release_handler; });
            return ControlResult{ControlStatus::success(), {{"released", true}}};
        });

    auto self_stop_token = endpoint.register_handler(
        generated(0x1602, "cast.stopSession"),
        [&](const axent::control::ControlRequest&) {
            const auto stopped = endpoint.stop();
            return ControlResult{ControlStatus::success(), {{"stopStatus", stopped.code}}};
        });

    auto removable = endpoint.register_handler(
        generated(0x1605, "cast.getAudio"),
        [](const axent::control::ControlRequest&) { return ControlResult{}; });
    removable.reset();
    auto replacement = endpoint.register_handler(
        generated(0x1605, "cast.getAudio"),
        [](const axent::control::ControlRequest&) {
            return ControlResult{ControlStatus::success(), {{"replacement", true}}};
        });

    expect_invalid_registration(endpoint, generated(0x1601, "cast.getStatus"));
    expect_invalid_registration(endpoint, generated(0x1612, "cast.unknown"));
    expect_invalid_registration(endpoint, private_route(0x1612, "cast.getStatus"));
    expect_invalid_registration(endpoint, {0x7F24, "cast.unknown", false});
    expect_invalid_registration(endpoint, private_route(0x7F25, "cast.subscribeEvents"));

    require(endpoint.start() == ControlStatus::success(),
            "endpoint should start on an ephemeral port");
    require(endpoint.running(), "endpoint should report running");
    require(endpoint.local_port() != 0, "endpoint should publish its bound port");

    bool sealed = false;
    try {
        auto ignored = endpoint.register_handler(
            generated(0x1603, "cast.getAirPlayName"),
            [](const axent::control::ControlRequest&) { return ControlResult{}; });
        (void)ignored;
    } catch (const std::logic_error&) {
        sealed = true;
    }
    require(sealed, "endpoint registry must be sealed after start");

    auto first = std::make_unique<WebSocketProbe>(endpoint.local_port());
    auto hello = first->next();
    require(hello.at("sid") == "", "hello sid must stay empty");
    require(hello.at("op") == 0, "endpoint must preserve AXTP Hello op");
    require(hello.at("d").contains("axtpVersion"), "hello must advertise AXTP version");

    first->send(request_message("", 1, "cast.getStatus"));
    auto response = first->next();
    require(response_code(response) == 0x0024,
            "request before Identify must preserve ControlOpenRequired");

    first->send({{"sid", ""},
                 {"op", 2},
                 {"d", {{"randomSeed", 0x12345678},
                         {"resumeSid", "legacy-session"},
                         {"eventMasks", "000000"}}}});
    auto identified = first->next();
    require(identified.at("op") == 3, "endpoint must preserve AXTP Identified op");
    std::string sid = identified.at("sid").get<std::string>();
    require(sid == "legacy-session", "resumeSid behavior must stay compatible");

    first->send({{"sid", sid}, {"op", 4}, {"d", {{"eventMasks", "cast.*"}}}});
    auto reidentified = first->next();
    require(reidentified.at("op") == 3 && reidentified.at("sid") == sid,
            "Reidentify must acknowledge the existing SID exactly once");
    require(!first->next_for(100ms).has_value(),
            "Reidentify must not enqueue a duplicate Identified frame");

    first->send({{"sid", ""},
                 {"op", 2},
                 {"d", {{"randomSeed", 7}, {"resumeSid", "replacement-session"}}}});
    auto duplicate_identified = first->next();
    require(duplicate_identified.at("op") == 3 && duplicate_identified.at("sid") == sid,
            "duplicate Identify must keep the active SID");
    require(!first->next_for(100ms).has_value(),
            "duplicate Identify must produce only one acknowledgement");

    first->send(request_message("wrong-session", 2, "cast.getStatus"));
    response = first->next();
    require(response_code(response) == 0x0033, "wrong SID must preserve RpcPayloadInvalid");

    first->send({{"sid", sid}, {"op", 9}, {"d", {{"id", 3}, {"requests", nlohmann::json::array()}}}});
    response = first->next();
    require(response.at("op") == 10, "batch error must use RequestBatchResponse");
    require(response_code(response) == 0x003F, "batch must preserve RpcBatchUnsupported");

    first->send(request_message(sid, 4, "cast.unknown"));
    response = first->next();
    require(response_code(response) == 0x0036, "unknown method must preserve RpcMethodNotFound");

    auto request = [&](std::uint32_t id, std::string method, nlohmann::json params) {
        first->send(request_message(sid, id, std::move(method), std::move(params)));
        return first->next();
    };

    first->send(request_message(
        sid,
        101,
        "cast.getStatus",
        {{"includeSensitive", false}},
        {{"src", "ep-controller"}, {"dst", "ep-nearcast"}}));
    response = first->next();
    require(response.at("sid") == sid, "response sid must match the identified session");
    require(response.at("op") == 8, "endpoint must preserve AXTP response op");
    require(response.at("d").at("id") == 101, "response id mismatch");
    require(response_code(response) == 0, "successful status mismatch");
    require(response.at("d").at("result").at("method") == "cast.getStatus",
            "handler result mismatch");
    require(response.at("d").at("result").at("sourceEndpointId") == "ep-controller",
            "routed source Endpoint must reach the handler");
    require(response.at("d").at("result").at("destinationEndpointId") == "ep-nearcast",
            "routed destination Endpoint must reach the handler");
    require(response.at("m") == nlohmann::json({{"src", "ep-nearcast"},
                                                   {"dst", "ep-controller"}}),
            "runtime must reverse routed response metadata");

    response = request(102, "cast.getStatus", nlohmann::json::object());
    require(response.at("d").at("id") == 102,
            "legacy request must still complete using its request ID");
    require(response.at("d").at("result").at("sourceEndpointId") == "",
            "legacy request without metadata must expose an empty source Endpoint");
    require(response.at("d").at("result").at("destinationEndpointId") == "",
            "legacy request without metadata must expose an empty destination Endpoint");

    response = request(103, "cast.getStatus", {{"throwInvalid", true}});
    require(response_code(response) == ControlStatus::invalid_argument().code,
            "invalid_argument exception mapping mismatch");

    const std::vector<std::uint32_t> status_codes = {
        ControlStatus::not_supported().code,
        ControlStatus::busy().code,
        ControlStatus::timeout().code,
        ControlStatus::unavailable().code,
        ControlStatus::internal_error().code,
        ControlStatus::stream_not_open().code,
        0x0033,
        0x0555,
    };
    std::uint32_t request_id = 200;
    for (const auto code : status_codes) {
        response = request(request_id++, "cast.getStatus", {{"statusCode", code}});
        require(response_code(response) == code,
                "protocol error status must be preserved without remapping");
    }

    response = request(301, "cast.subscribeEvents", nlohmann::json::object());
    require(response_code(response) == 0, "explicit private method should dispatch");
    require(response.at("d").at("result").at("accepted") == true,
            "private method result mismatch");

    response = request(302, "cast.getAudio", nlohmann::json::object());
    require(response.at("d").at("result").at("replacement") == true,
            "route reset before start should permit a replacement registration");

    response = request(303, "cast.getStatus", {{"publish", true}});
    require(response.at("op") == 8 && response.at("d").at("id") == 303,
            "handler response must be sent before its queued event");
    auto event = first->next();
    require(event.at("op") == 6, "queued handler event must follow the response");
    require(event.at("d").at("event") == "cast.sessionIncoming",
            "queued handler event name mismatch");
    require(event.at("d").at("data").at("fromHandler") == true,
            "queued handler event data mismatch");

    std::atomic<bool> posted{false};
    require(endpoint.post([&]() {
                observe_worker();
                posted.store(true);
            }) == ControlStatus::success(),
            "endpoint should accept product-neutral worker tasks");
    const auto post_deadline = std::chrono::steady_clock::now() + 2s;
    while (!posted.load() && std::chrono::steady_clock::now() < post_deadline) {
        std::this_thread::sleep_for(2ms);
    }
    require(posted.load(), "posted task should run on the endpoint worker");

    response = request(304, "cast.stopSession", nlohmann::json::object());
    require(response.at("d").at("result").at("stopStatus") ==
                ControlStatus::invalid_state().code,
            "stop called from a handler must fail fast instead of self-joining");
    require(endpoint.running(), "rejected self-stop must leave endpoint running");

    require(endpoint.publish_event(generated(0x1602, "cast.sessionStateChanged"), nullptr) ==
                ControlStatus::success(),
            "event with null data should enqueue");
    event = first->next();
    require(event.at("d").contains("data") && event.at("d").at("data").is_null(),
            "event null data must remain null on the wire");

    auto second = std::make_unique<WebSocketProbe>(endpoint.local_port());
    const auto second_hello = second->next();
    require(second_hello.at("op") == 0,
            "new connection must receive a targeted Hello");
    require(!first->next_for(250ms).has_value(),
            "existing clients must not receive a new client's Hello");
    second->send({{"sid", ""}, {"op", 2}, {"d", {{"resumeSid", "shared-session"}}}});
    const auto second_identified = second->next();
    require(second_identified.at("sid") == sid,
            "additional clients must join the active shared SID without replacing it");
    require(!first->next_for(250ms).has_value(),
            "existing clients must not receive another client's Identified response");

    second->send(request_message(sid, 350, "cast.getStatus"));
    const auto second_response = second->next();
    require(second_response.at("d").at("id") == 350 &&
                response_code(second_response) == 0,
            "second client request must receive its own response");
    require(!first->next_for(250ms).has_value(),
            "request responses must not leak to other clients");

    first->send(request_message(sid, 351, "cast.getStatus"));
    const auto first_response = first->next();
    require(first_response.at("d").at("id") == 351 &&
                response_code(first_response) == 0,
            "first client must remain usable after another client identifies");
    require(!second->next_for(250ms).has_value(),
            "first client responses must not leak to the second client");

    require(endpoint.publish_event(
                generated(0x1601, "cast.sessionIncoming"), {{"broadcast", true}}) ==
                ControlStatus::success(),
            "broadcast event should enqueue");
    const auto first_event = first->next();
    const auto second_event = second->next();
    require(first_event.at("sid") == sid && second_event.at("sid") == sid,
            "event must preserve current multi-client broadcast/shared SID behavior");
    second.reset();

    const auto calls_before_reset = handler_calls.load();
    status_token.reset();
    response = request(401, "cast.getStatus", nlohmann::json::object());
    require(response_code(response) == ControlStatus::unavailable().code,
            "inactive sealed registration should return unavailable");
    require(handler_calls.load() == calls_before_reset,
            "handler must not be called after registration token reset");

    first->send(request_message(sid, 500, "cast.getSession", {{"block", true}}));
    {
        std::unique_lock<std::mutex> lock(blocking_mutex);
        require(blocking_cv.wait_for(lock, 2s, [&]() { return handler_entered; }),
                "blocking handler should enter before stop");
    }
    std::atomic<bool> stop_finished{false};
    ControlStatus stop_status = ControlStatus::internal_error();
    std::thread stopper([&]() {
        stop_status = endpoint.stop();
        stop_finished.store(true);
    });
    std::this_thread::sleep_for(50ms);
    require(!stop_finished.load(), "stop must wait for an in-flight handler");
    require(endpoint.start() == ControlStatus::invalid_state(),
            "start must fail fast while another thread is stopping the endpoint");
    std::atomic<bool> follower_stop_finished{false};
    ControlStatus follower_stop_status = ControlStatus::internal_error();
    std::thread follower_stopper([&]() {
        follower_stop_status = endpoint.stop();
        follower_stop_finished.store(true);
    });
    std::this_thread::sleep_for(20ms);
    require(!follower_stop_finished.load(),
            "concurrent stop should wait for the active stop operation");
    require(endpoint.post([]() {}) == ControlStatus::unavailable(),
            "stop must reject newly posted work");
    require(endpoint.publish_event(generated(0x1601, "cast.sessionIncoming"), {}) ==
                ControlStatus::unavailable(),
            "stop must reject newly published events");
    {
        std::lock_guard<std::mutex> lock(blocking_mutex);
        release_handler = true;
    }
    blocking_cv.notify_all();
    stopper.join();
    follower_stopper.join();
    require(stop_status == ControlStatus::success() && stop_finished.load(),
            "external stop should join its worker successfully");
    require(follower_stop_status == ControlStatus::success() && follower_stop_finished.load(),
            "concurrent stop should complete idempotently after the shared worker joins");
    require(!endpoint.running(), "endpoint should report stopped");
    require(endpoint.local_port() == 0, "stopped endpoint must clear its local port");
    require(endpoint.stop() == ControlStatus::success(), "stop should be idempotent");

    first.reset();
    require(endpoint.start() == ControlStatus::success(),
            "endpoint should restart with its sealed static registry");
    auto reconnected = std::make_unique<WebSocketProbe>(endpoint.local_port());
    hello = reconnected->next();
    require(hello.at("op") == 0, "reconnected client should receive Hello");
    reconnected->send({{"sid", ""}, {"op", 2}, {"d", {{"randomSeed", 7}}}});
    identified = reconnected->next();
    require(identified.at("op") == 3 && identified.at("sid").is_string(),
            "reconnected client should complete Identify");
    reconnected.reset();
    require(endpoint.stop() == ControlStatus::success(), "restarted endpoint should stop cleanly");

    axent::control::RegistrationToken survivor;
    {
        axent::AxtpControlEndpoint short_lived;
        survivor = short_lived.register_handler(
            generated(0x1601, "cast.getSession"),
            [](const axent::control::ControlRequest&) { return ControlResult{}; });
    }
    survivor.reset();
    require(!survivor, "token reset after endpoint destruction must be safe");

    (void)private_token;
    (void)blocking_token;
    (void)self_stop_token;
    (void)replacement;
    {
        axent::AxtpControlEndpoint limited;
        auto token = limited.register_handler({std::nullopt,"cast.setPresentationSettings",false},
            [](const axent::control::ControlRequest&) { return axent::control::ControlResult{}; });
        axent::AxtpControlEndpointOptions options;
        options.max_pending_events = 1;
        require(limited.start(options).ok(),"new generated route must register by name");
        std::vector<axent::ControlEvent> batch = {
            {{std::nullopt,"cast.sessionsChanged",false},nlohmann::json::object()},
            {{std::nullopt,"cast.presentationChanged",false},nlohmann::json::object()}};
        require(limited.publish_events(std::move(batch)) == ControlStatus::busy(),
            "oversized event batch must reject atomically");
        require(limited.running(),"event backpressure must preserve control listener");
        require(limited.stop().ok(),"limited endpoint stops independently");
    }
    return 0;
}
