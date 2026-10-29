#include "axent/control/axtp_control_endpoint.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#include "core/protocol/generated/axtp_event_registry_generated.h"
#include "core/protocol/generated/axtp_method_registry_generated.h"
#include "core/runtime/endpoint/axtp_endpoint.hpp"
#include "json_rpc/websocket_json_rpc_adapter.hpp"
#include "websocket/ix_websocket_transport.hpp"

namespace axent {
namespace {

constexpr auto kWorkerPollInterval = std::chrono::milliseconds(10);
constexpr std::uint32_t kMaxAxtpCode = 0xFFFF;
thread_local const void* g_active_endpoint_worker = nullptr;

class WorkerThreadMarker final {
public:
    explicit WorkerThreadMarker(const void* endpoint)
        : previous_(g_active_endpoint_worker)
    {
        g_active_endpoint_worker = endpoint;
    }

    ~WorkerThreadMarker()
    {
        g_active_endpoint_worker = previous_;
    }

private:
    const void* previous_ = nullptr;
};

struct ResolvedRoute {
    std::uint32_t protocol_id = 0;
    std::string name;
    bool private_route = false;
};

template <typename Descriptor, std::size_t Size>
ResolvedRoute resolve_route(const control::ControlRoute& route,
                            const Descriptor (&registry)[Size])
{
    if (route.name.empty()) {
        throw std::invalid_argument("control route name must not be empty");
    }
    if (route.protocol_id.has_value() && *route.protocol_id > kMaxAxtpCode) {
        throw std::invalid_argument("AXTP route id exceeds the 16-bit protocol range");
    }

    const Descriptor* by_name = nullptr;
    const Descriptor* by_id = nullptr;
    for (const auto& descriptor : registry) {
        if (route.name == descriptor.name) {
            by_name = &descriptor;
        }
        if (route.protocol_id.has_value() && *route.protocol_id == descriptor.id) {
            by_id = &descriptor;
        }
    }

    if (route.allow_private) {
        if (!route.protocol_id.has_value()) {
            throw std::invalid_argument("private AXTP routes require an explicit protocol id");
        }
        if (by_name != nullptr || by_id != nullptr) {
            throw std::invalid_argument("allow_private cannot override a generated AXTP route");
        }
        return {*route.protocol_id, route.name, true};
    }

    if (by_name == nullptr) {
        throw std::invalid_argument("control route name is not in the generated AXTP registry");
    }
    if (route.protocol_id.has_value() &&
        (by_id == nullptr || by_id != by_name)) {
        throw std::invalid_argument("control route id and name do not match the generated AXTP registry");
    }
    return {by_name->id, by_name->name, false};
}

ResolvedRoute resolve_method_route(const control::ControlRoute& route)
{
    return resolve_route(route, axtp::kMethodRegistry);
}

ResolvedRoute resolve_event_route(const control::ControlRoute& route)
{
    return resolve_route(route, axtp::kEventRegistry);
}

nlohmann::json json_from_bytes(const axtp::Bytes& bytes)
{
    if (bytes.empty()) {
        return nlohmann::json::object();
    }
    try {
        const std::string text(bytes.begin(), bytes.end());
        auto parsed = nlohmann::json::parse(text);
        // Preserve value kinds for the contract/host validator. Coercing an
        // array or null into {} hides malformed arguments from new methods.
        return parsed;
    } catch (const std::exception&) {
        return nlohmann::json::object();
    }
}

axtp::Bytes bytes_from_json(const nlohmann::json& value, bool null_as_object)
{
    const auto text = value.is_null() && null_as_object
        ? nlohmann::json::object().dump()
        : value.dump();
    return {text.begin(), text.end()};
}

axtp::ErrorCode endpoint_status_code(ControlStatus status) noexcept
{
    switch (status) {
    case ControlStatus::Ok:
    case ControlStatus::Accepted:
        return axtp::ErrorCode::Success;
    case ControlStatus::NotFound:
        return axtp::ErrorCode::NotFound;
    case ControlStatus::NotSupported:
        return axtp::ErrorCode::NotSupported;
    case ControlStatus::Forbidden:
        return axtp::ErrorCode::PermissionDenied;
    case ControlStatus::InvalidArgument:
        return axtp::ErrorCode::InvalidArgument;
    case ControlStatus::Busy:
        return axtp::ErrorCode::Busy;
    case ControlStatus::Unavailable:
        return axtp::ErrorCode::Unavailable;
    case ControlStatus::InternalError:
        return axtp::ErrorCode::InternalError;
    }
    return axtp::ErrorCode::InternalError;
}

axtp::RpcResponseData endpoint_response(ControlResult result)
{
    axtp::RpcResponseData response;
    response.encoding = axtp::RpcEncoding::Json;
    response.overrideEncoding = true;
    response.statusCode = endpoint_status_code(result.status);
    response.overrideStatus = true;
    response.body = bytes_from_json(result.body, true);
    return response;
}

axtp::RpcResponseData endpoint_internal_error()
{
    return endpoint_response({ControlStatus::InternalError, nlohmann::json::object()});
}

struct HandlerSlot {
    explicit HandlerSlot(control::ControlHandler next_handler)
        : handler(std::move(next_handler))
    {
    }

