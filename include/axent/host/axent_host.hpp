#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "axent/adapters/axtp_adapter.hpp"
#include "axent/core/types.hpp"
#include "axent/firmware/firmware_update_service.hpp"
#include "axent/input/input_route.hpp"
#include "axent/media/media_frame.hpp"
#include "axent/media/media_relay.hpp"
#include "axent/media/media_subscription.hpp"
#include "axent/media/video_stream_params.hpp"

namespace axent {

class Broker;
class MediaStreamRelay;

struct AxentHostOptions {
    AxentHostOptions();

    using AdapterFactory = std::function<std::unique_ptr<Adapter>(AxtpAdapterConfig)>;

    bool enable_mock_adapter = true;
    bool enable_axtp_adapter = false;
    AxtpAdapterConfig axtp;
    AdapterFactory axtp_adapter_factory;
};

struct SessionAcquireRequest {
    std::string client_id;
    std::string device_id;
    bool media = false;
};

struct SessionLease {
    bool acquired = false;
    std::string session_id;
    std::string device_id;
    std::string client_id;
    bool media = false;
    std::string reason;
    ControlStatus status = ControlStatus::Ok;
};

struct EndpointControlRequest {
    std::string source_endpoint_id;
    std::string destination_endpoint_id;
    std::string method;
    nlohmann::json params = nlohmann::json::object();
};

class MediaConsumer {
public:
    std::optional<MediaFrame> read();
    MediaRelayStats stats() const;

private:
    friend class AxentHost;

    explicit MediaConsumer(std::shared_ptr<MediaStreamRelay> relay);

    std::shared_ptr<MediaStreamRelay> relay_;
};

class AxentHost {
public:
    AxentHost();
    ~AxentHost();

    AxentHost(const AxentHost&) = delete;
    AxentHost& operator=(const AxentHost&) = delete;
    AxentHost(AxentHost&&) = delete;
    AxentHost& operator=(AxentHost&&) = delete;

    bool start(AxentHostOptions options = {});
    // Host lifecycle mutation is not re-entrant from media sink callbacks:
    // start returns false, acquire returns Busy, and stop/release throw
    // std::logic_error. Media publication also returns false. Subscription
    // cancel and call() remain supported.
    void stop();
    bool running() const;

    std::vector<DeviceSnapshot> discover_devices() const;
    std::vector<DeviceSnapshot> refresh_devices();
    TransportDiagnostics transport_diagnostics() const;
    // Return diagnostics for one physical device.  The no-argument form is
    // retained for compatibility and returns the adapter's aggregate view.
    TransportDiagnostics transport_diagnostics(const std::string& device_id) const;
    void upsert_device(DeviceSnapshot snapshot);
    SessionLease acquire_session(const SessionAcquireRequest& request);
    void release_session(const std::string& session_id, const std::string& reason);
    firmware::MaintenanceLeaseProvider& maintenance_lease_provider();

    std::unique_ptr<MediaConsumer> create_media_consumer(const std::string& session_id,
                                                         MediaRelayOptions options);
    MediaSubscriptionPtr subscribe_media(const std::string& session_id,
                                         std::shared_ptr<IMediaFrameSink> sink,
                                         MediaSubscriptionOptions options = {});
    MediaStreamSubscriptionPtr subscribe_media_stream(
        const std::string& session_id,
        std::shared_ptr<IMediaStreamSink> sink,
        MediaSubscriptionOptions options = {});
    bool publish_media_stream_event(const std::string& session_id,
                                    MediaStreamEvent event);
    bool publish_media_frame(const std::string& session_id, MediaFrame frame);

    VideoStreamParamsResult set_video_stream_params(
        const std::string& session_id,
        const VideoStreamParamsRequest& request);
    VideoStreamParamsState video_stream_params_state(const std::string& session_id) const;
    VideoStreamParamsSubscriptionPtr subscribe_video_stream_params(
        const std::string& session_id,
        VideoStreamParamsObserver observer);

    ControlResult call(const std::string& session_id,
                       const std::string& method,
                       const nlohmann::json& params);
    ControlOperationPtr call_async(
        const std::string& session_id,
        const std::string& method,
        const nlohmann::json& params,
        ControlCallOptions options = {});
    ControlOperationPtr call_endpoint(
        EndpointControlRequest request,
        ControlCallOptions options = {});

    input::StreamOpenResult open_input_stream(
        const std::string& session_id,
        const input::StreamOpenRequest& request);
    // Authenticated media integration only. Withdraw with route_ready=false
    // using the same binding and physical generation; input producers cannot
    // mint source identities. A ready replacement requires confirmed cleanup.
    bool set_input_source(const std::string& session_id,
                          input::SourceDescriptor source,
                          std::string* reason = nullptr);
    std::uint64_t input_session_generation(const std::string& session_id) const;
    std::optional<input::SourceDescriptor> input_source(const std::string& session_id) const;
    input::StreamRouteSnapshot input_stream_snapshot(const std::string& session_id) const;
    input::StreamResult send_input_frame(
        const std::string& session_id,
        const input::StreamFrame& frame,
        std::string* reason = nullptr);
    input::StreamResult close_input_stream(
        const std::string& session_id,
        const input::StreamHandle& handle,
        std::string reason = {});

    // AxentHost serializes its own lifecycle/session/media state. The returned
    // reference is valid only until stop() or the next start(), and those calls
    // must not race with users such as ControlPlane that hold this reference.
    Broker& broker();

private:
    bool reserve_maintenance(const std::string& device_id,
                             std::string& reason,
                             std::function<void()>& release);
    bool publish_media_stream_event_for_session(const std::string& session_id,
                                                MediaStreamEvent event);
    bool publish_media_frame_for_device(std::string device_id, MediaFrame frame);
    bool publish_media_stream_event_for_device(MediaStreamEvent event);

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace axent
