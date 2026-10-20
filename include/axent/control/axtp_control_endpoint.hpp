#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "axent/control/control_contract.hpp"

namespace axent {

struct AxtpControlEndpointOptions {
    std::string bind_address = "127.0.0.1";
    std::uint16_t port = 0;
    std::size_t max_pending_events = 1024;
    std::size_t max_pending_event_bytes = 4U * 1024U * 1024U;
    std::size_t max_pending_tasks = 1024;
};

struct ControlEvent {
    control::ControlRoute route;
    nlohmann::json data;
};

class AxtpControlEndpoint final {
public:
    AxtpControlEndpoint();
    ~AxtpControlEndpoint();

    AxtpControlEndpoint(const AxtpControlEndpoint&) = delete;
    AxtpControlEndpoint& operator=(const AxtpControlEndpoint&) = delete;
    AxtpControlEndpoint(AxtpControlEndpoint&&) = delete;
    AxtpControlEndpoint& operator=(AxtpControlEndpoint&&) = delete;

    control::RegistrationToken register_handler(control::ControlRoute route,
                                                  control::ControlHandler handler);
    control::RegistrationToken register_endpoint_handler(
        control::ControlRoute route,
        control::EndpointControlHandler handler);

    control::ControlStatus start(AxtpControlEndpointOptions options = {});
    control::ControlStatus stop();
    bool running() const noexcept;
    std::uint16_t local_port() const noexcept;

    control::ControlStatus post(control::ControlTask task);

    control::ControlStatus publish_event(control::ControlRoute route,
                                         nlohmann::json data = nlohmann::json::object());
    // All parts are admitted atomically; overflow disconnects control clients
    // for resynchronization, without stopping the listener or media owners.
    control::ControlStatus publish_events(std::vector<ControlEvent> events);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace axent