    control::ControlResult invoke(const control::ControlRequest& request)
    {
        control::ControlHandler current;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (!active || !handler) {
                return {control::ControlStatus::unavailable(), nlohmann::json::object()};
            }
            current = handler;
            ++in_flight;
            callback_thread = std::this_thread::get_id();
        }

        control::ControlResult result;
        try {
            result = current(request);
        } catch (const std::invalid_argument&) {
            result = {control::ControlStatus::invalid_argument(), nlohmann::json::object()};
        } catch (const std::exception&) {
            result = {control::ControlStatus::internal_error(), nlohmann::json::object()};
        } catch (...) {
            result = {control::ControlStatus::internal_error(), nlohmann::json::object()};
        }

        {
            std::lock_guard<std::mutex> lock(mutex);
            --in_flight;
            if (in_flight == 0) {
                callback_thread = {};
                idle.notify_all();
            }
        }
        return result;
    }

    void deactivate() noexcept
    {
        std::unique_lock<std::mutex> lock(mutex);
        active = false;
        handler = {};
        if (callback_thread == std::this_thread::get_id()) {
            return;
        }
        idle.wait(lock, [this]() { return in_flight == 0; });
    }

    std::mutex mutex;
    std::condition_variable idle;
    control::ControlHandler handler;
    bool active = true;
    std::size_t in_flight = 0;
    std::thread::id callback_thread;
};

struct EndpointHandlerSlot {
    explicit EndpointHandlerSlot(control::EndpointControlHandler next_handler)
        : handler(std::move(next_handler))
    {
    }

    ControlOperationPtr invoke(const control::ControlRequest& request)
    {
        control::EndpointControlHandler current;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (!active || !handler) {
                return {};
            }
            current = handler;
            ++in_flight;
            callback_thread = std::this_thread::get_id();
        }

        ControlOperationPtr operation;
        try {
            operation = current(request);
        } catch (...) {
        }

        bool cancel_operation = false;
        {
            std::lock_guard<std::mutex> lock(mutex);
            discard_finished_operations();
            if (operation && active) {
                pending_operations.push_back(operation);
            } else if (operation) {
                cancel_operation = true;
            }
            --in_flight;
            if (in_flight == 0) {
                callback_thread = {};
                idle.notify_all();
            }
        }
        if (cancel_operation) {
            operation->cancel();
        }
        return operation;
    }

    void deactivate() noexcept
    {
        std::vector<ControlOperationPtr> operations;
        {
            std::unique_lock<std::mutex> lock(mutex);
            active = false;
            handler = {};
            if (callback_thread != std::this_thread::get_id()) {
                idle.wait(lock, [this]() { return in_flight == 0; });
            }
            collect_pending_operations(operations);
        }
        cancel_operations(operations);
    }

