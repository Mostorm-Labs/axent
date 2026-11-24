#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <set>
#include <thread>
#include <utility>
#include <vector>

#include "axent/core/adapter.hpp"
#include "axent/input/input_route.hpp"
#include "axent/media/media_frame.hpp"
#include "axent/media/media_stream.hpp"
#include "axent/media/video_stream_params.hpp"
#include "axent/transport/types.hpp"

namespace axent {

namespace detail {
class AxtpAdapterRuntimeFactory;
} // namespace detail

namespace testing {
class AxtpAdapterTestSeam;
} // namespace testing

struct AxtpAdapterConfig {
    TransportSelector selector;
    bool enable_media = true;
    bool enable_video = true;
    bool enable_audio = true;
    std::uint32_t audio_sample_rate = 48000;
    std::uint32_t audio_channels = 2;
    std::optional<std::uint32_t> video_frame_rate;
    // Ordered, generic codec preferences supplied by the product host.  The
    // adapter intersects these with the peer capabilities; it does not own
    // product/backend policy.  Empty retains legacy H.264 behavior.
    std::vector<MediaCodec> video_codec_preferences;
    // Compatibility escape hatch for peers whose control contract only
    // accepts H.264 while the media payload is known to be H.265.  The host
    // must set both overrides together; normal negotiation leaves them empty.
    std::optional<MediaCodec> video_open_codec_override;
    std::optional<MediaCodec> video_decode_codec_override;
    std::string video_source = "wireless_cast";
    std::string audio_source = "wireless_cast_audio";
    bool enable_session_health_probe = true;
    SessionProbeMode session_probe_mode = SessionProbeMode::Auto;
    // Advisory OPEN value.  A valid peer ACCEPT value is exposed through
    // TransportDiagnostics and controls the active heartbeat schedule.
    std::uint32_t requested_heartbeat_interval_ms = 1000;
    std::uint32_t session_health_probe_interval_ms = 1000;
    std::uint32_t session_health_probe_timeout_ms = 250;
    std::uint32_t session_health_failure_threshold = 3;
    std::uint32_t session_recovery_backoff_initial_ms = 1000;
    std::uint32_t session_recovery_backoff_max_ms = 5000;
    EndpointDeliveryMode endpoint_delivery_mode =
        EndpointDeliveryMode::LocalProjection;
};

class AxentHost;

class AxtpAdapter final : public Adapter {
public:
    using MediaFrameCallback = std::function<void(std::string device_id, MediaFrame frame)>;
    using MediaStreamEventCallback = std::function<void(MediaStreamEvent event)>;

    AxtpAdapter();
    explicit AxtpAdapter(AxtpAdapterConfig config);
    ~AxtpAdapter() override;

    static AxtpAdapterConfig na20_defaults();
    static DeviceSnapshot snapshot_from_descriptor(
        const TransportDescriptor& descriptor,
        EndpointDeliveryMode endpoint_delivery_mode =
            EndpointDeliveryMode::LocalProjection);

    AdapterMetadata metadata() const override;
    std::vector<Capability> capabilities() const override;
    std::vector<DeviceSnapshot> discover() override;
    ControlResult call(const std::string& device_id, const std::string& method, const nlohmann::json& params) override;
    ControlResult call(const AdapterControlRequest& request) override;
    ControlOperationPtr call_async(
        const std::string& device_id,
        const std::string& method,
        const nlohmann::json& params,
        ControlCallOptions options = {}) override;
    ControlOperationPtr call_async(
        const AdapterControlRequest& request,
        ControlCallOptions options = {}) override;
    ControlResult start_firmware_update(const std::string& device_id, const std::string& file_path) override;
    ControlResult start_firmware_update(
        const AdapterControlRequest& request,
        const std::string& file_path) override;