    void cancel_pending() noexcept
    {
        std::vector<ControlOperationPtr> operations;
        {
            std::lock_guard<std::mutex> lock(mutex);
            collect_pending_operations(operations);
        }
        cancel_operations(operations);
    }

private:
    void collect_pending_operations(std::vector<ControlOperationPtr>& operations)
    {
        for (auto pending = pending_operations.begin(); pending != pending_operations.end();) {
            if (auto operation = pending->lock()) {
                if (!operation->ready()) {
                    operations.push_back(std::move(operation));
                }
                pending = pending_operations.erase(pending);
            } else {
                pending = pending_operations.erase(pending);
            }
        }
    }

    void discard_finished_operations()
    {
        for (auto pending = pending_operations.begin(); pending != pending_operations.end();) {
            const auto operation = pending->lock();
            if (!operation || operation->ready()) {
                pending = pending_operations.erase(pending);
            } else {
                ++pending;
            }
        }
    }

    static void cancel_operations(const std::vector<ControlOperationPtr>& operations) noexcept
    {
        for (const auto& operation : operations) {
            operation->cancel();
        }
    }

    std::mutex mutex;
    std::condition_variable idle;
    control::EndpointControlHandler handler;
    std::vector<std::weak_ptr<ControlOperation>> pending_operations;
    bool active = true;
    std::size_t in_flight = 0;
    std::thread::id callback_thread;
};

struct HandlerRegistration {
    ResolvedRoute route;
    std::shared_ptr<HandlerSlot> sync_slot;
    std::shared_ptr<EndpointHandlerSlot> endpoint_slot;
};

struct PendingEvent {
    ResolvedRoute route;
    nlohmann::json data = nlohmann::json::object();
};

struct RegistrationRegistry {
    void unregister_slot(const std::shared_ptr<HandlerSlot>& slot) noexcept
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (!sealed) {
                for (auto it = handlers.begin(); it != handlers.end(); ++it) {
                    if (it->sync_slot != slot) {
                        continue;
                    }
                    registered_ids.erase(it->route.protocol_id);
                    registered_names.erase(it->route.name);
                    handlers.erase(it);
                    break;
                }
            }
        }
        slot->deactivate();
    }

    void unregister_endpoint_slot(const std::shared_ptr<EndpointHandlerSlot>& slot) noexcept
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (!sealed) {
                for (auto it = handlers.begin(); it != handlers.end(); ++it) {
                    if (it->endpoint_slot != slot) {
                        continue;
                    }
                    registered_ids.erase(it->route.protocol_id);
                    registered_names.erase(it->route.name);
                    handlers.erase(it);
                    break;
                }
            }
        }
        slot->deactivate();
    }

    void cancel_pending_endpoint_operations() noexcept
    {
        std::vector<std::shared_ptr<EndpointHandlerSlot>> endpoint_slots;
        {
            std::lock_guard<std::mutex> lock(mutex);
            for (const auto& registration : handlers) {
                if (registration.endpoint_slot) {
                    endpoint_slots.push_back(registration.endpoint_slot);
                }
            }
        }
        for (const auto& slot : endpoint_slots) {
            slot->cancel_pending();
        }
    }

    std::mutex mutex;
    bool sealed = false;
    std::vector<HandlerRegistration> handlers;
    std::map<std::uint32_t, std::string> registered_ids;
    std::map<std::string, std::uint32_t> registered_names;
};

} // namespace

struct AxtpControlEndpoint::Impl {
    enum class LifecycleState {
        Stopped,
        Starting,
        Running,
        Stopping,
    };

    control::RegistrationToken register_handler(control::ControlRoute route,
                                                  control::ControlHandler handler)
    {
        if (!handler) {
            throw std::invalid_argument("control handler must not be empty");
        }
        auto resolved = resolve_method_route(route);
        auto slot = std::make_shared<HandlerSlot>(std::move(handler));
        {
            std::lock_guard<std::mutex> lock(registry->mutex);
            if (registry->sealed) {
                throw std::logic_error("control handlers must be registered before endpoint start");
            }
            if (registry->registered_ids.count(resolved.protocol_id) != 0 ||
                registry->registered_names.count(resolved.name) != 0) {
                throw std::invalid_argument("control route collides with an existing registration");
            }
            registry->registered_ids.emplace(resolved.protocol_id, resolved.name);
            registry->registered_names.emplace(resolved.name, resolved.protocol_id);
            registry->handlers.push_back(HandlerRegistration{std::move(resolved), slot, {}});
        }

        return control::RegistrationToken(
            [weak_registry = std::weak_ptr<RegistrationRegistry>(registry),
             weak_slot = std::weak_ptr<HandlerSlot>(slot)]() {
                auto current = weak_slot.lock();
                if (!current) {
                    return;
                }
                if (auto registrations = weak_registry.lock()) {
                    registrations->unregister_slot(current);
                } else {
                    current->deactivate();
                }
            });
    }

    control::RegistrationToken register_endpoint_handler(
        control::ControlRoute route,
        control::EndpointControlHandler handler)
    {
        if (!handler) {
            throw std::invalid_argument("endpoint control handler must not be empty");
        }
        auto resolved = resolve_method_route(route);
        auto slot = std::make_shared<EndpointHandlerSlot>(std::move(handler));
        {
            std::lock_guard<std::mutex> lock(registry->mutex);
            if (registry->sealed) {
                throw std::logic_error("control handlers must be registered before endpoint start");
            }
            if (registry->registered_ids.count(resolved.protocol_id) != 0 ||
                registry->registered_names.count(resolved.name) != 0) {
                throw std::invalid_argument("control route collides with an existing registration");
            }
            registry->registered_ids.emplace(resolved.protocol_id, resolved.name);
            registry->registered_names.emplace(resolved.name, resolved.protocol_id);
            registry->handlers.push_back(HandlerRegistration{std::move(resolved), {}, slot});
        }

        return control::RegistrationToken(
            [weak_registry = std::weak_ptr<RegistrationRegistry>(registry),
             weak_slot = std::weak_ptr<EndpointHandlerSlot>(slot)]() {
                auto current = weak_slot.lock();
                if (!current) {
                    return;
                }
                if (auto registrations = weak_registry.lock()) {
                    registrations->unregister_endpoint_slot(current);
                } else {
                    current->deactivate();
                }
            });
    }