    TransportDiagnostics diagnostics() const;
    // Return diagnostics for one physical AXTP device. The no-argument form
    // remains available for compatibility and returns an aggregate snapshot
    // when more than one device context is active.
    TransportDiagnostics diagnostics(const std::string& device_id) const;
    void set_media_frame_callback(MediaFrameCallback callback);
    void set_media_stream_event_callback(MediaStreamEventCallback callback);
    std::vector<MediaStreamDescriptor> active_media_stream_descriptors() const;
    std::vector<MediaStreamDescriptor> active_media_stream_descriptors(
        const std::string& device_id) const;
    ControlStatus open_session_status(const std::string& device_id,
                                      std::string& error,
                                      bool configure_media = true);
    bool open_session(const std::string& device_id, std::string& error);
    // Install the capability/source binding received from the authenticated
    // media/device integration.  A producer request cannot create it.
    // Withdraw with route_ready=false and the same binding/generation. This
    // fences input immediately while retaining close ownership for cleanup.
    bool set_input_source(const std::string& device_id,
                          input::SourceDescriptor source,
                          std::string* reason = nullptr);
    std::uint64_t input_session_generation(const std::string& device_id) const;
    std::optional<input::SourceDescriptor> input_source(const std::string& device_id) const;
    input::StreamRouteSnapshot input_stream_snapshot(const std::string& device_id) const;
    VideoStreamParamsResult set_video_stream_params(
        const std::string& device_id,
        const VideoStreamParamsRequest& request);
    VideoStreamParamsState video_stream_params_state(const std::string& device_id) const;
    VideoStreamParamsSubscriptionPtr subscribe_video_stream_params(
        const std::string& device_id,
        VideoStreamParamsObserver observer);

    // Typed input uses the already-open physical AXTP session.  These methods
    // do not create a second transport or tunnel data through JSON-RPC.
    input::StreamOpenResult open_input_stream(
        const std::string& device_id,
        const input::StreamOpenRequest& request);
    input::StreamResult send_input_frame(
        const std::string& device_id,
        const input::StreamFrame& frame,
        std::string* reason = nullptr);
    input::StreamResult close_input_stream(
        const std::string& device_id,
        const input::StreamHandle& handle,
        std::string reason = {});

private:
    friend class AxentHost;
    friend class testing::AxtpAdapterTestSeam;

    AxtpAdapter(AxtpAdapterConfig config,
                std::shared_ptr<detail::AxtpAdapterRuntimeFactory> runtime_factory,
                bool device_context = false);

    struct DeviceContext;
    class DeviceContextOperation;
    std::shared_ptr<DeviceContext> device_context_for(
        const std::string& device_id,
        bool create = true) const;
    std::shared_ptr<DeviceContext> find_device_context(
        const std::string& device_id) const;
    std::vector<std::shared_ptr<DeviceContext>> device_contexts() const;
    void install_device_context_callbacks(const std::shared_ptr<DeviceContext>& context) const;
    void update_selector_from_descriptor(const TransportDescriptor& descriptor);