    control::ControlStatus start(AxtpControlEndpointOptions options)
    {
        std::unique_lock<std::mutex> lock(lifecycle_mutex);
        if (lifecycle_state != LifecycleState::Stopped) {
            return control::ControlStatus::invalid_state();
        }
        lifecycle_state = LifecycleState::Starting;
        if (options.bind_address.empty()) {
            options.bind_address = "127.0.0.1";
        }
        if (!options.max_pending_events || !options.max_pending_event_bytes || !options.max_pending_tasks) {
            lifecycle_state = LifecycleState::Stopped;
            return control::ControlStatus::invalid_argument();
        }
        event_limit = options.max_pending_events;
        event_byte_limit = options.max_pending_event_bytes;
        task_limit = options.max_pending_tasks;
        disconnect_requested.store(false);
        std::vector<HandlerRegistration> registrations;
        {
            std::lock_guard<std::mutex> registry_lock(registry->mutex);
            registry->sealed = true;
            registrations = registry->handlers;
        }

        try {
            auto next_broker = std::make_unique<axtp::BasicBroker<>>();
            for (const auto& registration : registrations) {
                const auto& resolved = registration.route;
                next_broker->registry().addMethod(resolved.protocol_id, resolved.name);
                if (registration.sync_slot) {
                    next_broker->registerRawMethod(
                    resolved.protocol_id,
                    [slot = registration.sync_slot,
                     method = resolved.name](const axtp::RpcContext& context,
                                             const axtp::RpcRequestView& request) {
                        control::ControlRequest control_request{
                            context.requestId,
                            method,
                            json_from_bytes(request.body),
                            {},
                            {},
                        };
                        control_request.source_endpoint_id = context.endpoint.src.value_or("");
                        control_request.destination_endpoint_id = context.endpoint.dst.value_or("");
                        auto result = slot->invoke(control_request);
                        if (result.status.code > kMaxAxtpCode) {
                            result.status = control::ControlStatus::internal_error();
                            result.body = nlohmann::json::object();
                        }

                        axtp::RpcResponseData response;
                        response.encoding = axtp::RpcEncoding::Json;
                        response.overrideEncoding = true;
                        response.statusCode =
                            static_cast<axtp::ErrorCode>(static_cast<std::uint16_t>(result.status.code));
                        response.overrideStatus = true;
                        response.body = bytes_from_json(result.body, true);
                        return response;
                    });
                } else {
                    next_broker->registerDeferredRawMethod(
                        resolved.protocol_id,
                        [slot = registration.endpoint_slot,
                         method = resolved.name](const axtp::RpcContext& context,
                                                 const axtp::RpcRequestView& request) {
                            control::ControlRequest control_request{
                                context.requestId,
                                method,
                                json_from_bytes(request.body),
                                {},
                                {},
                            };
                            control_request.source_endpoint_id = context.endpoint.src.value_or("");
                            control_request.destination_endpoint_id = context.endpoint.dst.value_or("");
                            auto operation = slot->invoke(control_request);
                            return [operation = std::move(operation)]()
                                -> std::optional<axtp::RpcResponseData> {
                                if (!operation) {
                                    return endpoint_internal_error();
                                }
                                try {
                                    const auto result = operation->try_result();
                                    if (!result.has_value()) {
                                        return std::nullopt;
                                    }
                                    return endpoint_response(*result);
                                } catch (...) {
                                    return endpoint_internal_error();
                                }
                            };
                        });
                }
            }

            auto next_transport = std::make_unique<transport::WebSocketTransport>(
                options.port, options.bind_address.c_str());
            auto next_endpoint =
                std::make_unique<axtp::AxtpEndpoint<axtp::BasicBroker<>>>(*next_broker);
            next_endpoint->attachTransport(*next_transport);
            auto next_adapter =
                std::make_unique<axtp::WebSocketJsonRpcAdapter>(*next_endpoint, *next_transport);
            next_transport->bind(*next_adapter);
            next_transport->open();
            if (next_transport->localPort() == 0) {
                next_transport->close();
                lifecycle_state = LifecycleState::Stopped;
                lifecycle_changed.notify_all();
                return control::ControlStatus::unavailable();
            }

            local_port.store(next_transport->localPort());
            broker = std::move(next_broker);
            endpoint = std::move(next_endpoint);
            adapter = std::move(next_adapter);
            transport = std::move(next_transport);
            running.store(true);
            worker = std::thread([this]() { run(); });
            lifecycle_state = LifecycleState::Running;
            lifecycle_changed.notify_all();
            return control::ControlStatus::success();
        } catch (const std::exception&) {
            running.store(false);
            cleanup_runtime();
            lifecycle_state = LifecycleState::Stopped;
            lifecycle_changed.notify_all();
            return control::ControlStatus::internal_error();
        }
    }

    control::ControlStatus stop()
    {
        if (g_active_endpoint_worker == this) {
            return control::ControlStatus::invalid_state();
        }
        std::thread joining_worker;
        {
            std::unique_lock<std::mutex> lock(lifecycle_mutex);
            if (lifecycle_state == LifecycleState::Stopping) {
                lifecycle_changed.wait(lock, [this]() {
                    return lifecycle_state != LifecycleState::Stopping;
                });
                return lifecycle_state == LifecycleState::Stopped
                    ? control::ControlStatus::success()
                    : control::ControlStatus::invalid_state();
            }
            if (lifecycle_state == LifecycleState::Stopped) {
                return control::ControlStatus::success();
            }
            if (lifecycle_state != LifecycleState::Running) {
                return control::ControlStatus::invalid_state();
            }
            lifecycle_state = LifecycleState::Stopping;
            running.store(false);
            joining_worker = std::move(worker);
        }

        if (joining_worker.joinable()) {
            joining_worker.join();
        }

        registry->cancel_pending_endpoint_operations();

        {
            std::lock_guard<std::mutex> lock(lifecycle_mutex);
            cleanup_runtime();
            {
                std::lock_guard<std::mutex> event_lock(event_mutex);
                pending_events.clear();
            }
            {
                std::lock_guard<std::mutex> task_lock(task_mutex);
                pending_tasks.clear();
            }
            lifecycle_state = LifecycleState::Stopped;
        }
        lifecycle_changed.notify_all();
        return control::ControlStatus::success();
    }