    std::thread request_stop_session_pump_locked();
    void process_next_control_call(const std::string& device_id);
    void expire_pending_control_calls(std::chrono::steady_clock::time_point now);
    void cancel_control_calls(
        const std::optional<std::string>& device_id,
        const std::optional<std::uint64_t>& physical_generation,
        std::string reason);
    bool ensure_session_locked(const std::string& device_id,
                               std::string& error,
                               ControlStatus& status,
                               bool configure_media,
                               MediaStreamEventReason open_reason =
                                   MediaStreamEventReason::InitialOpen);
    void refresh_diagnostics_locked();
    void record_transport_trace(const std::string& event_name,
                                bool accepted_read,
                                bool write_report,
                                bool read_error,
                                bool write_error,
                                bool dropped_report,
                                const std::string& message);
    void drop_pending_media_frames_for_device(const std::string& device_id);
    void bind_media_delivery_session(const std::string& device_id,
                                     const std::string& session_id);
    void unbind_media_delivery_session(const std::string& device_id,
                                       const std::string& session_id);
    void reset_session_for_device(const std::string& device_id);
    bool media_configure_retry_due_locked(std::chrono::steady_clock::time_point now) const;
    void configure_media_streams(
        const std::string& device_id,
        MediaStreamEventReason open_reason = MediaStreamEventReason::InitialOpen);
    bool configure_media_stream_kind(const std::string& device_id,
                                     MediaKind kind,
                                     bool update_retry_state,
                                     bool from_video_reconfigure,
                                     MediaStreamEventReason open_reason =
                                         MediaStreamEventReason::InitialOpen);
    void advance_video_reconfigure(const std::string& device_id);
    void clear_video_stream_params_session(bool preserve_logical_session = false);
    void notify_video_stream_params_state(VideoStreamParamsState state);
    void retry_pending_media_source_recoveries(
        const std::string& device_id,
        std::chrono::steady_clock::time_point now);
    void clear_media_streams(MediaStreamEventReason reason = MediaStreamEventReason::Shutdown);
    void enqueue_media_stream_events(std::vector<MediaStreamEvent> events);
    struct MediaSourceStateEvent {
        std::uint32_t event_id = 0;
        std::string event_name;
        MediaKind kind = MediaKind::Unknown;
        std::string source;
        std::string state;
        std::string reason;
        std::uint32_t active_stream_id = 0;
        bool has_active_stream_id = false;
        bool valid = false;
        std::uint64_t order = 0;
        bool deferred_client_action = false;
    };
    void enqueue_media_source_state_event(MediaSourceStateEvent event);
    bool has_pending_media_source_state_events() const;
    void process_pending_media_source_state_events(
        const std::string& device_id,
        bool allow_client_calls = true);
    void process_media_source_state_event(const std::string& device_id,
                                          const MediaSourceStateEvent& event);
    void run_pending_orphan_stream_closes(const std::string& device_id);
    void handle_stream_payload(const std::string& device_id,
                               std::uint32_t stream_id,
                               std::uint32_t sequence_id,
                               std::uint64_t cursor,
                               std::vector<std::uint8_t> data,
                               std::uint64_t ingress_token = 0);
    MediaFrame frame_from_stream(const std::string& device_id,
                                 std::uint32_t stream_id,
                                 std::uint32_t sequence_id,
                                 std::uint64_t cursor,
                                 std::vector<std::uint8_t> data,
                                 std::uint64_t ingress_token = 0) const;
    bool is_current_media_frame(const MediaFrame& frame) const;
    void commit_pending_media_batch();
    void drain_pending_media_callbacks();
    void notify_media_dispatch();
    void run_media_dispatcher();
    void request_session_recovery(const std::string& device_id, std::string reason);
    void run_session_recovery_worker();
    bool recover_session_once(const std::string& device_id,
                              std::string& error,
                              std::uint64_t recovery_generation);
    bool session_health_probe_due(std::chrono::steady_clock::time_point now);
    bool run_session_health_probe(const std::string& device_id);
    void set_session_health(SessionHealthState state,
                            std::uint32_t probe_failures,
                            std::string reason = {});
    bool has_media_delivery_session(const std::string& device_id) const;
    void note_inbound_activity();
    void sync_runtime_activity();
    void publish_runtime_progress(const std::string& device_id);
    void snapshot_transport_diagnostics_from_runtime(bool force = false);
    bool run_legacy_capabilities_probe(const std::string& device_id);
    bool run_control_heartbeat_probe(const std::string& device_id,
                                     bool* transport_error,
                                     bool* had_other_activity);

    AxtpAdapterConfig config_;
    // A public AxtpAdapter is a device manager. Each discovered physical
    // device owns a private leaf AxtpAdapter, which keeps the mature AXTP
    // session/poll/recovery/media state isolated without exposing runtime
    // types through Axent's public API.
    bool device_context_ = false;
    // Serialize first-use leaf construction. The map mutex is intentionally
    // not held while a leaf starts its worker threads or receives callbacks.
    mutable std::mutex device_creation_mutex_;
    mutable std::mutex device_context_mutex_;
    mutable std::map<std::string, std::shared_ptr<DeviceContext>> device_contexts_;
    // Device IDs in this set are temporarily unavailable while their retired
    // leaf drains. New callers fail fast instead of creating a replacement
    // transport before the old one has fully closed.
    mutable std::set<std::string> retiring_device_ids_;
    mutable std::map<std::string, TransportDescriptor> transport_descriptors_;
    // Canonical HID identities observed on more than one physical
    // path/interface are withheld until discovery becomes unambiguous.
    mutable std::set<std::string> ambiguous_device_ids_;
    // A selector that already names one serial/path is a single-device
    // configuration. Reserve its first logical ID when discovery metadata is
    // unavailable so a second ID cannot accidentally reopen the same handle.
    mutable std::string fixed_selector_device_id_;
    struct ManagerCallbackState;
    std::shared_ptr<ManagerCallbackState> manager_callback_state_;
    struct RuntimeState;
    std::unique_ptr<RuntimeState> runtime_;
    // A descriptor can change HID path after a replug while retaining its
    // serial-number identity. Selector updates are synchronized with the
    // next physical open/recovery and never mutate an in-flight transport.
    mutable std::mutex selector_mutex_;
    MediaFrameCallback media_frame_callback_;
    MediaStreamEventCallback media_stream_event_callback_;
    mutable std::mutex media_callback_mutex_;
    mutable std::recursive_mutex media_callback_dispatch_mutex_;
    bool draining_media_callbacks_ = false;
    struct ActiveMediaStream {
        MediaStreamDescriptor descriptor;
    };
    mutable std::mutex media_stream_mutex_;
    std::map<std::uint32_t, ActiveMediaStream> active_media_streams_;
    std::map<std::uint32_t, std::uint64_t> media_stream_generations_;
    // Source recovery closes both receiver-pull legs once before reopening.
    std::optional<MediaStreamDescriptor> pending_video_source_recovery_close_;
    std::optional<MediaStreamDescriptor> pending_audio_source_recovery_close_;
    bool source_recovery_cycle_active_ = false;
    bool source_recovery_close_sent_ = false;
    std::uint8_t source_recovery_reopen_mask_ = 0;
    struct PendingMediaIngressFrame {
        // Capture the complete delivery identity at the runtime callback
        // boundary.  A source lifecycle event from the same poll can replace
        // a same-ID stream before the batch is committed, and a Host lease
        // can be rebound before the dispatcher runs.  Looking either value up
        // later would incorrectly deliver an old physical frame as new.
        MediaFrame frame;
    };
    struct PendingMediaDispatchItem {
        enum class Kind {
            StreamEvent,
            Frame,
        } kind = Kind::StreamEvent;
        MediaStreamEvent event;
        MediaFrame frame;
    };
    mutable std::mutex pending_media_source_state_mutex_;
    std::queue<MediaSourceStateEvent> pending_media_source_state_events_;
    // Streamable events may require close/open RPCs.  A callRaw progress hook
    // reconciles pure lifecycle immediately but defers those client-owning
    // actions until the outer RPC returns to the pump.
    std::queue<MediaSourceStateEvent> deferred_media_source_state_events_;
    std::uint64_t next_media_source_state_order_ = 0;
    std::uint64_t latest_video_source_state_order_ = 0;
    std::uint64_t latest_audio_source_state_order_ = 0;
    // A single open transition can observe more than one foreign/future ID.
    // Retain each candidate until the response identifies which terminal
    // belongs to this open; a later unrelated stale ID must not overwrite it.
    std::map<std::uint32_t, std::uint64_t> pending_video_open_terminal_orders_;
    std::map<std::uint32_t, std::uint64_t> pending_audio_open_terminal_orders_;
    std::queue<std::pair<MediaKind, std::uint32_t>> pending_orphan_stream_closes_;
    std::mutex pending_media_ingress_mutex_;
    std::queue<PendingMediaIngressFrame> pending_media_ingress_frames_;
    std::mutex pending_media_dispatch_mutex_;
    std::queue<PendingMediaDispatchItem> pending_media_dispatch_items_;
    std::mutex media_dispatch_mutex_;
    std::condition_variable media_dispatch_cv_;
    std::uint64_t media_dispatch_generation_ = 0;
    bool stop_media_dispatcher_ = false;
    std::thread media_dispatcher_;
    mutable std::mutex media_delivery_session_mutex_;
    std::map<std::string, std::string> media_delivery_sessions_;
    // Monotonic logical binding fence. It advances on every bind/unbind so a
    // stream payload decoded before a lease handoff cannot be delivered after
    // the replacement lease, even when Core deferred its broker callback.
    std::atomic<std::uint64_t> media_binding_epoch_{1};
    mutable std::mutex mutex_;
    mutable std::mutex session_mutex_;
    mutable std::mutex client_mutex_;
    TransportDiagnostics diagnostics_;
    std::chrono::steady_clock::time_point next_media_configure_attempt_;
    std::uint32_t media_configure_attempts_ = 0;
    std::chrono::steady_clock::time_point next_video_configure_attempt_;
    std::chrono::steady_clock::time_point next_audio_configure_attempt_;
    std::uint32_t video_configure_retry_attempt_ = 0;
    std::uint32_t audio_configure_retry_attempt_ = 0;
    bool video_source_terminal_ = false;
    bool audio_source_terminal_ = false;
    bool video_source_waiting_ = false;
    bool audio_source_waiting_ = false;
    bool video_source_recovery_pending_ = false;
    bool audio_source_recovery_pending_ = false;
    std::chrono::steady_clock::time_point next_video_source_recovery_attempt_;
    std::chrono::steady_clock::time_point next_audio_source_recovery_attempt_;
    std::string active_device_id_;
    std::atomic<bool> stop_session_pump_{false};
    std::thread session_pump_;
    struct PendingControlCall;
    std::mutex control_mutex_;
    std::condition_variable control_cv_;
    std::deque<std::shared_ptr<PendingControlCall>> pending_control_calls_;
    std::shared_ptr<PendingControlCall> in_flight_control_call_;
    bool accepting_control_calls_ = true;
    std::atomic<std::uint64_t> control_queue_depth_{0};
    std::atomic<std::uint64_t> control_in_flight_{0};
    std::atomic<std::uint64_t> control_outstanding_high_water_{0};
    std::atomic<std::uint64_t> media_dispatch_queue_depth_{0};
    std::atomic<std::uint64_t> media_dispatch_queue_high_water_{0};
    std::atomic<std::uint64_t> media_frames_dispatched_during_control_call_{0};
    // Keep media staged while an internal open/reopen RPC is deciding the
    // next stream generation.  Runtime progress may otherwise publish an old
    // generation immediately before its Closed/Opened lifecycle entries.
    std::atomic<std::uint32_t> video_lifecycle_transition_depth_{0};
    std::atomic<std::uint32_t> audio_lifecycle_transition_depth_{0};
    std::chrono::steady_clock::time_point next_transport_diagnostics_snapshot_;
    bool transport_counters_snapshot_owned_ = false;
    std::mutex recovery_mutex_;
    std::condition_variable recovery_cv_;
    std::thread recovery_worker_;
    bool stop_recovery_worker_ = false;
    bool recovery_requested_ = false;
    std::string recovery_device_id_;
    std::string recovery_reason_;
    std::uint32_t recovery_attempt_ = 0;
    std::chrono::steady_clock::time_point next_health_probe_;
    std::chrono::steady_clock::time_point last_transport_activity_;
    std::uint64_t inbound_activity_generation_ = 0;
    std::uint64_t last_runtime_activity_generation_ = 0;
    std::uint64_t last_probe_activity_generation_ = 0;
    SessionProbeMode effective_probe_mode_ = SessionProbeMode::Auto;
    std::uint32_t negotiated_heartbeat_interval_ms_ = 0;
    std::uint64_t heartbeat_attempts_ = 0;
    std::uint64_t heartbeat_acks_ = 0;
    std::uint64_t heartbeat_timeouts_ = 0;
    std::uint64_t legacy_probe_attempts_ = 0;
    std::uint64_t legacy_probe_successes_ = 0;
    std::uint64_t legacy_fallbacks_ = 0;
    std::string legacy_fallback_reason_;
    SessionHealthState session_health_ = SessionHealthState::Healthy;
    std::uint32_t health_probe_failures_ = 0;
    std::uint64_t session_recoveries_ = 0;
    std::string last_session_recovery_reason_;
    std::atomic<std::uint64_t> recovery_generation_{0};
};

} // namespace axent