    control::ControlStatus post(control::ControlTask task)
    {
        if (!task) {
            return control::ControlStatus::invalid_argument();
        }
        if (!running.load()) {
            return control::ControlStatus::unavailable();
        }
        std::lock_guard<std::mutex> lock(task_mutex);
        if (!running.load()) {
            return control::ControlStatus::unavailable();
        }
        if (pending_tasks.size() >= task_limit) return control::ControlStatus::busy();
        pending_tasks.push_back(std::move(task));
        return control::ControlStatus::success();
    }

    control::ControlStatus publish_event(control::ControlRoute route, nlohmann::json data)
    {
        return publish_events({ControlEvent{std::move(route), std::move(data)}});
    }

    control::ControlStatus publish_events(std::vector<ControlEvent> batch)
    {
        std::vector<PendingEvent> resolved_batch;
        std::size_t bytes = 0;
        try {
            for (auto& event : batch) {
                auto resolved = resolve_event_route(event.route);
                const auto size = event.data.dump().size() + resolved.name.size() + 64;
                if (size > event_byte_limit || bytes > event_byte_limit - size) {
                    disconnect_requested.store(true);
                    return control::ControlStatus::busy();
                }
                bytes += size;
                resolved_batch.push_back({std::move(resolved), std::move(event.data)});
            }
        } catch (const std::invalid_argument&) {
            return control::ControlStatus::invalid_argument();
        } catch (const nlohmann::json::exception&) {
            return control::ControlStatus::invalid_argument();
        }
        if (!running.load()) {
            return control::ControlStatus::unavailable();
        }

        {
            std::lock_guard<std::mutex> lock(event_mutex);
            if (!running.load()) {
                return control::ControlStatus::unavailable();
            }
            if (disconnect_requested.load() || resolved_batch.size() > event_limit ||
                pending_events.size() > event_limit - resolved_batch.size() ||
                pending_event_bytes > event_byte_limit - bytes) {
                pending_events.clear();
                pending_event_bytes = 0;
                disconnect_requested.store(true);
                return control::ControlStatus::busy();
            }
            for (auto& event : resolved_batch) {
                event_ids.emplace(event.route.protocol_id, event.route.name);
                event_names.emplace(event.route.name, event.route.protocol_id);
                pending_events.push_back(std::move(event));
            }
            pending_event_bytes += bytes;
        }
        return control::ControlStatus::success();
    }

    void run() noexcept
    {
        WorkerThreadMarker worker_marker(this);
        while (running.load()) {
            try {
                if (disconnect_requested.exchange(false) && transport) transport->disconnectClients();
                if (adapter != nullptr && transport != nullptr) {
                    adapter->poll(*transport);
                }
                if (endpoint != nullptr) {
                    endpoint->progress();
                }
                if (!running.load()) {
                    break;
                }
                run_pending_tasks();
                if (!running.load()) {
                    break;
                }
                send_pending_events();
            } catch (const std::exception&) {
            } catch (...) {
            }
            std::this_thread::sleep_for(kWorkerPollInterval);
        }
    }

    void run_pending_tasks()
    {
        std::deque<control::ControlTask> tasks;
        {
            std::lock_guard<std::mutex> lock(task_mutex);
            tasks.swap(pending_tasks);
        }
        for (auto& task : tasks) {
            if (!running.load()) {
                break;
            }
            try {
                task();
            } catch (const std::exception&) {
            } catch (...) {
            }
        }
    }

    void send_pending_events()
    {
        std::deque<PendingEvent> events;
        {
            std::lock_guard<std::mutex> lock(event_mutex);
            events.swap(pending_events);
            pending_event_bytes = 0;
        }
        if (adapter == nullptr) {
            return;
        }
        for (auto& pending : events) {
            if (!running.load()) {
                break;
            }
            axtp::RpcPayload event;
            event.encoding = axtp::RpcEncoding::Json;
            event.op = axtp::RpcOp::Event;
            event.methodOrEventId = pending.route.protocol_id;
            event.bodyEncoding = axtp::RpcBodyEncoding::None;
            event.meta.sourceProtocol = axtp::SourceProtocol::JsonRpc;
            event.meta.jsonMethodOrEventName = pending.route.name;
            event.body = bytes_from_json(pending.data, false);
            adapter->sendEvent(std::move(event));
        }
    }

    void cleanup_runtime() noexcept
    {
        if (transport != nullptr) {
            transport->close();
        }
        adapter.reset();
        endpoint.reset();
        transport.reset();
        broker.reset();
        local_port.store(0);
    }

    mutable std::mutex lifecycle_mutex;
    std::condition_variable lifecycle_changed;
    LifecycleState lifecycle_state = LifecycleState::Stopped;
    std::shared_ptr<RegistrationRegistry> registry = std::make_shared<RegistrationRegistry>();

    std::mutex task_mutex;
    std::deque<control::ControlTask> pending_tasks;

    std::mutex event_mutex;
    std::deque<PendingEvent> pending_events;
    std::size_t pending_event_bytes = 0;
    std::atomic<std::size_t> event_limit{1024};
    std::atomic<std::size_t> event_byte_limit{4U * 1024U * 1024U};
    std::atomic<std::size_t> task_limit{1024};
    std::atomic<bool> disconnect_requested{false};
    std::map<std::uint32_t, std::string> event_ids;
    std::map<std::string, std::uint32_t> event_names;

    std::unique_ptr<axtp::BasicBroker<>> broker;
    std::unique_ptr<transport::WebSocketTransport> transport;
    std::unique_ptr<axtp::AxtpEndpoint<axtp::BasicBroker<>>> endpoint;
    std::unique_ptr<axtp::WebSocketJsonRpcAdapter> adapter;
    std::atomic<bool> running{false};
    std::atomic<std::uint16_t> local_port{0};
    std::thread worker;
};

AxtpControlEndpoint::AxtpControlEndpoint()
    : impl_(std::make_unique<Impl>())
{
}

AxtpControlEndpoint::~AxtpControlEndpoint()
{
    (void)impl_->stop();
}

control::RegistrationToken AxtpControlEndpoint::register_handler(control::ControlRoute route,
                                                                  control::ControlHandler handler)
{
    return impl_->register_handler(std::move(route), std::move(handler));
}

control::RegistrationToken AxtpControlEndpoint::register_endpoint_handler(
    control::ControlRoute route,
    control::EndpointControlHandler handler)
{
    return impl_->register_endpoint_handler(std::move(route), std::move(handler));
}

control::ControlStatus AxtpControlEndpoint::start(AxtpControlEndpointOptions options)
{
    return impl_->start(std::move(options));
}

control::ControlStatus AxtpControlEndpoint::stop()
{
    return impl_->stop();
}

bool AxtpControlEndpoint::running() const noexcept
{
    return impl_->running.load();
}

std::uint16_t AxtpControlEndpoint::local_port() const noexcept
{
    return impl_->local_port.load();
}

control::ControlStatus AxtpControlEndpoint::post(control::ControlTask task)
{
    return impl_->post(std::move(task));
}

control::ControlStatus AxtpControlEndpoint::publish_event(control::ControlRoute route,
                                                           nlohmann::json data)
{
    return impl_->publish_event(std::move(route), std::move(data));
}

control::ControlStatus AxtpControlEndpoint::publish_events(std::vector<ControlEvent> events)
{
    return impl_->publish_events(std::move(events));
}

} // namespace axent
