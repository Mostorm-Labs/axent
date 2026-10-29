#include "axent/adapters/axtp_adapter.hpp"
#include "axent/adapters/axtp_endpoint_identity.hpp"
#include "axtp_adapter_test_seam.hpp"

#include "axtp_adapter_internal.hpp"
#include "../core/control_operation_internal.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <iomanip>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "axtp_runtime.hpp"
#include "axtp_sdk.hpp"
#include "hidapi/hid_transport.hpp"

namespace axent {
namespace {

constexpr auto kMediaConfigureRetryInterval = std::chrono::seconds(1);
constexpr auto kSourceWaitingFallbackInterval = std::chrono::seconds(15);

void update_high_water(std::atomic<std::uint64_t>& target,
                       std::uint64_t candidate)
{
    auto current = target.load(std::memory_order_relaxed);
    while (current < candidate &&
           !target.compare_exchange_weak(
               current,
               candidate,
               std::memory_order_relaxed,
               std::memory_order_relaxed)) {
    }
}

std::chrono::milliseconds media_retry_delay(std::uint32_t attempt,
                                             bool source_waiting,
                                             MediaKind kind)
{
    if (source_waiting) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            kSourceWaitingFallbackInterval);
    }
    constexpr std::uint32_t kSeconds[] = {1, 2, 4, 8, 15};
    const auto index = std::min<std::size_t>(
        (sizeof(kSeconds) / sizeof(kSeconds[0])) - 1,
        attempt == 0 ? 0 : static_cast<std::size_t>(attempt - 1));
    const auto baseMs = static_cast<std::uint64_t>(kSeconds[index]) * 1000ULL;
    // Deterministic per-kind jitter keeps retries de-synchronised without
    // introducing a process-global RNG into the adapter's pump thread.
    const auto seed = static_cast<std::uint64_t>(attempt + 1U) * 1103515245ULL +
        (kind == MediaKind::Audio ? 0x5EEDULL : 0xC0DEULL);
    const auto jitterPermille = static_cast<std::int64_t>(seed % 201ULL) - 100LL;
    const auto adjusted = baseMs * static_cast<std::uint64_t>(1000LL + jitterPermille) / 1000ULL;
    return std::chrono::milliseconds(std::max<std::uint64_t>(100ULL, adjusted));
}

std::chrono::milliseconds heartbeat_schedule_delay(std::uint32_t intervalMs,
                                                   std::uint64_t generation)
{
    // Probe no later than the negotiated deadline, with a deterministic
    // negative jitter in the required 90..100% window.  Determinism keeps the
    // pump reproducible while still avoiding a fleet-wide phase lock.
    const auto bounded = std::clamp<std::uint32_t>(intervalMs, 500U, 60000U);
    const auto percent = 900U + static_cast<std::uint32_t>((generation * 1103515245ULL) % 101ULL);
    return std::chrono::milliseconds(
        std::max<std::uint32_t>(1U, bounded * percent / 1000U));
}

std::string hex4(std::uint16_t value)
{
    std::ostringstream out;
    out << std::hex << std::nouppercase << std::setfill('0') << std::setw(4)
        << static_cast<unsigned int>(value);
    return out.str();
}

std::string descriptor_id_for(const axent::transport::HidDeviceInfo& device)
{
    if (!device.serialNumber.empty()) {
        return "hid:" + hex4(device.vendorId) + ":" + hex4(device.productId) + ":" + device.serialNumber;
    }
    if (!device.path.empty()) {
        return "hid:" + hex4(device.vendorId) + ":" + hex4(device.productId) + ":" + device.path;
    }
    return "hid:" + hex4(device.vendorId) + ":" + hex4(device.productId);
}

bool populate_selector_identity_from_device_id(const std::string& device_id,
                                               TransportDescriptor& descriptor)
{
    // DeviceManager snapshots created by a product host may reach the adapter
    // without a preceding HID enumeration (notably embedded/test hosts). Keep
    // the physical selector internal, but recover the conventional
    // hid:<vid>:<pid>:<serial> identity so two logical contexts do not both
    // reopen an arbitrary first HID handle.
    constexpr const char* prefix = "hid:";
    if (device_id.rfind(prefix, 0) != 0) {
        return false;
    }
    const auto vendor_end = device_id.find(':', 4);
    if (vendor_end == std::string::npos) {
        return false;
    }
    const auto product_end = device_id.find(':', vendor_end + 1);
    if (product_end == std::string::npos || product_end + 1 >= device_id.size()) {
        return false;
    }
    const auto parse_hex4 = [](const std::string& value, std::uint16_t& output) {
        // descriptor_id_for() emits canonical, fixed-width VID/PID fields.
        // Reject uppercase aliases, signs, whitespace and abbreviated values
        // rather than letting std::stoul() accept a merely numeric prefix.
        if (value.size() != 4 ||
            !std::all_of(value.begin(), value.end(), [](unsigned char ch) {
                return (ch >= '0' && ch <= '9') ||
                    (ch >= 'a' && ch <= 'f');
            })) {
            return false;
        }
        try {
            std::size_t parsed = 0;
            const auto numeric = std::stoul(value, &parsed, 16);
            if (parsed == value.size() && numeric <= 0xffffU) {
                output = static_cast<std::uint16_t>(numeric);
                return true;
            }
        } catch (const std::exception&) {
        }
        return false;
    };
    if (!parse_hex4(device_id.substr(4, vendor_end - 4), descriptor.vendor_id)) {
        return false;
    }
    const auto product_begin = vendor_end + 1;
    if (!parse_hex4(
            device_id.substr(product_begin, product_end - product_begin),
            descriptor.product_id)) {
        return false;
    }
    const auto suffix = device_id.substr(product_end + 1);
    // descriptor_id_for() uses the serial when one exists and otherwise
    // falls back to the platform HID path.  Prefer exact enumeration (the
    // manager does that before this parser); this fallback still needs to
    // distinguish common path-shaped IDs used by embedded hosts.
    const bool looks_like_path =
        suffix.rfind("/", 0) == 0 || suffix.rfind("\\", 0) == 0 ||
        suffix.rfind("IOService:", 0) == 0 ||
        suffix.rfind("DevSrvsID:", 0) == 0 ||
        suffix.find('/') != std::string::npos ||
        suffix.find('\\') != std::string::npos;
    if (looks_like_path) {
        descriptor.path = suffix;
    } else {
        descriptor.serial_number = suffix;
    }
    return true;
}

std::string trace_event_name(axent::transport::HidReportTraceKind kind)
{
    switch (kind) {
    case axent::transport::HidReportTraceKind::ReadReport:
        return "read-report";
    case axent::transport::HidReportTraceKind::ReadTimeout:
        return "read-timeout";
    case axent::transport::HidReportTraceKind::ReadError:
        return "read-error";
    case axent::transport::HidReportTraceKind::WriteFrame:
        return "write-frame";
    case axent::transport::HidReportTraceKind::WriteReport:
        return "write-report";
    case axent::transport::HidReportTraceKind::WriteError:
        return "write-error";
    case axent::transport::HidReportTraceKind::AcceptedReport:
        return "accepted-report";
    case axent::transport::HidReportTraceKind::DroppedReportId:
        return "dropped-report-id";
    }
    return "unknown";
}

std::string error_name(axtp::ErrorCode code)
{
    switch (code) {
    case axtp::ErrorCode::Success:
        return "success";
    case axtp::ErrorCode::NotSupported:
        return "not-supported";
    case axtp::ErrorCode::Unavailable:
        return "unavailable";
    case axtp::ErrorCode::RpcMethodNotFound:
        return "rpc-method-not-found";
    case axtp::ErrorCode::RpcMethodNotSupported:
        return "rpc-method-not-supported";
    case axtp::ErrorCode::RpcMethodDisabled:
        return "rpc-method-disabled";
    case axtp::ErrorCode::CapabilityMethodUnsupported:
        return "capability-method-unsupported";
    case axtp::ErrorCode::CapabilityStreamUnsupported:
        return "capability-stream-unsupported";
    case axtp::ErrorCode::RpcResponseTimeout:
        return "rpc-response-timeout";
    default:
        return "axtp-error-" + std::to_string(static_cast<std::uint32_t>(code));
    }
}

// Keep runtime error identities inside the Axent adapter.  Product hosts see
// only the stable public contract and can therefore stop retrying a method
// that the peer explicitly does not implement without depending on generated
// cpp-runtime IDs.
ControlStatus control_status_for_runtime_error(axtp::ErrorCode code)
{
    switch (code) {
    case axtp::ErrorCode::NotSupported:
    case axtp::ErrorCode::RpcMethodNotFound:
    case axtp::ErrorCode::RpcMethodNotSupported:
    case axtp::ErrorCode::RpcMethodDisabled:
    case axtp::ErrorCode::CapabilityMethodUnsupported:
    case axtp::ErrorCode::CapabilityStreamUnsupported:
        return ControlStatus::NotSupported;
    case axtp::ErrorCode::InvalidArgument:
        return ControlStatus::InvalidArgument;
    case axtp::ErrorCode::Busy:
        return ControlStatus::Busy;
    default:
        return ControlStatus::Unavailable;
    }
}

bool runtime_error_is_terminal(axtp::ErrorCode code)
{
    switch (code) {
    case axtp::ErrorCode::NotSupported:
    case axtp::ErrorCode::RpcMethodNotFound:
    case axtp::ErrorCode::RpcMethodNotSupported:
    case axtp::ErrorCode::RpcMethodDisabled:
    case axtp::ErrorCode::CapabilityMethodUnsupported:
    case axtp::ErrorCode::CapabilityStreamUnsupported:
        return true;
    default:
        return false;
    }
}

const char* media_kind_name(MediaKind kind)
{
    switch (kind) {
    case MediaKind::Video:
        return "video";
    case MediaKind::Audio:
        return "audio";
    case MediaKind::Unknown:
    default:
        return "unknown";
    }
}

std::string capabilities_method_name(MediaKind kind)
{
    return std::string(media_kind_name(kind)) + ".getStreamCapabilities";
}

std::string open_stream_method_name(MediaKind kind)
{
    return std::string(media_kind_name(kind)) + ".openStream";
}

std::optional<nlohmann::json> parse_json_object(const std::string& text)
{
    if (text.empty()) {
        return std::nullopt;
    }
    try {
        auto parsed = nlohmann::json::parse(text);
        if (parsed.is_object()) {
            return parsed;
        }
    } catch (const std::exception&) {
    }
    return std::nullopt;
}

std::string json_string_or(const nlohmann::json& object, const char* key, std::string fallback = {})
{
    if (object.is_object() && object.contains(key) && object[key].is_string()) {
        return object[key].get<std::string>();
    }
    return fallback;
}

std::uint32_t json_u32_or(const nlohmann::json& object, const char* key, std::uint32_t fallback = 0)
{
    if (!object.is_object() || !object.contains(key)) {
        return fallback;
    }
    const auto& value = object[key];
    if (value.is_number_unsigned()) {
        return value.get<std::uint32_t>();
    }
    if (value.is_number_integer()) {
        const auto signed_value = value.get<std::int64_t>();
        return signed_value > 0 ? static_cast<std::uint32_t>(signed_value) : fallback;
    }
    return fallback;
}

std::string ascii_lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

struct ParsedMediaSourceStateEvent {
    std::uint32_t event_id = 0;
    std::string event_name;
    MediaKind kind = MediaKind::Unknown;
    std::string source;
    std::string state;
    std::string reason;
    std::uint32_t active_stream_id = 0;
    bool has_active_stream_id = false;
    bool valid = false;
};

ParsedMediaSourceStateEvent parse_media_source_state_event(
    const axtp::RpcPayload& payload,
    MediaKind kind,
    std::uint32_t event_id,
    const char* expected_name)
{
    ParsedMediaSourceStateEvent event;
    event.event_id = event_id;
    event.event_name = payload.meta.jsonMethodOrEventName.empty()
        ? expected_name
        : payload.meta.jsonMethodOrEventName;
    event.kind = kind;
    const auto body = parse_json_object(
        std::string(payload.body.begin(), payload.body.end()));
    if (!body.has_value()) {
        return event;
    }
    event.source = json_string_or(*body, "source");
    event.state = json_string_or(*body, "state");
    event.reason = json_string_or(*body, "reason");
    bool active_stream_id_valid = true;
    if (body->contains("activeStreamId")) {
        const auto& active_stream_id = (*body)["activeStreamId"];
        if (active_stream_id.is_number_unsigned()) {
            const auto value = active_stream_id.get<std::uint64_t>();
            if (value <= std::numeric_limits<std::uint32_t>::max()) {
                event.has_active_stream_id = true;
                event.active_stream_id = static_cast<std::uint32_t>(value);
            } else {
                active_stream_id_valid = false;
            }
        } else if (active_stream_id.is_number_integer()) {
            const auto value = active_stream_id.get<std::int64_t>();
            if (value >= 0 &&
                static_cast<std::uint64_t>(value) <=
                    std::numeric_limits<std::uint32_t>::max()) {
                event.has_active_stream_id = true;
                event.active_stream_id = static_cast<std::uint32_t>(value);
            } else {
                active_stream_id_valid = false;
            }
        } else {
            active_stream_id_valid = false;
        }
    }
    event.valid = !event.state.empty() && active_stream_id_valid;
    return event;
}

bool is_terminal_source_state(const std::string& state, const std::string& reason)
{
    const auto normalized_state = ascii_lower(state);
    const auto normalized_reason = ascii_lower(reason);
    return normalized_state == "idle" || normalized_state == "stopped" ||
        normalized_state == "unavailable" || normalized_state == "failed" ||
        normalized_reason == "source_disconnected";
}

bool is_streamable_source_state(const std::string& state)
{
    const auto normalized = ascii_lower(state);
    return normalized == "receiving" || normalized == "available";
}

bool source_state_is_streamable(const nlohmann::json& body)
{
    if (!body.is_object()) {
        return false;
    }
    const auto state = json_string_or(body, "state", json_string_or(body, "currentState"));
    if (state == "available" || state == "receiving") {
        return true;
    }
    if (body.contains("available") && body["available"].is_boolean() && body["available"].get<bool>()) {
        return true;
    }
    return false;
}

bool source_matches(const nlohmann::json& body, const std::string& source)
{
    if (source.empty()) {
        return true;
    }
    const auto source_id = json_string_or(body, "sourceId", json_string_or(body, "source"));
    return source_id.empty() || source_id == source;
}

bool capabilities_are_streamable(const nlohmann::json& capabilities, const std::string& source)
{
    if (!capabilities.is_object()) {
        return false;
    }
    if (capabilities.contains("sourceState") &&
        source_state_is_streamable(capabilities["sourceState"])) {
        return true;
    }
    if (capabilities.contains("sources") && capabilities["sources"].is_array()) {
        for (const auto& entry : capabilities["sources"]) {
            if (source_matches(entry, source) && source_state_is_streamable(entry)) {
                return true;
            }
        }
    }
    if (capabilities.value("supported", false) &&
        capabilities.contains("openModes") &&
        capabilities["openModes"].is_array()) {
        for (const auto& mode : capabilities["openModes"]) {
            if (mode.is_string() && mode.get<std::string>() == "receiver_pull") {
                return true;
            }
        }
    }
    return false;
}

std::uint32_t choose_audio_channels(const nlohmann::json& capabilities,
                                    const std::string& source,
                                    std::uint32_t requested)
{
    if (requested == 0) {
        requested = 2;
    }
    if (!capabilities.is_object() || !capabilities.contains("sources") ||
        !capabilities["sources"].is_array()) {
        return requested;
    }
    for (const auto& entry : capabilities["sources"]) {
        if (!source_matches(entry, source) || !entry.contains("channels") ||
            !entry["channels"].is_array()) {
            continue;
        }
        for (const auto& channel : entry["channels"]) {
            if (channel.is_number_unsigned() && channel.get<std::uint32_t>() == requested) {
                return requested;
            }
            if (channel.is_number_integer() &&
                channel.get<std::int64_t>() == static_cast<std::int64_t>(requested)) {
                return requested;
            }
        }
        for (const auto& channel : entry["channels"]) {
            if (channel.is_number_unsigned() && channel.get<std::uint32_t>() > 0) {
                return channel.get<std::uint32_t>();
            }
            if (channel.is_number_integer() && channel.get<std::int64_t>() > 0) {
                return static_cast<std::uint32_t>(channel.get<std::int64_t>());
            }
        }
    }
    return requested;
}

MediaCodec codec_for_open_result(MediaKind kind, const nlohmann::json& result)
{
    const auto codec = ascii_lower(json_string_or(result, "codec", json_string_or(result, "format")));
    if (codec == "h264") {
        return MediaCodec::H264;
    }
    if (codec == "h265" || codec == "hevc") {
        return MediaCodec::H265;
    }
    if (codec == "aac") {
        return MediaCodec::Aac;
    }
    if (codec == "pcm" || codec == "lpcm") {
        return MediaCodec::Pcm;
    }
    if (!codec.empty()) {
        return MediaCodec::Opaque;
    }
    return kind == MediaKind::Video ? MediaCodec::H264 :
        (kind == MediaKind::Audio ? MediaCodec::Aac : MediaCodec::Unknown);
}

std::unique_ptr<axtp::ITransport> make_default_hid_transport(axent::transport::HidTransportOptions options)
{
#if AXENT_HAS_AXTP_HID_TRANSPORT
    return std::make_unique<axent::transport::HidTransport>(std::move(options));
#else
    (void)options;
    return nullptr;
#endif
}

} // namespace

namespace detail {
namespace {

class DefaultAxtpAdapterRuntimeFactory final : public AxtpAdapterRuntimeFactory {
public:
    std::unique_ptr<axtp::ITransport> create(const axent::transport::HidTransportOptions& options) override
    {
        return make_default_hid_transport(options);
    }
};

} // namespace

std::shared_ptr<AxtpAdapterRuntimeFactory> make_default_axtp_runtime_factory()
{
    return std::make_shared<DefaultAxtpAdapterRuntimeFactory>();
}

} // namespace detail

namespace {

enum class ManagerCallbackKind {
    Frame,
    StreamEvent,
};

struct ActiveManagerCallback {
    const void* state = nullptr;
    ManagerCallbackKind kind = ManagerCallbackKind::Frame;
    std::uint64_t generation = 0;
};

thread_local std::vector<ActiveManagerCallback> active_manager_callbacks;

std::size_t active_manager_callback_count(const void* state,
                                          ManagerCallbackKind kind,
                                          std::uint64_t generation)
{
    return static_cast<std::size_t>(std::count_if(
        active_manager_callbacks.begin(),
        active_manager_callbacks.end(),
        [=](const ActiveManagerCallback& active) {
            return active.state == state && active.kind == kind &&
                active.generation == generation;
        }));
}

bool has_active_manager_callback(const void* state)
{
    return std::any_of(
        active_manager_callbacks.begin(),
        active_manager_callbacks.end(),
        [state](const ActiveManagerCallback& active) {
            return active.state == state;
        });
}

class ActiveManagerCallbackGuard final {
public:
    explicit ActiveManagerCallbackGuard(ActiveManagerCallback active)
    {
        active_manager_callbacks.push_back(active);
    }

    ~ActiveManagerCallbackGuard()
    {
        active_manager_callbacks.pop_back();
    }

    ActiveManagerCallbackGuard(const ActiveManagerCallbackGuard&) = delete;
    ActiveManagerCallbackGuard& operator=(const ActiveManagerCallbackGuard&) = delete;
};

class VideoStreamParamsObserverSlot final {
public:
    explicit VideoStreamParamsObserverSlot(VideoStreamParamsObserver next)
        : observer(std::move(next))
    {
    }

    void publish(const VideoStreamParamsState& state)
    {
        VideoStreamParamsObserver current;
        {
            std::lock_guard<std::mutex> lock(mutex);
            current = observer;
        }
        if (current) {
            current(state);
        }
    }

    void cancel() noexcept
    {
        std::lock_guard<std::mutex> lock(mutex);
        observer = {};
    }

    std::mutex mutex;
    VideoStreamParamsObserver observer;
};

class VideoStreamParamsSubscriptionState final : public VideoStreamParamsSubscription {
public:
    explicit VideoStreamParamsSubscriptionState(
        std::shared_ptr<VideoStreamParamsObserverSlot> next)
        : slot_(std::move(next))
    {
    }

    ~VideoStreamParamsSubscriptionState() override
    {
        cancel();
    }

    void cancel() noexcept override
    {
        if (slot_) {
            slot_->cancel();
            slot_.reset();
        }
    }

private:
    std::shared_ptr<VideoStreamParamsObserverSlot> slot_;
};

struct VideoReconfigureOperation {
    std::optional<std::uint32_t> previous_frame_rate;
    std::optional<std::uint32_t> requested_frame_rate;
    std::optional<MediaStreamDescriptor> previous_video_descriptor;
    std::optional<MediaStreamDescriptor> previous_audio_descriptor;
    nlohmann::json previous_video_open_params = nlohmann::json::object();
    nlohmann::json previous_audio_open_params = nlohmann::json::object();
    bool video_close_sent = false;
    bool audio_close_sent = false;
    bool video_close_terminal = false;
    bool audio_close_terminal = false;
    bool video_opened = false;
    bool audio_opened = false;
    bool rollback = false;
    std::chrono::steady_clock::time_point close_deadline;
};

constexpr std::uint32_t kStatusSuccess = 0x0000;
constexpr std::uint32_t kStatusNotSupported = 0x0003;
constexpr std::uint32_t kStatusInvalidState = 0x0004;
constexpr std::uint32_t kStatusBusy = 0x0005;
constexpr std::uint32_t kStatusInvalidArgument = 0x000A;
constexpr std::uint32_t kStatusMediaSourceUnavailable = 0x0802;
constexpr std::uint32_t kStatusMediaFrameRateUnsupported = 0x0805;
constexpr std::uint32_t kStatusMediaStreamStartFailed = 0x0807;
constexpr std::uint32_t kStatusMediaStreamStopFailed = 0x0808;

} // namespace

struct AxtpAdapter::RuntimeState {
    explicit RuntimeState(std::shared_ptr<detail::AxtpAdapterRuntimeFactory> next_factory)
        : factory(std::move(next_factory))
    {
    }

    std::shared_ptr<detail::AxtpAdapterRuntimeFactory> factory;
    std::unique_ptr<axtp::sdk::AxtpClient> client;
    axtp::ITransport* active_transport = nullptr;
    mutable std::mutex video_params_mutex;
    VideoStreamParamsState video_params_state;
    std::optional<std::uint32_t> session_video_frame_rate;
    std::optional<VideoReconfigureOperation> video_reconfigure;
    nlohmann::json active_video_open_params = nlohmann::json::object();
    nlohmann::json active_audio_open_params = nlohmann::json::object();
    std::vector<std::weak_ptr<VideoStreamParamsObserverSlot>> video_params_observers;
    std::string health_probe_method;
    nlohmann::json health_probe_params = nlohmann::json::object();
    std::uint64_t next_video_reconfigure_id = 1;
    bool suppress_video_auto_open = false;
    bool suppress_paired_audio_auto_open = false;
    std::vector<std::uint32_t> video_frame_rates;
    bool video_supports_active_reconfigure = true;
    // Capabilities are static for a physical session.  Source-state changes
    // only invalidate the open operation, so an idle/waiting source does not
    // cause a large capabilities RPC on every retry tick.
    std::map<MediaKind, nlohmann::json> capabilities_cache;
    std::uint64_t physical_session_generation = 0;
};

struct AxtpAdapter::ManagerCallbackState {
    std::mutex mutex;
    std::condition_variable cv;
    MediaFrameCallback frame_callback;
    MediaStreamEventCallback stream_event_callback;
    std::uint64_t frame_generation = 1;
    std::uint64_t stream_event_generation = 1;
    std::map<std::uint64_t, std::size_t> frame_in_flight;
    std::map<std::uint64_t, std::size_t> stream_event_in_flight;
};

struct AxtpAdapter::DeviceContext {
    TransportDescriptor descriptor;
    std::unique_ptr<AxtpAdapter> adapter;
    // Manager calls may hold a shared_ptr after the context is removed from
    // the map. Retire the context and wait for this counter before stopping
    // its leaf, so an old WS request cannot reopen it while a new context is
    // being created for the same physical device. The mutex is held only for
    // the state transition/counter update, never across an AXTP call.
    std::mutex lifecycle_mutex;
    std::condition_variable lifecycle_cv;
    std::size_t in_flight = 0;
    bool retired = false;
};

class AxtpAdapter::DeviceContextOperation final {
public:
    explicit DeviceContextOperation(const std::shared_ptr<DeviceContext>& context)
        : context_(context)
    {
        if (!context_) {
            return;
        }
        std::lock_guard<std::mutex> lock(context_->lifecycle_mutex);
        if (context_->retired) {
            return;
        }
        ++context_->in_flight;
        acquired_ = true;
    }

    ~DeviceContextOperation()
    {
        if (!acquired_ || !context_) {
            return;
        }
        {
            std::lock_guard<std::mutex> lock(context_->lifecycle_mutex);
            if (context_->in_flight != 0) {
                --context_->in_flight;
            }
        }
        context_->lifecycle_cv.notify_all();
    }

    DeviceContextOperation(const DeviceContextOperation&) = delete;
    DeviceContextOperation& operator=(const DeviceContextOperation&) = delete;

    explicit operator bool() const
    {
        return acquired_;
    }

private:
    std::shared_ptr<DeviceContext> context_;
    bool acquired_ = false;
};

struct AxtpAdapter::PendingControlCall {
    std::string device_id;
    std::string source_endpoint_id;
    std::string destination_endpoint_id;
    EndpointDeliveryMode endpoint_delivery_mode =
        EndpointDeliveryMode::LocalProjection;
    std::string method;
    std::string params;
    std::chrono::steady_clock::time_point deadline;
    std::uint64_t physical_generation = 0;
    std::uint64_t recovery_generation = 0;
    ControlOperationSource source;
    std::atomic<bool> aborted{false};
};

AxtpAdapter::AxtpAdapter()
    : AxtpAdapter(na20_defaults())
{
}

AxtpAdapter::AxtpAdapter(AxtpAdapterConfig config)
    : AxtpAdapter(std::move(config), detail::make_default_axtp_runtime_factory())
{
}

AxtpAdapter::AxtpAdapter(
    AxtpAdapterConfig config,
    std::shared_ptr<detail::AxtpAdapterRuntimeFactory> runtime_factory,
    bool device_context)
    : config_(std::move(config))
    , device_context_(device_context)
    , runtime_(std::make_unique<RuntimeState>(std::move(runtime_factory)))
{
    if (!runtime_->factory) {
        runtime_->factory = detail::make_default_axtp_runtime_factory();
    }
    if (device_context_) {
        media_dispatcher_ = std::thread(&AxtpAdapter::run_media_dispatcher, this);
        recovery_worker_ = std::thread(&AxtpAdapter::run_session_recovery_worker, this);
    } else {
        manager_callback_state_ = std::make_shared<ManagerCallbackState>();
    }
}

std::shared_ptr<AxtpAdapter::DeviceContext> AxtpAdapter::find_device_context(
    const std::string& device_id) const
{
    std::lock_guard<std::mutex> lock(device_context_mutex_);
    const auto found = device_contexts_.find(device_id);
    return found == device_contexts_.end() ? nullptr : found->second;
}

std::vector<std::shared_ptr<AxtpAdapter::DeviceContext>> AxtpAdapter::device_contexts() const
{
    std::vector<std::shared_ptr<DeviceContext>> contexts;
    std::lock_guard<std::mutex> lock(device_context_mutex_);
    contexts.reserve(device_contexts_.size());
    for (const auto& entry : device_contexts_) {
        contexts.push_back(entry.second);
    }
    return contexts;
}

void AxtpAdapter::install_device_context_callbacks(
    const std::shared_ptr<DeviceContext>& context) const
{
    if (!context || !context->adapter) {
        return;
    }
    const auto callback_state = manager_callback_state_;
    if (!callback_state) {
        return;
    }
    // Install stable forwarding callbacks once. Replacing a manager callback
    // then only updates the manager-owned function slot; it never acquires
    // several leaf dispatch locks in opposite orders while device callbacks
    // are running concurrently.
    context->adapter->set_media_frame_callback(
        [callback_state](std::string device_id, MediaFrame frame) {
            MediaFrameCallback callback;
            std::uint64_t generation = 0;
            {
                std::lock_guard<std::mutex> lock(callback_state->mutex);
                callback = callback_state->frame_callback;
                if (!callback) {
                    return;
                }
                generation = callback_state->frame_generation;
                ++callback_state->frame_in_flight[generation];
            }
            struct InFlightGuard {
                std::shared_ptr<ManagerCallbackState> state;
                std::uint64_t generation = 0;
                ~InFlightGuard()
                {
                    {
                        std::lock_guard<std::mutex> lock(state->mutex);
                        const auto found = state->frame_in_flight.find(generation);
                        if (found != state->frame_in_flight.end() &&
                            --found->second == 0) {
                            state->frame_in_flight.erase(found);
                        }
                    }
                    state->cv.notify_all();
                }
            } in_flight{callback_state, generation};
            ActiveManagerCallbackGuard active({
                callback_state.get(), ManagerCallbackKind::Frame, generation});
            callback(std::move(device_id), std::move(frame));
        });
    context->adapter->set_media_stream_event_callback(
        [callback_state](MediaStreamEvent event) {
            MediaStreamEventCallback callback;
            std::uint64_t generation = 0;
            {
                std::lock_guard<std::mutex> lock(callback_state->mutex);
                callback = callback_state->stream_event_callback;
                if (!callback) {
                    return;
                }
                generation = callback_state->stream_event_generation;
                ++callback_state->stream_event_in_flight[generation];
            }
            struct InFlightGuard {
                std::shared_ptr<ManagerCallbackState> state;
                std::uint64_t generation = 0;
                ~InFlightGuard()
                {
                    {
                        std::lock_guard<std::mutex> lock(state->mutex);
                        const auto found =
                            state->stream_event_in_flight.find(generation);
                        if (found != state->stream_event_in_flight.end() &&
                            --found->second == 0) {
                            state->stream_event_in_flight.erase(found);
                        }
                    }
                    state->cv.notify_all();
                }
            } in_flight{callback_state, generation};
            ActiveManagerCallbackGuard active({
                callback_state.get(), ManagerCallbackKind::StreamEvent, generation});
            callback(std::move(event));
        });
}

std::shared_ptr<AxtpAdapter::DeviceContext> AxtpAdapter::device_context_for(
    const std::string& device_id,
    bool create) const
{
    if (device_context_ || device_id.empty()) {
        return nullptr;
    }

    // Only one caller constructs the first leaf for a physical ID.  Without
    // this single-flight fence, concurrent WS requests can start two worker
    // sets and let the losing temporary leaf briefly open the same HID peer.
    std::lock_guard<std::mutex> creation_lock(device_creation_mutex_);

    TransportDescriptor descriptor;
    bool descriptor_known = false;
    bool descriptor_cache_populated = false;
    {
        std::lock_guard<std::mutex> lock(device_context_mutex_);
        if (retiring_device_ids_.find(device_id) != retiring_device_ids_.end()) {
            // A reset owns this physical identity until its old leaf has
            // closed. Do not let a caller create a replacement in the gap.
            return nullptr;
        }
        if (ambiguous_device_ids_.find(device_id) !=
            ambiguous_device_ids_.end()) {
            return nullptr;
        }
        const auto found = transport_descriptors_.find(device_id);
        if (found != transport_descriptors_.end()) {
            descriptor = found->second;
            descriptor_known = true;
        }
        descriptor_cache_populated = !transport_descriptors_.empty();
        const auto existing = device_contexts_.find(device_id);
        if (existing != device_contexts_.end()) {
            if (descriptor_known && existing->second &&
                existing->second->adapter) {
                existing->second->descriptor = descriptor;
                existing->second->adapter->update_selector_from_descriptor(
                    descriptor);
            }
            return existing->second;
        }
    }
    if (!create) {
        return nullptr;
    }

#if AXENT_HAS_AXTP_HID_TRANSPORT
    // A path-only HID ID and a serial-number HID ID share the same public
    // hid:<vid>:<pid>:<suffix> shape. Resolve an exact enumerated descriptor
    // before falling back to parsing the suffix, so paths are never passed to
    // hid_open() as if they were serial numbers.
    if (!descriptor_known && device_id.rfind("hid:", 0) == 0) {
        TransportSelector selector;
        {
            std::lock_guard<std::mutex> selector_lock(selector_mutex_);
            selector = config_.selector;
        }
        if (selector.kind == TransportKind::Hid) {
            const auto injected=runtime_->factory->discovery_override();
            const auto hid_devices = injected?*injected:axent::transport::enumerateHidDevices(
                selector.vendor_id, selector.product_id);
            auto projection = detail::project_hid_devices(
                selector, hid_devices, config_.endpoint_delivery_mode);
            std::lock_guard<std::mutex> lock(device_context_mutex_);
            ambiguous_device_ids_ = projection.ambiguous_device_ids;
            if (ambiguous_device_ids_.find(device_id) !=
                ambiguous_device_ids_.end()) {
                transport_descriptors_.erase(device_id);
                return nullptr;
            }
            const auto candidate = projection.descriptors.find(device_id);
            if (candidate != projection.descriptors.end()) {
                descriptor = candidate->second;
                descriptor_known = true;
                transport_descriptors_[device_id] = descriptor;
            }
        }
    }
#endif

    if (!descriptor_known && descriptor_cache_populated) {
        // Once discovery has produced a concrete set, never reopen an
        // arbitrary first HID handle for an unknown logical device.
        return nullptr;
    }
    if (descriptor_known && !descriptor.online) {
        return nullptr;
    }
    TransportSelector selector;
    {
        std::lock_guard<std::mutex> selector_lock(selector_mutex_);
        selector = config_.selector;
    }
    if (!descriptor_known &&
        (!selector.serial_number.empty() || !selector.path.empty())) {
        std::lock_guard<std::mutex> lock(device_context_mutex_);
        if (!fixed_selector_device_id_.empty() &&
            fixed_selector_device_id_ != device_id) {
            return nullptr;
        }
        fixed_selector_device_id_ = device_id;
    }
    if (!descriptor_known) {
        descriptor.id = device_id;
        descriptor.kind = selector.kind;
        descriptor.online = true;
        descriptor.path = selector.path;
        descriptor.serial_number = selector.serial_number;
        descriptor.vendor_id = selector.vendor_id;
        descriptor.product_id = selector.product_id;
        descriptor.usage_page = selector.usage_page;
        descriptor.usage = selector.usage;
        if (selector.path.empty() && selector.serial_number.empty()) {
            if (!populate_selector_identity_from_device_id(device_id, descriptor)) {
                return nullptr;
            }
            // A canonical ID is still checked against the configured
            // discovery allowlist.  This prevents an arbitrary VID/PID string
            // from bypassing a product's transport policy when discovery is
            // unavailable, while descriptors learned from discovery remain
            // authoritative below.
            if ((selector.vendor_id != 0 &&
                 selector.vendor_id != descriptor.vendor_id) ||
                (selector.product_id != 0 &&
                 selector.product_id != descriptor.product_id)) {
                return nullptr;
            }
        }
        if (descriptor.path.empty() && descriptor.serial_number.empty()) {
            // A broad VID/PID/usage selector is not a physical identity.  An
            // arbitrary logical ID must never cause two contexts to open the
            // first matching HID handle; require discovery or a canonical ID
            // that yields an exact serial/path selector.
            return nullptr;
        }
    }

    auto child_config = config_;
    child_config.selector.kind = descriptor.kind;
    // A serial is the stable exact selector across replug/path churn. Path is
    // used only for HID devices which expose no serial number.
    child_config.selector.path = descriptor.serial_number.empty()
        ? descriptor.path : std::string{};
    child_config.selector.serial_number = descriptor.serial_number;
    if (descriptor.vendor_id != 0) {
        child_config.selector.vendor_id = descriptor.vendor_id;
    }
    if (descriptor.product_id != 0) {
        child_config.selector.product_id = descriptor.product_id;
    }
    if (descriptor.usage_page != 0) {
        child_config.selector.usage_page = descriptor.usage_page;
    }
    if (descriptor.usage != 0) {
        child_config.selector.usage = descriptor.usage;
    }
    auto context = std::make_shared<DeviceContext>();
    context->descriptor = descriptor;
    context->adapter = std::unique_ptr<AxtpAdapter>(new AxtpAdapter(
        std::move(child_config), runtime_->factory, true));
    install_device_context_callbacks(context);

    std::lock_guard<std::mutex> context_lock(device_context_mutex_);
    device_contexts_.emplace(device_id, context);
    return context;
}

void AxtpAdapter::update_selector_from_descriptor(
    const TransportDescriptor& descriptor)
{
    if (!device_context_) {
        return;
    }
    std::lock_guard<std::mutex> lock(selector_mutex_);
    if (descriptor.kind != TransportKind::Unknown) {
        config_.selector.kind = descriptor.kind;
    }
    config_.selector.path = descriptor.serial_number.empty()
        ? descriptor.path : std::string{};
    config_.selector.serial_number = descriptor.serial_number;
    if (descriptor.vendor_id != 0) {
        config_.selector.vendor_id = descriptor.vendor_id;
    }
    if (descriptor.product_id != 0) {
        config_.selector.product_id = descriptor.product_id;
    }
    if (descriptor.usage_page != 0) {
        config_.selector.usage_page = descriptor.usage_page;
    }
    if (descriptor.usage != 0) {
        config_.selector.usage = descriptor.usage;
    }
}

AxtpAdapter::~AxtpAdapter()
{
    if (!device_context_) {
        // Leaf forwarding callbacks read this slot for every invocation. Set
        // it empty before destroying the leaves; their destructors join the
        // dispatchers and therefore fence any callback which already copied
        // the old function before the manager object disappears.
        set_media_frame_callback({});
        set_media_stream_event_callback({});
        // Destroy every leaf while all manager callback state and locks are
        // still alive. A leaf may drain a final lifecycle event while its
        // pump/dispatcher is being joined.
        std::map<std::string, std::shared_ptr<DeviceContext>> contexts;
        {
            std::lock_guard<std::mutex> lock(device_context_mutex_);
            contexts.swap(device_contexts_);
            transport_descriptors_.clear();
            ambiguous_device_ids_.clear();
            retiring_device_ids_.clear();
            fixed_selector_device_id_.clear();
        }
        contexts.clear();
        return;
    }
    {
        std::lock_guard<std::mutex> lock(control_mutex_);
        accepting_control_calls_ = false;
    }
    // Stop recovery before taking the final pump handle.  A recovery already
    // past its wake-up can otherwise replace session_pump_ after the
    // destructor joined the old thread, leaving a joinable thread (and a live
    // client user) behind during member destruction.
    recovery_generation_.fetch_add(1);
    cancel_control_calls(
        std::nullopt, std::nullopt, "AXTP adapter is stopping");
    std::thread pump;
    {
        std::lock_guard<std::mutex> lock(media_delivery_session_mutex_);
        if (!media_delivery_sessions_.empty()) {
            media_binding_epoch_.fetch_add(1, std::memory_order_acq_rel);
        }
        media_delivery_sessions_.clear();
    }
    {
        std::lock_guard<std::mutex> lock(recovery_mutex_);
        stop_recovery_worker_ = true;
        recovery_requested_ = false;
    }
    recovery_cv_.notify_all();
    if (recovery_worker_.joinable()) {
        recovery_worker_.join();
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pump = request_stop_session_pump_locked();
    }
    if (pump.joinable()) {
        pump.join();
    }
    {
        std::lock_guard<std::mutex> lock(media_dispatch_mutex_);
        stop_media_dispatcher_ = true;
        ++media_dispatch_generation_;
    }
    media_dispatch_cv_.notify_all();
    if (media_dispatcher_.joinable()) {
        media_dispatcher_.join();
    }
    std::lock_guard<std::mutex> client_lock(client_mutex_);
    if (runtime_->client) {
        runtime_->client->close();
    }
}

AxtpAdapterConfig AxtpAdapter::na20_defaults()
{
    AxtpAdapterConfig config;
    config.selector.kind = TransportKind::Hid;
    config.selector.vendor_id = 0x0581;
    config.selector.product_id = 0x2582;
    config.selector.usage_page = 0x0081;
    config.selector.usage = 0;
    config.selector.report_id = 0x05;
    config.selector.input_report_size = 0;
    config.selector.output_report_size = 0;
    config.selector.read_buffer_size = 4096;
    config.selector.max_reports_per_poll = 32;
    return config;
}

axent::transport::HidTransportOptions detail::hid_options_from_selector(const TransportSelector& selector)
{
    axent::transport::HidTransportOptions options;
    options.vendorId = selector.vendor_id;
    options.productId = selector.product_id;
    options.usagePage = selector.usage_page;
    options.usage = selector.usage;
    options.devicePath = selector.path;
    options.serialNumber = selector.serial_number;
    options.reportId = selector.report_id;
    options.inputReportSize = selector.input_report_size;
    options.outputReportSize = selector.output_report_size;
    options.readBufferSize = selector.read_buffer_size;
    options.maxReportsPerPoll = selector.max_reports_per_poll;
    options.useReadThread = true;
    options.readThreadTimeoutMs = 1000;
    options.reportTrace = [](const axent::transport::HidReportTrace&) {};
    return options;
}

TransportDescriptor detail::descriptor_from_hid_device(const axent::transport::HidDeviceInfo& device)
{
    TransportDescriptor descriptor;
    descriptor.id = descriptor_id_for(device);
    descriptor.kind = TransportKind::Hid;
    descriptor.online = true;
    descriptor.vendor_id = device.vendorId;
    descriptor.product_id = device.productId;
    descriptor.usage_page = device.usagePage;
    descriptor.usage = device.usage;
    descriptor.interface_number = device.interfaceNumber;
    descriptor.path = device.path;
    descriptor.serial_number = device.serialNumber;
    descriptor.manufacturer = device.manufacturer;
    descriptor.product = device.product;
    descriptor.bus_type = device.busType;
    return descriptor;
}

DeviceSnapshot AxtpAdapter::snapshot_from_descriptor(
    const TransportDescriptor& descriptor,
    EndpointDeliveryMode endpoint_delivery_mode)
{
    DeviceSnapshot snapshot;
    snapshot.id = descriptor.id;
    snapshot.adapter = "axtp";
    snapshot.identity.vendor = descriptor.manufacturer;
    snapshot.identity.model = descriptor.product;
    snapshot.identity.serial_number = descriptor.serial_number;
    snapshot.connection.online = descriptor.online;
    snapshot.connection.transport = "hid";
    snapshot.connection.last_change_reason = "hid-discovered";
    snapshot.status.health = descriptor.online ? "ready" : "offline";
    snapshot.endpoint_delivery_mode = endpoint_delivery_mode;
    if (descriptor.kind == TransportKind::Hid &&
        !descriptor.serial_number.empty()) {
        snapshot.endpoint_id = axtp_endpoint_id_from_key(
            "device:axtp:hid:" + hex4(descriptor.vendor_id) + ":" +
            hex4(descriptor.product_id) + ":" + descriptor.serial_number);
    }
    return snapshot;
}

detail::AxtpDiscoveryProjection detail::project_hid_devices(
    const TransportSelector& selector,
    const std::vector<axent::transport::HidDeviceInfo>& hid_devices,
    EndpointDeliveryMode endpoint_delivery_mode)
{
    AxtpDiscoveryProjection projection;
    for (const auto& device : hid_devices) {
        if (!matches_selector(selector, device)) {
            continue;
        }
        auto descriptor = descriptor_from_hid_device(device);
        if (projection.ambiguous_device_ids.find(descriptor.id) !=
            projection.ambiguous_device_ids.end()) {
            continue;
        }
        const auto existing = projection.descriptors.find(descriptor.id);
        if (existing == projection.descriptors.end()) {
            projection.descriptors.emplace(descriptor.id, std::move(descriptor));
            continue;
        }
        // Some platforms can report the exact same HID interface more than
        // once. That is a harmless duplicate. A different path or interface
        // with the same canonical VID/PID/serial evidence is not: choosing
        // either provider would make the Endpoint nondeterministic.
        if (!descriptor.path.empty() &&
            existing->second.path == descriptor.path &&
            existing->second.interface_number == descriptor.interface_number) {
            continue;
        }
        projection.descriptors.erase(existing);
        projection.ambiguous_device_ids.insert(descriptor.id);
    }
    for (const auto& [device_id, descriptor] : projection.descriptors) {
        (void)device_id;
        projection.devices.push_back(AxtpAdapter::snapshot_from_descriptor(
            descriptor, endpoint_delivery_mode));
    }
    return projection;
}

AdapterMetadata AxtpAdapter::metadata() const
{
    return {"axtp", "AXTP Runtime Adapter", true, ""};
}

std::vector<Capability> AxtpAdapter::capabilities() const
{
    return {
        {"axtp.runtime",
         "axtp",
         true,
         "",
         {{"status.get", RiskLevel::Safe, false, false},
          {"stream.flowControl.get", RiskLevel::Safe, false, false},
          {"firmware.update", RiskLevel::Dangerous, true, true}},
         {"axtp.session.changed"}},
    };
}

std::vector<DeviceSnapshot> AxtpAdapter::discover()
{
    std::vector<DeviceSnapshot> devices;
    TransportSelector selector;
    {
        std::lock_guard<std::mutex> selector_lock(selector_mutex_);
        selector = config_.selector;
    }
    if (selector.kind != TransportKind::Hid) {
        return devices;
    }

#if AXENT_HAS_AXTP_HID_TRANSPORT
    const auto injected=runtime_->factory->discovery_override();
    const auto hid_devices = injected?*injected:axent::transport::enumerateHidDevices(
        selector.vendor_id, selector.product_id);
    auto projection = detail::project_hid_devices(
        selector, hid_devices, config_.endpoint_delivery_mode);
    devices = std::move(projection.devices);
    auto& descriptors = projection.descriptors;
    if (!device_context_) {
        std::vector<std::pair<std::shared_ptr<DeviceContext>, TransportDescriptor>>
            selector_updates;
        {
            std::lock_guard<std::mutex> lock(device_context_mutex_);
            ambiguous_device_ids_ = projection.ambiguous_device_ids;
            if (!descriptors.empty() || transport_descriptors_.empty() ||
                !ambiguous_device_ids_.empty()) {
                transport_descriptors_ = descriptors;
            }
            for (const auto& [device_id, descriptor] : descriptors) {
                const auto context = device_contexts_.find(device_id);
                if (context == device_contexts_.end() ||
                    !context->second || !context->second->adapter) {
                    continue;
                }
                context->second->descriptor = descriptor;
                selector_updates.emplace_back(context->second, descriptor);
            }
        }
        // Keep this outside the manager map lock. A leaf may be opening or
        // recovering and serializes its selector snapshot independently.
        for (const auto& [context, descriptor] : selector_updates) {
            DeviceContextOperation context_operation(context);
            if (context_operation) {
                context->adapter->update_selector_from_descriptor(descriptor);
            }
        }
    }
#endif
    return devices;
}

ControlResult AxtpAdapter::call(const std::string& device_id, const std::string& method, const nlohmann::json& params)
{
    AdapterControlRequest request;
    request.device_id = device_id;
    request.method = method;
    request.params = params;
    return call(request);
}

ControlResult AxtpAdapter::call(const AdapterControlRequest& request)
{
    if (!device_context_) {
        const auto context = device_context_for(request.device_id);
        if (!context || !context->adapter) {
            return {ControlStatus::NotFound,
                    {{"error", "AXTP device is not available"}}};
        }
        DeviceContextOperation context_operation(context);
        if (!context_operation) {
            return {ControlStatus::Unavailable,
                    {{"error", "AXTP device context is being retired"}}};
        }
        return context->adapter->call(request);
    }
    // Keep the legacy synchronous Adapter entry point convenient for direct
    // users: it may establish the first physical session.  call_async must
    // not do that work, because opening a HID/AXTP session can block for a
    // whole RPC timeout before an operation is even observable by its caller.
    const auto accepted_at = std::chrono::steady_clock::now();
    const auto deadline = accepted_at + std::chrono::milliseconds(5000);
    bool session_ready = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        session_ready = diagnostics_.open &&
            active_device_id_ == request.device_id;
    }
    if (!session_ready) {
        std::string error;
        const auto status = open_session_status(request.device_id, error, false);
        if (status != ControlStatus::Ok) {
            return {status, {{"error", error}}};
        }
    }
    if (std::chrono::steady_clock::now() >= deadline) {
        return {ControlStatus::Unavailable,
                {{"error", "control operation timeout during session setup"}}};
    }
    ControlCallOptions options;
    options.deadline = deadline;
    auto operation = call_async(request, options);
    auto result = operation
        ? operation->wait()
        : ControlResult{
              ControlStatus::InternalError,
              {{"error", "AXTP adapter returned no control operation"}}};
    // Preserve the legacy synchronous API's callback fence without making
    // the session pump execute product callbacks.  Async callers observe the
    // operation independently; direct synchronous callers return only after
    // lifecycle/frame work already queued by their response has drained.
    drain_pending_media_callbacks();
    return result;
}

ControlOperationPtr AxtpAdapter::call_async(
    const std::string& device_id,
    const std::string& method,
    const nlohmann::json& params,
    ControlCallOptions options)
{
    AdapterControlRequest request;
    request.device_id = device_id;
    request.method = method;
    request.params = params;
    return call_async(request, std::move(options));
}

ControlOperationPtr AxtpAdapter::call_async(
    const AdapterControlRequest& routed_request,
    ControlCallOptions options)
{
    if (!device_context_) {
        const auto context = device_context_for(routed_request.device_id);
        if (!context || !context->adapter) {
            return make_completed_control_operation(
                {ControlStatus::NotFound,
                 {{"error", "AXTP device is not available"}}});
        }
        DeviceContextOperation context_operation(context);
        if (!context_operation) {
            return make_completed_control_operation(
                {ControlStatus::Unavailable,
                 {{"error", "AXTP device context is being retired"}}});
        }
        return context->adapter->call_async(routed_request, std::move(options));
    }
    const auto accepted_at = std::chrono::steady_clock::now();
    const auto deadline = options.deadline.value_or(
        options.timeout <= std::chrono::milliseconds::zero()
            ? accepted_at
            : accepted_at + options.timeout);
    if (deadline <= accepted_at) {
        return make_completed_control_operation(
            {ControlStatus::Unavailable,
             {{"error", "control operation timeout before submission"}}});
    }

    {
        std::lock_guard<std::mutex> lock(control_mutex_);
        if (!accepting_control_calls_) {
            return make_completed_control_operation(
                {ControlStatus::Unavailable,
                 {{"error", "AXTP adapter is stopping"}}});
        }
        const std::size_t outstanding = pending_control_calls_.size() +
            (in_flight_control_call_ ? 1U : 0U);
        if (outstanding >= 32U) {
            return make_completed_control_operation(
                {ControlStatus::Busy,
                 {{"error", "AXTP control operation queue is full"}}});
        }
    }

    auto request = std::make_shared<PendingControlCall>();
    request->device_id = routed_request.device_id;
    request->source_endpoint_id = routed_request.source_endpoint_id;
    request->destination_endpoint_id = routed_request.destination_endpoint_id;
    request->endpoint_delivery_mode = routed_request.endpoint_delivery_mode;
    request->method = routed_request.method;
    request->params = routed_request.params.is_null()
        ? std::string("{}") : routed_request.params.dump();
    request->deadline = deadline;
    // The submitting thread deliberately does not inspect the active AXTP
    // session. The pump is the sole runtime owner; it validates the device
    // and captures this value immediately before dispatch.
    request->physical_generation = 0;
    request->recovery_generation = recovery_generation_.load();
    const std::weak_ptr<PendingControlCall> weak_request = request;
    request->source.set_cancel_handler([this, weak_request]() {
        if (const auto request = weak_request.lock()) {
            request->aborted.store(true);
            std::lock_guard<std::mutex> lock(control_mutex_);
            const auto pending = std::find(
                pending_control_calls_.begin(),
                pending_control_calls_.end(),
                request);
            if (pending != pending_control_calls_.end()) {
                pending_control_calls_.erase(pending);
                control_queue_depth_.store(
                    pending_control_calls_.size(), std::memory_order_relaxed);
            }
        }
        control_cv_.notify_all();
    });
    {
        std::lock_guard<std::mutex> lock(control_mutex_);
        const std::size_t outstanding =
            pending_control_calls_.size() +
            (in_flight_control_call_ ? 1U : 0U);
        if (!accepting_control_calls_) {
            return make_completed_control_operation(
                {ControlStatus::Unavailable,
                 {{"error", "AXTP adapter is stopping"}}});
        }
        // reset/recovery invalidates a call before it can be observed by the
        // FIFO.  Check after obtaining the queue lock as well: teardown first
        // advances this generation and then drains the queue, so an enqueue
        // racing that drain cannot leave an operation stranded behind a
        // stopped pump.
        if (request->recovery_generation != recovery_generation_.load()) {
            return make_completed_control_operation(
                {ControlStatus::Unavailable,
                 {{"error", "AXTP physical session changed before control submission"}}});
        }
        if (outstanding >= 32U) {
            return make_completed_control_operation(
                {ControlStatus::Busy,
                 {{"error", "AXTP control operation queue is full"}}});
        }
        pending_control_calls_.push_back(request);
        control_queue_depth_.store(
            pending_control_calls_.size(), std::memory_order_relaxed);
        update_high_water(
            control_outstanding_high_water_, outstanding + 1U);
    }
    control_cv_.notify_all();
    return request->source.operation();
}

void AxtpAdapter::process_next_control_call(const std::string& device_id)
{
    std::shared_ptr<PendingControlCall> request;
    {
        std::lock_guard<std::mutex> lock(control_mutex_);
        while (!pending_control_calls_.empty()) {
            request = std::move(pending_control_calls_.front());
            pending_control_calls_.pop_front();
            if (request && !request->source.operation()->ready()) {
                break;
            }
            request.reset();
        }
        if (!request) {
            control_queue_depth_.store(
                pending_control_calls_.size(), std::memory_order_relaxed);
            return;
        }
        in_flight_control_call_ = request;
        control_queue_depth_.store(
            pending_control_calls_.size(), std::memory_order_relaxed);
        control_in_flight_.store(1, std::memory_order_relaxed);
    }

    const auto clear_in_flight = [this, &request]() {
        std::lock_guard<std::mutex> lock(control_mutex_);
        if (in_flight_control_call_ == request) {
            in_flight_control_call_.reset();
        }
        control_in_flight_.store(0, std::memory_order_relaxed);
    };
    const auto now = std::chrono::steady_clock::now();
    std::uint64_t active_generation = 0;
    bool session_matches_request = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        active_generation = runtime_->physical_session_generation;
        session_matches_request = diagnostics_.open &&
            active_device_id_ == request->device_id;
    }
    if (request->recovery_generation != recovery_generation_.load() ||
        request->device_id != device_id || !session_matches_request) {
        request->source.complete(
            {ControlStatus::Unavailable,
             {{"error", "AXTP physical session changed before control dispatch"}}});
        clear_in_flight();
        return;
    }
    if (now >= request->deadline) {
        request->source.complete(
            {ControlStatus::Unavailable,
             {{"error", "AXTP control operation timed out in queue"}}});
        clear_in_flight();
        return;
    }
    request->physical_generation = active_generation;
    if (runtime_->client == nullptr) {
        request->source.complete(
            {ControlStatus::Unavailable,
             {{"error", "AXTP session is unavailable"}}});
        clear_in_flight();
        return;
    }

    axtp::sdk::CallOptions call_options;
    call_options.timeout = std::max(
        std::chrono::milliseconds(1),
        std::chrono::duration_cast<std::chrono::milliseconds>(
            request->deadline - now));
    call_options.cancelled = [request]() {
        return request->aborted.load() ||
            request->source.operation()->ready();
    };
    call_options.progress = [this, &device_id]() {
        publish_runtime_progress(device_id);
    };
    if (request->endpoint_delivery_mode == EndpointDeliveryMode::NativeRelay) {
        if (!request->source_endpoint_id.empty()) {
            call_options.endpoint.src = request->source_endpoint_id;
        }
        if (!request->destination_endpoint_id.empty()) {
            call_options.endpoint.dst = request->destination_endpoint_id;
        }
    }
    const auto body = runtime_->client->callJson(
        request->method, request->params, call_options);
    const auto last_error = runtime_->client->lastError();
    // A callRaw progress callback deliberately performs no recursive client
    // work. Source lifecycle reconciliation runs after the outer call returns.
    process_pending_media_source_state_events(device_id);
    sync_runtime_activity();
    commit_pending_media_batch();

    if (!last_error.ok()) {
        request->source.complete(
            {control_status_for_runtime_error(last_error.code),
             {{"error",
               last_error.message.empty()
                   ? error_name(last_error.code)
                   : last_error.message},
              {"axtp_code", static_cast<std::uint32_t>(last_error.code)}}});
        clear_in_flight();
        return;
    }

    note_inbound_activity();
    if (body.empty()) {
        request->source.complete(
            {ControlStatus::Ok, nlohmann::json::object()});
    } else {
        try {
            request->source.complete(
                {ControlStatus::Ok, nlohmann::json::parse(body)});
        } catch (const std::exception&) {
            request->source.complete(
                {ControlStatus::Ok, {{"body", body}}});
        }
    }
    clear_in_flight();
}

void AxtpAdapter::expire_pending_control_calls(
    std::chrono::steady_clock::time_point now)
{
    std::vector<std::shared_ptr<PendingControlCall>> expired;
    {
        std::lock_guard<std::mutex> lock(control_mutex_);
        for (auto request = pending_control_calls_.begin();
             request != pending_control_calls_.end();) {
            if (!*request || (*request)->source.operation()->ready()) {
                request = pending_control_calls_.erase(request);
                continue;
            }
            if (now < (*request)->deadline) {
                ++request;
                continue;
            }
            (*request)->aborted.store(true);
            expired.push_back(std::move(*request));
            request = pending_control_calls_.erase(request);
        }
        control_queue_depth_.store(
            pending_control_calls_.size(), std::memory_order_relaxed);
    }
    for (auto& request : expired) {
        request->source.complete(
            {ControlStatus::Unavailable,
             {{"error", "AXTP control operation timeout while queued"}}});
    }
    if (!expired.empty()) {
        control_cv_.notify_all();
    }
}

void AxtpAdapter::cancel_control_calls(
    const std::optional<std::string>& device_id,
    const std::optional<std::uint64_t>& physical_generation,
    std::string reason)
{
    std::vector<std::shared_ptr<PendingControlCall>> cancelled;
    {
        std::lock_guard<std::mutex> lock(control_mutex_);
        const auto matches = [&](const std::shared_ptr<PendingControlCall>& request) {
            return request &&
                (!device_id.has_value() || request->device_id == *device_id) &&
                (!physical_generation.has_value() ||
                 request->physical_generation == *physical_generation);
        };
        for (auto request = pending_control_calls_.begin();
             request != pending_control_calls_.end();) {
            if (matches(*request)) {
                cancelled.push_back(std::move(*request));
                request = pending_control_calls_.erase(request);
            } else {
                ++request;
            }
        }
        if (matches(in_flight_control_call_)) {
            cancelled.push_back(in_flight_control_call_);
        }
        control_queue_depth_.store(
            pending_control_calls_.size(), std::memory_order_relaxed);
    }
    for (auto& request : cancelled) {
        request->aborted.store(true);
        request->source.complete(
            {ControlStatus::Unavailable, {{"error", reason}}});
    }
    control_cv_.notify_all();
}

ControlResult AxtpAdapter::start_firmware_update(const std::string& device_id,
                                                 const std::string& file_path)
{
    AdapterControlRequest request;
    request.device_id = device_id;
    request.method = "firmware.update";
    return start_firmware_update(request, file_path);
}

ControlResult AxtpAdapter::start_firmware_update(
    const AdapterControlRequest& request,
    const std::string& file_path)
{
    if (!device_context_) {
        const auto context = device_context_for(request.device_id);
        if (!context || !context->adapter) {
            return {ControlStatus::NotFound,
                    {{"error", "AXTP device is not available"}}};
        }
        DeviceContextOperation context_operation(context);
        if (!context_operation) {
            return {ControlStatus::Unavailable,
                    {{"error", "AXTP device context is being retired"}}};
        }
        return context->adapter->start_firmware_update(request, file_path);
    }
    return {ControlStatus::Unavailable, {{"error", "AXTP firmware update skeleton only"}}};
}

bool detail::matches_selector(const TransportSelector& selector, const axent::transport::HidDeviceInfo& device)
{
    if (selector.kind != TransportKind::Hid) {
        return false;
    }
    if (selector.vendor_id != 0 && device.vendorId != selector.vendor_id) {
        return false;
    }
    if (selector.product_id != 0 && device.productId != selector.product_id) {
        return false;
    }
    if (selector.usage_page != 0 && device.usagePage != selector.usage_page) {
        return false;
    }
    if (selector.usage != 0 && device.usage != selector.usage) {
        return false;
    }
    if (!selector.path.empty() && device.path != selector.path) {
        return false;
    }
    if (!selector.serial_number.empty() && device.serialNumber != selector.serial_number) {
        return false;
    }
    return true;
}

void AxtpAdapter::record_transport_trace(const std::string& event_name,
                                         bool accepted_read,
                                         bool write_report,
                                         bool read_error,
                                         bool write_error,
                                         bool dropped_report,
                                         const std::string& message)
{
    if (!device_context_) {
        const auto contexts = device_contexts();
        if (!contexts.empty()) {
            for (const auto& context : contexts) {
                if (context && context->adapter) {
                    DeviceContextOperation context_operation(context);
                    if (context_operation) {
                        context->adapter->record_transport_trace(
                            event_name,
                            accepted_read,
                            write_report,
                            read_error,
                            write_error,
                            dropped_report,
                            message);
                    }
                }
            }
            return;
        }
    }
    std::lock_guard<std::mutex> lock(mutex_);
    diagnostics_.last_event = event_name;
    // Once a concrete HID transport snapshot is available its atomic
    // counters are the single source of truth.  Mixing trace increments with
    // later snapshot assignment can make a counter move backwards.  The
    // increment path remains for transport-free characterization seams.
    if (!transport_counters_snapshot_owned_) {
        if (accepted_read) {
            ++diagnostics_.read_reports;
        }
        if (write_report) {
            ++diagnostics_.write_reports;
        }
        if (read_error) {
            ++diagnostics_.read_errors;
        }
        if (write_error) {
            ++diagnostics_.write_errors;
        }
        if (dropped_report) {
            ++diagnostics_.dropped_reports;
        }
    }
    if ((read_error || write_error) && !message.empty()) {
        diagnostics_.last_error = message;
    }
}

void AxtpAdapter::note_inbound_activity()
{
    std::lock_guard<std::mutex> lock(mutex_);
    ++inbound_activity_generation_;
    last_transport_activity_ = std::chrono::steady_clock::now();
    diagnostics_.inbound_activity_generation = inbound_activity_generation_;
}

void AxtpAdapter::sync_runtime_activity()
{
    if (runtime_->client == nullptr) {
        return;
    }
    const auto generation = runtime_->client->inboundActivityGeneration();
    bool changed = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (generation != last_runtime_activity_generation_) {
            last_runtime_activity_generation_ = generation;
            changed = true;
        }
    }
    if (changed) {
        note_inbound_activity();
    }
}

void AxtpAdapter::publish_runtime_progress(const std::string& device_id)
{
    // This hook is called only by the session owner after AxtpClient::poll().
    // It must never issue another client operation: doing so would recurse
    // into the currently waiting RPC.  It may, however, expire unrelated
    // queued controls and move already-decoded media to the dispatcher.
    expire_pending_control_calls(std::chrono::steady_clock::now());
    // During the initial app-ready handshake runtime_->client is still null;
    // activity and transport snapshots are conditional, but staged media and
    // lifecycle work can still be committed on every poll.
    if (runtime_->client != nullptr) {
        sync_runtime_activity();
    }
    process_pending_media_source_state_events(device_id, false);
    if (runtime_->client != nullptr) {
        snapshot_transport_diagnostics_from_runtime();
    }
    commit_pending_media_batch();
}

void AxtpAdapter::snapshot_transport_diagnostics_from_runtime(bool force)
{
#if AXENT_HAS_AXTP_HID_TRANSPORT
    // The caller is the physical-session owner and holds client_mutex_.  Copy
    // transport state into Axent-owned diagnostics here so diagnostics() never
    // dereferences runtime_->active_transport while another thread tears down
    // the client and its transport.
    if (runtime_->active_transport == nullptr) {
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    if (!force &&
        next_transport_diagnostics_snapshot_.time_since_epoch().count() != 0 &&
        now < next_transport_diagnostics_snapshot_) {
        return;
    }
    next_transport_diagnostics_snapshot_ = now + std::chrono::milliseconds(100);
    const auto* transport =
        dynamic_cast<const axent::transport::HidTransport*>(runtime_->active_transport);
    if (transport == nullptr) {
        return;
    }
    const auto& transport_options = transport->options();
    const auto profile = transport->profile();
    const auto transport_stats = transport->stats();
    const bool transport_open = transport->isOpen();

    std::lock_guard<std::mutex> lock(mutex_);
    transport_counters_snapshot_owned_ = true;
    diagnostics_.open = transport_open;
    diagnostics_.negotiated_input_report_size = transport_options.inputReportSize;
    diagnostics_.negotiated_output_report_size = transport_options.outputReportSize;
    diagnostics_.read_buffer_size = transport_options.readBufferSize;
    diagnostics_.preferred_frame_size = profile.preferredFrameSize;
    diagnostics_.read_reports = transport_stats.acceptedReports;
    diagnostics_.read_bytes = transport_stats.readBytes;
    diagnostics_.write_reports = transport_stats.writeReports;
    diagnostics_.write_bytes = transport_stats.writeBytes;
    diagnostics_.read_errors = transport_stats.readErrors;
    diagnostics_.write_errors = transport_stats.writeErrors;
    diagnostics_.dropped_reports = transport_stats.droppedReportId;
    diagnostics_.queued_reports = transport_stats.queuedReports;
#endif
}

TransportDiagnostics AxtpAdapter::diagnostics() const
{
    if (!device_context_) {
        const auto contexts = device_contexts();
        if (!contexts.empty()) {
            if (contexts.size() == 1 && contexts.front()->adapter) {
                const auto& context = contexts.front();
                DeviceContextOperation context_operation(context);
                return context_operation ? context->adapter->diagnostics()
                                          : TransportDiagnostics{};
            }
            TransportDiagnostics aggregate;
            bool first = true;
            for (const auto& context : contexts) {
                if (!context || !context->adapter) {
                    continue;
                }
                DeviceContextOperation context_operation(context);
                if (!context_operation) {
                    continue;
                }
                const auto current = context->adapter->diagnostics();
                aggregate.open = aggregate.open || current.open;
                aggregate.negotiated_input_report_size = std::max(
                    aggregate.negotiated_input_report_size,
                    current.negotiated_input_report_size);
                aggregate.negotiated_output_report_size = std::max(
                    aggregate.negotiated_output_report_size,
                    current.negotiated_output_report_size);
                aggregate.read_buffer_size = std::max(
                    aggregate.read_buffer_size, current.read_buffer_size);
                aggregate.preferred_frame_size = std::max(
                    aggregate.preferred_frame_size, current.preferred_frame_size);
                aggregate.read_reports += current.read_reports;
                aggregate.write_reports += current.write_reports;
                aggregate.read_errors += current.read_errors;
                aggregate.write_errors += current.write_errors;
                aggregate.dropped_reports += current.dropped_reports;
                aggregate.queued_reports += current.queued_reports;
                aggregate.read_bytes += current.read_bytes;
                aggregate.write_bytes += current.write_bytes;
                aggregate.control_queue_depth += current.control_queue_depth;
                aggregate.control_in_flight += current.control_in_flight;
                aggregate.control_outstanding_high_water +=
                    current.control_outstanding_high_water;
                aggregate.media_dispatch_queue_depth +=
                    current.media_dispatch_queue_depth;
                aggregate.media_dispatch_queue_high_water +=
                    current.media_dispatch_queue_high_water;
                aggregate.media_frames_dispatched_during_control_call +=
                    current.media_frames_dispatched_during_control_call;
                aggregate.active_media_streams += current.active_media_streams;
                aggregate.health_probe_failures += current.health_probe_failures;
                aggregate.session_recoveries += current.session_recoveries;
                aggregate.inbound_activity_generation +=
                    current.inbound_activity_generation;
                aggregate.heartbeat_attempts += current.heartbeat_attempts;
                aggregate.heartbeat_acks += current.heartbeat_acks;
                aggregate.heartbeat_timeouts += current.heartbeat_timeouts;
                aggregate.legacy_probe_attempts += current.legacy_probe_attempts;
                aggregate.legacy_probe_successes += current.legacy_probe_successes;
                aggregate.legacy_fallbacks += current.legacy_fallbacks;
                aggregate.video_retry.configure_attempts +=
                    current.video_retry.configure_attempts;
                aggregate.audio_retry.configure_attempts +=
                    current.audio_retry.configure_attempts;
                aggregate.video_retry.next_retry_in_ms = std::max(
                    aggregate.video_retry.next_retry_in_ms,
                    current.video_retry.next_retry_in_ms);
                aggregate.audio_retry.next_retry_in_ms = std::max(
                    aggregate.audio_retry.next_retry_in_ms,
                    current.audio_retry.next_retry_in_ms);
                aggregate.video_retry.terminal =
                    aggregate.video_retry.terminal || current.video_retry.terminal;
                aggregate.audio_retry.terminal =
                    aggregate.audio_retry.terminal || current.audio_retry.terminal;
                if (static_cast<int>(current.session_health) >
                    static_cast<int>(aggregate.session_health)) {
                    aggregate.session_health = current.session_health;
                }
                if (first) {
                    aggregate.requested_probe_mode = current.requested_probe_mode;
                    aggregate.effective_probe_mode = current.effective_probe_mode;
                    aggregate.negotiated_heartbeat_interval_ms =
                        current.negotiated_heartbeat_interval_ms;
                    first = false;
                } else {
                    if (aggregate.requested_probe_mode != current.requested_probe_mode) {
                        aggregate.requested_probe_mode = SessionProbeMode::Auto;
                    }
                    if (aggregate.effective_probe_mode != current.effective_probe_mode) {
                        aggregate.effective_probe_mode = SessionProbeMode::Auto;
                    }
                    if (aggregate.negotiated_heartbeat_interval_ms !=
                        current.negotiated_heartbeat_interval_ms) {
                        aggregate.negotiated_heartbeat_interval_ms = 0;
                    }
                }
                if (!current.last_event.empty()) {
                    aggregate.last_event = current.last_event;
                }
                if (!current.last_error.empty()) {
                    aggregate.last_error = current.last_error;
                }
                if (!current.last_session_recovery_reason.empty()) {
                    aggregate.last_session_recovery_reason =
                        current.last_session_recovery_reason;
                }
                if (!current.legacy_fallback_reason.empty()) {
                    aggregate.legacy_fallback_reason = current.legacy_fallback_reason;
                }
                if (!current.video_retry.last_error.empty()) {
                    aggregate.video_retry.last_error = current.video_retry.last_error;
                }
                if (!current.audio_retry.last_error.empty()) {
                    aggregate.audio_retry.last_error = current.audio_retry.last_error;
                }
            }
            // A numeric stream ID is scoped to a physical device. It has no
            // unambiguous meaning in the compatibility aggregate.
            aggregate.active_video_stream_id = 0;
            aggregate.active_audio_stream_id = 0;
            return aggregate;
        }
    }
    std::lock_guard<std::mutex> lock(mutex_);
    const_cast<AxtpAdapter*>(this)->refresh_diagnostics_locked();
    return diagnostics_;
}

TransportDiagnostics AxtpAdapter::diagnostics(const std::string& device_id) const
{
    if (!device_context_) {
        const auto context = find_device_context(device_id);
        if (!context || !context->adapter) {
            return {};
        }
        DeviceContextOperation context_operation(context);
        return context_operation ? context->adapter->diagnostics()
                                  : TransportDiagnostics{};
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!active_device_id_.empty() && active_device_id_ != device_id) {
            return {};
        }
    }
    return diagnostics();
}

void AxtpAdapter::set_media_frame_callback(MediaFrameCallback callback)
{
    if (!device_context_) {
        const auto state = manager_callback_state_;
        if (!state) {
            return;
        }
        std::unique_lock<std::mutex> lock(state->mutex);
        const auto old_generation = state->frame_generation++;
        state->frame_callback = std::move(callback);
        if (has_active_manager_callback(state.get())) {
            // A callback may replace either callback kind. Waiting here can
            // form a cross-kind cycle with another device callback doing the
            // inverse replacement; the generation swap is still immediate.
            return;
        }
        const auto locally_active = active_manager_callback_count(
            state.get(), ManagerCallbackKind::Frame, old_generation);
        state->cv.wait(lock, [&]() {
            const auto found = state->frame_in_flight.find(old_generation);
            return found == state->frame_in_flight.end() ||
                found->second <= locally_active;
        });
        return;
    }
    std::lock_guard<std::recursive_mutex> dispatch_lock(
        media_callback_dispatch_mutex_);
    {
        std::lock_guard<std::mutex> lock(media_callback_mutex_);
        media_frame_callback_ = std::move(callback);
    }
}

void AxtpAdapter::set_media_stream_event_callback(MediaStreamEventCallback callback)
{
    if (!device_context_) {
        const auto state = manager_callback_state_;
        if (!state) {
            return;
        }
        std::unique_lock<std::mutex> lock(state->mutex);
        const auto old_generation = state->stream_event_generation++;
        state->stream_event_callback = std::move(callback);
        if (has_active_manager_callback(state.get())) {
            return;
        }
        const auto locally_active = active_manager_callback_count(
            state.get(), ManagerCallbackKind::StreamEvent, old_generation);
        state->cv.wait(lock, [&]() {
            const auto found =
                state->stream_event_in_flight.find(old_generation);
            return found == state->stream_event_in_flight.end() ||
                found->second <= locally_active;
        });
        return;
    }
    std::lock_guard<std::recursive_mutex> dispatch_lock(
        media_callback_dispatch_mutex_);
    {
        std::lock_guard<std::mutex> lock(media_callback_mutex_);
        media_stream_event_callback_ = std::move(callback);
    }
}

std::vector<MediaStreamDescriptor> AxtpAdapter::active_media_stream_descriptors() const
{
    if (!device_context_) {
        const auto contexts = device_contexts();
        if (!contexts.empty()) {
            std::vector<MediaStreamDescriptor> descriptors;
            for (const auto& context : contexts) {
                if (!context || !context->adapter) {
                    continue;
                }
                DeviceContextOperation context_operation(context);
                if (!context_operation) {
                    continue;
                }
                auto current = context->adapter->active_media_stream_descriptors();
                descriptors.insert(
                    descriptors.end(),
                    std::make_move_iterator(current.begin()),
                    std::make_move_iterator(current.end()));
            }
            return descriptors;
        }
    }
    std::vector<MediaStreamDescriptor> descriptors;
    std::lock_guard<std::mutex> lock(media_stream_mutex_);
    descriptors.reserve(active_media_streams_.size());
    for (const auto& entry : active_media_streams_) {
        descriptors.push_back(entry.second.descriptor);
    }
    return descriptors;
}

std::vector<MediaStreamDescriptor> AxtpAdapter::active_media_stream_descriptors(
    const std::string& device_id) const
{
    if (!device_context_) {
        const auto context = find_device_context(device_id);
        if (!context || !context->adapter) {
            return {};
        }
        DeviceContextOperation context_operation(context);
        return context_operation
            ? context->adapter->active_media_stream_descriptors()
            : std::vector<MediaStreamDescriptor>{};
    }
    auto descriptors = active_media_stream_descriptors();
    descriptors.erase(
        std::remove_if(
            descriptors.begin(), descriptors.end(),
            [&device_id](const MediaStreamDescriptor& descriptor) {
                return !descriptor.device_id.empty() &&
                    descriptor.device_id != device_id;
            }),
        descriptors.end());
    return descriptors;
}

VideoStreamParamsResult AxtpAdapter::set_video_stream_params(
    const std::string& device_id,
    const VideoStreamParamsRequest& request)
{
    if (!device_context_) {
        const auto context = find_device_context(device_id);
        if (!context || !context->adapter) {
            VideoStreamParamsResult result;
            result.status_code = kStatusInvalidState;
            return result;
        }
        DeviceContextOperation context_operation(context);
        if (!context_operation) {
            VideoStreamParamsResult result;
            result.status_code = kStatusInvalidState;
            return result;
        }
        return context->adapter->set_video_stream_params(device_id, request);
    }
    VideoStreamParamsResult result;
    if ((!request.frame_rate.has_value() && !request.reset_frame_rate) ||
        (request.frame_rate.has_value() && request.reset_frame_rate) ||
        (request.frame_rate.has_value() && *request.frame_rate == 0)) {
        result.status_code = kStatusInvalidArgument;
        return result;
    }

    std::optional<MediaStreamDescriptor> active_video;
    std::optional<MediaStreamDescriptor> active_audio;
    {
        std::lock_guard<std::mutex> lock(media_stream_mutex_);
        const auto active = std::find_if(
            active_media_streams_.begin(), active_media_streams_.end(),
            [](const auto& entry) {
                return entry.second.descriptor.kind == MediaKind::Video;
            });
        if (active != active_media_streams_.end()) {
            active_video = active->second.descriptor;
        }
        const auto audio = std::find_if(
            active_media_streams_.begin(), active_media_streams_.end(),
            [](const auto& entry) {
                return entry.second.descriptor.kind == MediaKind::Audio;
            });
        if (audio != active_media_streams_.end()) {
            active_audio = audio->second.descriptor;
        }
    }
    bool session_active = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        session_active = active_device_id_ == device_id && diagnostics_.open;
    }

    VideoStreamParamsState state;
    {
        std::lock_guard<std::mutex> lock(runtime_->video_params_mutex);
        if (!session_active) {
            result.status_code = kStatusInvalidState;
            result.state = runtime_->video_params_state;
            return result;
        }
        if (runtime_->video_reconfigure.has_value()) {
            result.status_code = kStatusBusy;
            result.state = runtime_->video_params_state;
            return result;
        }
        if (active_video.has_value() &&
            !runtime_->video_supports_active_reconfigure) {
            result.status_code = kStatusNotSupported;
            result.state = runtime_->video_params_state;
            return result;
        }
        if (request.frame_rate.has_value() &&
            !runtime_->video_frame_rates.empty() &&
            std::find(runtime_->video_frame_rates.begin(),
                      runtime_->video_frame_rates.end(),
                      *request.frame_rate) == runtime_->video_frame_rates.end()) {
            result.status_code = kStatusMediaFrameRateUnsupported;
            result.state = runtime_->video_params_state;
            return result;
        }

        const auto requested = request.reset_frame_rate
            ? std::optional<std::uint32_t>{}
            : request.frame_rate;
        if (runtime_->session_video_frame_rate == requested) {
            runtime_->video_params_state.state = VideoStreamParamsStateKind::Unchanged;
            runtime_->video_params_state.phase = active_video.has_value()
                ? VideoStreamParamsPhase::Streaming
                : VideoStreamParamsPhase::Idle;
            runtime_->video_params_state.changed_fields.clear();
            state = runtime_->video_params_state;
            result.status_code = kStatusSuccess;
            result.accepted = true;
            result.state = state;
            return result;
        }

        VideoReconfigureOperation operation;
        operation.previous_frame_rate = runtime_->session_video_frame_rate;
        operation.requested_frame_rate = requested;
        operation.previous_video_descriptor = active_video;
        operation.previous_audio_descriptor = active_audio;
        operation.previous_video_open_params = runtime_->active_video_open_params;
        operation.previous_audio_open_params = runtime_->active_audio_open_params;
        runtime_->video_reconfigure = std::move(operation);
        runtime_->session_video_frame_rate = requested;
        runtime_->suppress_video_auto_open = true;

        auto& current = runtime_->video_params_state;
        current.source = config_.video_source;
        current.desired_frame_rate = requested;
        current.effective_frame_rate = active_video.has_value() && active_video->frame_rate != 0
            ? std::optional<std::uint32_t>(active_video->frame_rate)
            : current.effective_frame_rate;
        current.reconfigure_id = "vr-" + std::to_string(runtime_->next_video_reconfigure_id++);
        current.state = VideoStreamParamsStateKind::Pending;
        current.phase = active_video.has_value() || active_audio.has_value()
            ? VideoStreamParamsPhase::Closing
            : VideoStreamParamsPhase::Opening;
        current.previous_stream_id = active_video.has_value()
            ? std::optional<std::uint32_t>(active_video->key.stream_id)
            : std::optional<std::uint32_t>{};
        current.active_stream_id = active_video.has_value()
            ? std::optional<std::uint32_t>(active_video->key.stream_id)
            : std::optional<std::uint32_t>{};
        current.rollback_applied = false;
        current.last_error.reset();
        current.changed_fields = {"frameRate"};
        state = current;
    }

    result.status_code = kStatusSuccess;
    result.accepted = true;
    result.state = state;
    notify_video_stream_params_state(state);
    return result;
}

VideoStreamParamsState AxtpAdapter::video_stream_params_state(
    const std::string& device_id) const
{
    if (!device_context_) {
        const auto context = find_device_context(device_id);
        if (!context || !context->adapter) {
            return {};
        }
        DeviceContextOperation context_operation(context);
        return context_operation ? context->adapter->video_stream_params_state(device_id)
                                  : VideoStreamParamsState{};
    }
    bool session_active = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        session_active = active_device_id_ == device_id && diagnostics_.open;
    }
    std::lock_guard<std::mutex> lock(runtime_->video_params_mutex);
    auto state = runtime_->video_params_state;
    if (!session_active) {
        state.session_id.clear();
        state.active_stream_id.reset();
    }
    return state;
}

VideoStreamParamsSubscriptionPtr AxtpAdapter::subscribe_video_stream_params(
    const std::string& device_id,
    VideoStreamParamsObserver observer)
{
    if (!device_context_) {
        const auto context = find_device_context(device_id);
        if (!context || !context->adapter) {
            return {};
        }
        DeviceContextOperation context_operation(context);
        return context_operation
            ? context->adapter->subscribe_video_stream_params(
                  device_id, std::move(observer))
            : VideoStreamParamsSubscriptionPtr{};
    }
    if (!observer) {
        return {};
    }
    bool session_active = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        session_active = active_device_id_ == device_id && diagnostics_.open;
    }
    if (!session_active) {
        return {};
    }
    auto slot = std::make_shared<VideoStreamParamsObserverSlot>(std::move(observer));
    {
        std::lock_guard<std::mutex> lock(runtime_->video_params_mutex);
        runtime_->video_params_observers.push_back(slot);
    }
    return std::make_unique<VideoStreamParamsSubscriptionState>(std::move(slot));
}

void AxtpAdapter::notify_video_stream_params_state(VideoStreamParamsState state)
{
    std::vector<std::shared_ptr<VideoStreamParamsObserverSlot>> observers;
    {
        std::lock_guard<std::mutex> lock(runtime_->video_params_mutex);
        auto& weak_observers = runtime_->video_params_observers;
        for (auto it = weak_observers.begin(); it != weak_observers.end();) {
            if (auto observer = it->lock()) {
                observers.push_back(std::move(observer));
                ++it;
            } else {
                it = weak_observers.erase(it);
            }
        }
    }
    for (const auto& observer : observers) {
        observer->publish(state);
    }
}

void AxtpAdapter::drop_pending_media_frames_for_device(const std::string& device_id)
{
    {
        std::lock_guard<std::mutex> lock(pending_media_ingress_mutex_);
        std::queue<PendingMediaIngressFrame> retained;
        while (!pending_media_ingress_frames_.empty()) {
            auto entry = std::move(pending_media_ingress_frames_.front());
            pending_media_ingress_frames_.pop();
            if (entry.frame.device_id != device_id) {
                retained.push(std::move(entry));
            }
        }
        pending_media_ingress_frames_.swap(retained);
    }
    {
        std::lock_guard<std::mutex> lock(pending_media_dispatch_mutex_);
        std::queue<PendingMediaDispatchItem> retained;
        while (!pending_media_dispatch_items_.empty()) {
            auto item = std::move(pending_media_dispatch_items_.front());
            pending_media_dispatch_items_.pop();
            if (item.kind != PendingMediaDispatchItem::Kind::Frame ||
                item.frame.device_id != device_id) {
                retained.push(std::move(item));
            }
        }
        pending_media_dispatch_items_.swap(retained);
        media_dispatch_queue_depth_.store(
            pending_media_dispatch_items_.size(), std::memory_order_relaxed);
    }
}

void AxtpAdapter::bind_media_delivery_session(const std::string& device_id,
                                              const std::string& session_id)
{
    if (!device_context_) {
        const auto context = device_context_for(device_id);
        if (context && context->adapter) {
            DeviceContextOperation context_operation(context);
            if (!context_operation) {
                return;
            }
            context->adapter->bind_media_delivery_session(device_id, session_id);
        }
        return;
    }
    // Advance the fence before publishing the replacement logical binding.
    // A Core stream event already decoded under the previous token must not
    // become eligible merely because its broker callback runs after this
    // assignment.
    {
        std::lock_guard<std::mutex> lock(media_delivery_session_mutex_);
        media_binding_epoch_.fetch_add(1, std::memory_order_acq_rel);
        media_delivery_sessions_[device_id] = session_id;
    }
    std::lock_guard<std::mutex> lock(runtime_->video_params_mutex);
    runtime_->video_params_state.session_id = session_id;
}

void AxtpAdapter::unbind_media_delivery_session(const std::string& device_id,
                                                const std::string& session_id)
{
    if (!device_context_) {
        const auto context = find_device_context(device_id);
        if (context && context->adapter) {
            DeviceContextOperation context_operation(context);
            if (!context_operation) {
                return;
            }
            context->adapter->unbind_media_delivery_session(device_id, session_id);
        }
        return;
    }
    // Invalidate staged and deferred frames before removing the logical lease.
    // The token check also covers frames that are still queued inside the
    // runtime Core and therefore cannot be removed by the adapter queue drain.
    bool removed = false;
    {
        std::lock_guard<std::mutex> lock(media_delivery_session_mutex_);
        const auto binding = media_delivery_sessions_.find(device_id);
        if (binding != media_delivery_sessions_.end() && binding->second == session_id) {
            media_binding_epoch_.fetch_add(1, std::memory_order_acq_rel);
            media_delivery_sessions_.erase(binding);
            removed = true;
        }
    }
    if (!removed) {
        return;
    }
    {
        std::lock_guard<std::mutex> video_lock(runtime_->video_params_mutex);
        if (runtime_->video_params_state.session_id == session_id) {
            runtime_->video_params_state.session_id.clear();
        }
    }
    // No media lease for this device remains while the Host performs a
    // logical handoff.  Drop frames already staged in Axent immediately;
    // payloads still queued inside runtime Core carry the incremented epoch
    // and are rejected when their broker callback eventually arrives.
    drop_pending_media_frames_for_device(device_id);
}

void AxtpAdapter::clear_video_stream_params_session(bool preserve_logical_session)
{
    std::lock_guard<std::mutex> lock(runtime_->video_params_mutex);
    const auto logical_session_id = preserve_logical_session
        ? runtime_->video_params_state.session_id
        : std::string{};
    runtime_->session_video_frame_rate.reset();
    runtime_->video_reconfigure.reset();
    runtime_->active_video_open_params = nlohmann::json::object();
    runtime_->active_audio_open_params = nlohmann::json::object();
    runtime_->suppress_video_auto_open = false;
    runtime_->suppress_paired_audio_auto_open = false;
    runtime_->video_frame_rates.clear();
    runtime_->video_supports_active_reconfigure = true;
    runtime_->capabilities_cache.clear();
    runtime_->video_params_state = {};
    runtime_->video_params_state.session_id = logical_session_id;
    runtime_->health_probe_method.clear();
    runtime_->health_probe_params = nlohmann::json::object();
}

void AxtpAdapter::clear_media_streams(MediaStreamEventReason reason)
{
    std::vector<MediaStreamEvent> closed_events;
    {
        std::lock_guard<std::mutex> lock(media_stream_mutex_);
        closed_events.reserve(active_media_streams_.size());
        for (const auto& entry : active_media_streams_) {
            closed_events.push_back(
                {MediaStreamEventKind::Closed, entry.second.descriptor, reason});
        }
        active_media_streams_.clear();
        pending_video_source_recovery_close_.reset();
        pending_audio_source_recovery_close_.reset();
        source_recovery_cycle_active_ = false;
        source_recovery_close_sent_ = false;
        source_recovery_reopen_mask_ = 0;
    }
    enqueue_media_stream_events(std::move(closed_events));
    std::lock_guard<std::mutex> lock(mutex_);
    diagnostics_.active_video_stream_id = 0;
    diagnostics_.active_audio_stream_id = 0;
    diagnostics_.active_media_streams = 0;
    next_media_configure_attempt_ = {};
    media_configure_attempts_ = 0;
    video_source_terminal_ = false;
    audio_source_terminal_ = false;
    video_source_waiting_ = false;
    audio_source_waiting_ = false;
    video_source_recovery_pending_ = false;
    audio_source_recovery_pending_ = false;
    next_video_source_recovery_attempt_ = {};
    next_audio_source_recovery_attempt_ = {};
    std::lock_guard<std::mutex> source_event_lock(pending_media_source_state_mutex_);
    std::queue<MediaSourceStateEvent> empty_source_events;
    pending_media_source_state_events_.swap(empty_source_events);
    std::queue<MediaSourceStateEvent> empty_deferred_source_events;
    deferred_media_source_state_events_.swap(empty_deferred_source_events);
    next_media_source_state_order_ = 0;
    latest_video_source_state_order_ = 0;
    latest_audio_source_state_order_ = 0;
    pending_video_open_terminal_orders_.clear();
    pending_audio_open_terminal_orders_.clear();
    std::queue<std::pair<MediaKind, std::uint32_t>> empty_orphan_closes;
    pending_orphan_stream_closes_.swap(empty_orphan_closes);
}

void AxtpAdapter::enqueue_media_stream_events(std::vector<MediaStreamEvent> events)
{
    if (events.empty()) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(pending_media_dispatch_mutex_);
        for (auto& event : events) {
            PendingMediaDispatchItem item;
            item.kind = PendingMediaDispatchItem::Kind::StreamEvent;
            item.event = std::move(event);
            pending_media_dispatch_items_.push(std::move(item));
        }
        const auto depth = pending_media_dispatch_items_.size();
        media_dispatch_queue_depth_.store(depth, std::memory_order_relaxed);
        update_high_water(media_dispatch_queue_high_water_, depth);
    }
    notify_media_dispatch();
}

void AxtpAdapter::enqueue_media_source_state_event(MediaSourceStateEvent event)
{
    note_inbound_activity();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        diagnostics_.last_media_source_event_id = event.event_id;
        diagnostics_.last_media_source_event_name = event.event_name;
        diagnostics_.last_media_source_event_source = event.source;
        diagnostics_.last_media_source_event_state = event.state;
        diagnostics_.last_media_source_event_reason = event.reason;
        diagnostics_.last_media_source_event_active_stream_id = event.active_stream_id;
        diagnostics_.last_media_source_event_has_active_stream_id = event.has_active_stream_id;
        diagnostics_.last_event = "media-source-event name=" + event.event_name +
            " id=0x" + hex4(static_cast<std::uint16_t>(event.event_id)) +
            " source=" + (event.source.empty() ? "<absent>" : event.source) +
            " state=" + (event.state.empty() ? "<absent>" : event.state) +
            " reason=" + (event.reason.empty() ? "<absent>" : event.reason) +
            " activeStreamId=" + (event.has_active_stream_id
                ? std::to_string(event.active_stream_id)
                : "<absent>");
        if (!event.valid) {
            diagnostics_.last_error = "invalid media source state event payload";
        }
    }
    const auto& configured_source = event.kind == MediaKind::Video
        ? config_.video_source
        : config_.audio_source;
    const auto& event_source = event.source.empty() ? configured_source : event.source;
    bool identity_relevant = true;
    if (event.valid && is_terminal_source_state(event.state, event.reason) &&
        event.has_active_stream_id && event.active_stream_id != 0) {
        std::lock_guard<std::mutex> stream_lock(media_stream_mutex_);
        const auto active = active_media_streams_.find(event.active_stream_id);
        identity_relevant = active != active_media_streams_.end() &&
            active->second.descriptor.kind == event.kind &&
            (event_source.empty() ||
             active->second.descriptor.source == event_source);
    }
    std::lock_guard<std::mutex> lock(pending_media_source_state_mutex_);
    event.order = ++next_media_source_state_order_;
    if (event.order == 0) {
        event.order = ++next_media_source_state_order_;
    }
    const bool relevant = identity_relevant && event.valid &&
        (event.kind == MediaKind::Video || event.kind == MediaKind::Audio) &&
        (configured_source.empty() || event_source == configured_source);
    if (relevant) {
        auto& latest_order = event.kind == MediaKind::Video
            ? latest_video_source_state_order_
            : latest_audio_source_state_order_;
        latest_order = event.order;
    }
    pending_media_source_state_events_.push(std::move(event));
}

bool AxtpAdapter::has_pending_media_source_state_events() const
{
    std::lock_guard<std::mutex> lock(pending_media_source_state_mutex_);
    return !pending_media_source_state_events_.empty();
}

void AxtpAdapter::process_pending_media_source_state_events(
    const std::string& device_id,
    bool allow_client_calls)
{
    for (;;) {
        std::queue<MediaSourceStateEvent> events;
        {
            std::lock_guard<std::mutex> lock(pending_media_source_state_mutex_);
            if (allow_client_calls) {
                events.swap(deferred_media_source_state_events_);
            }
            while (!pending_media_source_state_events_.empty()) {
                events.push(std::move(pending_media_source_state_events_.front()));
                pending_media_source_state_events_.pop();
            }
        }
        if (events.empty()) {
            return;
        }
        while (!events.empty()) {
            auto event = std::move(events.front());
            events.pop();
            bool deferred_action_is_current = true;
            if (event.deferred_client_action) {
                std::lock_guard<std::mutex> lock(
                    pending_media_source_state_mutex_);
                const auto latest_order = event.kind == MediaKind::Video
                    ? latest_video_source_state_order_
                    : latest_audio_source_state_order_;
                deferred_action_is_current = event.order == latest_order;
            }
            if (!deferred_action_is_current) {
                continue;
            }
            if (!allow_client_calls && event.valid &&
                event.kind != MediaKind::Unknown &&
                !is_terminal_source_state(event.state, event.reason) &&
                is_streamable_source_state(event.state)) {
                event.deferred_client_action = true;
                std::lock_guard<std::mutex> lock(
                    pending_media_source_state_mutex_);
                deferred_media_source_state_events_.push(std::move(event));
                continue;
            }
            process_media_source_state_event(device_id, event);
        }
        if (!allow_client_calls) {
            // Pure terminal/diagnostic reconciliation cannot enqueue another
            // runtime event.  Return promptly to callRaw so it can continue
            // polling the outstanding response.
            return;
        }
    }
}

void AxtpAdapter::process_media_source_state_event(
    const std::string& device_id,
    const MediaSourceStateEvent& event)
{
    if (!event.valid || event.kind == MediaKind::Unknown) {
        return;
    }
    if (event.kind == MediaKind::Video || event.kind == MediaKind::Audio) {
        std::lock_guard<std::mutex> lock(runtime_->video_params_mutex);
        if (runtime_->video_reconfigure.has_value()) {
            return;
        }
    }

    const auto configured_source = event.kind == MediaKind::Video
        ? config_.video_source
        : config_.audio_source;
    const auto& event_source = event.source.empty() ? configured_source : event.source;
    if (!configured_source.empty() && event_source != configured_source) {
        return;
    }
    const auto event_is_current = [this, &event]() {
        if (event.order == 0 ||
            (event.kind != MediaKind::Video && event.kind != MediaKind::Audio)) {
            return true;
        }
        std::lock_guard<std::mutex> lock(pending_media_source_state_mutex_);
        const auto latest_order = event.kind == MediaKind::Video
            ? latest_video_source_state_order_
            : latest_audio_source_state_order_;
        return event.order == latest_order;
    };
    const auto source_matches = [&](const MediaStreamDescriptor& descriptor) {
        // A source-state event may omit source. Prefer the configured source
        // in that case; if neither side names one, the event is still a valid
        // kind-scoped lifecycle fact.
        return event_source.empty() || descriptor.source == event_source;
    };

    if (is_terminal_source_state(event.state, event.reason)) {
        std::optional<MediaStreamDescriptor> closed_descriptor;
        std::optional<VideoStreamParamsState> video_params_update;
        std::uint32_t active_stream_count = 0;
        bool active_stream_id_is_foreign = false;
        {
            std::lock_guard<std::mutex> lock(media_stream_mutex_);
            auto match = active_media_streams_.end();
            if (event.has_active_stream_id && event.active_stream_id != 0) {
                const auto active = active_media_streams_.find(event.active_stream_id);
                if (active != active_media_streams_.end() &&
                    active->second.descriptor.kind == event.kind &&
                    source_matches(active->second.descriptor)) {
                    match = active;
                } else {
                    // A non-zero ID is an exact identity hint. Never fall back
                    // to kind/source when it identifies another, stale, or
                    // otherwise unknown stream.
                    active_stream_id_is_foreign = true;
                }
            } else {
                match = std::find_if(
                    active_media_streams_.begin(),
                    active_media_streams_.end(),
                    [&](const auto& entry) {
                        return entry.second.descriptor.kind == event.kind &&
                            source_matches(entry.second.descriptor);
                    });
            }
            if (match != active_media_streams_.end()) {
                closed_descriptor = match->second.descriptor;
                active_media_streams_.erase(match);
            }
            active_stream_count = static_cast<std::uint32_t>(active_media_streams_.size());
        }
        if (active_stream_id_is_foreign) {
            const bool open_transition_active = event.kind == MediaKind::Video
                ? video_lifecycle_transition_depth_.load(std::memory_order_acquire) != 0
                : audio_lifecycle_transition_depth_.load(std::memory_order_acquire) != 0;
            if (open_transition_active) {
                // The device may report the terminal state for the stream ID
                // returned by an openStream response that is still in flight.
                // Keep it as a candidate and resolve it against that response;
                // unrelated stale IDs remain ignored as before.
                std::lock_guard<std::mutex> lock(pending_media_source_state_mutex_);
                auto& candidates = event.kind == MediaKind::Video
                    ? pending_video_open_terminal_orders_
                    : pending_audio_open_terminal_orders_;
                auto& candidate_order = candidates[event.active_stream_id];
                candidate_order = std::max(candidate_order, event.order);
            }
            return;
        }
        // Start one paired recovery cycle for the first terminal source event.
        // Capture the descriptor which just closed as well as the other active
        // leg.  The terminal event is removed from active_media_streams_ above,
        // so taking the snapshot only from the map would lose the video leg.
        if (event.kind == MediaKind::Video) {
            std::lock_guard<std::mutex> lock(media_stream_mutex_);
            if (!source_recovery_cycle_active_ &&
                (closed_descriptor.has_value() || !active_media_streams_.empty())) {
                source_recovery_cycle_active_ = true;
                source_recovery_close_sent_ = false;
                source_recovery_reopen_mask_ = 0;
            }
            if (source_recovery_cycle_active_ && !source_recovery_close_sent_) {
                const auto capture = [&](const MediaStreamDescriptor& descriptor) {
                    if (descriptor.kind == MediaKind::Video &&
                        !pending_video_source_recovery_close_.has_value()) {
                        pending_video_source_recovery_close_ = descriptor;
                        source_recovery_reopen_mask_ |= 0x01;
                    } else if (descriptor.kind == MediaKind::Audio &&
                               !pending_audio_source_recovery_close_.has_value()) {
                        pending_audio_source_recovery_close_ = descriptor;
                        source_recovery_reopen_mask_ |= 0x02;
                    }
                };
                if (closed_descriptor.has_value()) {
                    capture(*closed_descriptor);
                }
                for (const auto& entry : active_media_streams_) {
                    capture(entry.second.descriptor);
                }
            }
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (event.kind == MediaKind::Video) {
                video_source_terminal_ = true;
                video_source_recovery_pending_ = false;
                next_video_source_recovery_attempt_ = {};
                if (closed_descriptor.has_value()) {
                    diagnostics_.active_video_stream_id = 0;
                }
            } else {
                audio_source_terminal_ = true;
                audio_source_recovery_pending_ = false;
                next_audio_source_recovery_attempt_ = {};
                if (closed_descriptor.has_value()) {
                    diagnostics_.active_audio_stream_id = 0;
                }
            }
            diagnostics_.active_media_streams = active_stream_count;
        }
        if (closed_descriptor.has_value()) {
            enqueue_media_stream_events({
                {MediaStreamEventKind::Closed,
                 std::move(*closed_descriptor),
                 MediaStreamEventReason::SourceRecovery},
            });
        }
        if (event.kind == MediaKind::Video) {
            {
                std::lock_guard<std::mutex> lock(runtime_->video_params_mutex);
                auto& current = runtime_->video_params_state;
                current.state = VideoStreamParamsStateKind::Idle;
                current.phase = VideoStreamParamsPhase::Idle;
                current.effective_frame_rate.reset();
                current.active_stream_id.reset();
                current.previous_stream_id.reset();
                current.reconfigure_id.clear();
                current.rollback_applied = false;
                current.last_error.reset();
                current.changed_fields.clear();
                video_params_update = current;
            }
            notify_video_stream_params_state(std::move(*video_params_update));
        }
        return;
    }

    if (!is_streamable_source_state(event.state)) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const bool method_terminal = event.kind == MediaKind::Video
            ? diagnostics_.video_retry.terminal
            : diagnostics_.audio_retry.terminal;
        if (method_terminal) {
            // An event cannot make a method/profile that this physical peer
            // explicitly rejected become supported.  Re-evaluate only after
            // a new physical session resets the terminal latch.
            return;
        }
        auto& terminal = event.kind == MediaKind::Video
            ? video_source_terminal_ : audio_source_terminal_;
        auto& source_waiting = event.kind == MediaKind::Video
            ? video_source_waiting_ : audio_source_waiting_;
        auto& recovery_pending = event.kind == MediaKind::Video
            ? video_source_recovery_pending_ : audio_source_recovery_pending_;
        auto& next_recovery = event.kind == MediaKind::Video
            ? next_video_source_recovery_attempt_ : next_audio_source_recovery_attempt_;
        auto& next_configure = event.kind == MediaKind::Video
            ? next_video_configure_attempt_ : next_audio_configure_attempt_;
        const auto retry_deadline = next_recovery.time_since_epoch().count() != 0
            ? next_recovery : next_configure;
        if (recovery_pending && !source_waiting &&
            retry_deadline.time_since_epoch().count() != 0 &&
            now < retry_deadline) {
            // A duplicate streamable event is useful diagnostics, but it must
            // not bypass a transient open failure's backoff.  A real
            // waiting->receiving transition is different: it wakes the
            // event-driven retry immediately instead of waiting 15 seconds.
            return;
        }
        terminal = false;
        source_waiting = false;
        recovery_pending = false;
        next_recovery = {};
        next_configure = {};
    }

    // A source-level stop is different from a normal per-kind retry: NA20 can
    // report the source as available again while NT10 is still carrying the
    // old encoder state.  Reset both receiver-pull legs before the first
    // replacement open so the encoder emits a fresh IDR.  This is deliberately
    // one-shot per terminal/recovery cycle and does not alter the retry timer.
    std::vector<MediaStreamDescriptor> source_recovery_closes;
    std::vector<MediaStreamEvent> source_recovery_closed_events;
    std::uint32_t source_recovery_active_stream_count = 0;
    bool source_recovery_video_closed = false;
    bool source_recovery_audio_closed = false;
    {
        std::lock_guard<std::mutex> lock(media_stream_mutex_);
        if (event.kind == MediaKind::Video &&
            source_recovery_cycle_active_ && !source_recovery_close_sent_) {
            if (pending_video_source_recovery_close_.has_value()) {
                source_recovery_closes.push_back(
                    std::move(*pending_video_source_recovery_close_));
                pending_video_source_recovery_close_.reset();
            }
            if (pending_audio_source_recovery_close_.has_value()) {
                source_recovery_closes.push_back(
                    std::move(*pending_audio_source_recovery_close_));
                pending_audio_source_recovery_close_.reset();
            }
            source_recovery_close_sent_ = true;

            // The explicit paired close is authoritative even when the device
            // does not emit a terminal event for the second leg.  Remove that
            // leg now so stale payloads cannot reach the renderer while the
            // source is being reopened, and publish its lifecycle Closed before
            // the replacement Opened event.
            for (const auto& descriptor : source_recovery_closes) {
                source_recovery_video_closed |= descriptor.kind == MediaKind::Video;
                source_recovery_audio_closed |= descriptor.kind == MediaKind::Audio;
                const auto active = active_media_streams_.find(descriptor.key.stream_id);
                if (active != active_media_streams_.end() &&
                    active->second.descriptor.key.generation == descriptor.key.generation) {
                    source_recovery_closed_events.push_back(
                        {MediaStreamEventKind::Closed,
                         active->second.descriptor,
                         MediaStreamEventReason::SourceRecovery});
                    active_media_streams_.erase(active);
                }
            }
            source_recovery_active_stream_count =
                static_cast<std::uint32_t>(active_media_streams_.size());
        }
    }
    if (!source_recovery_closes.empty()) {
        std::lock_guard<std::mutex> lock(mutex_);
        diagnostics_.active_media_streams = source_recovery_active_stream_count;
        if (source_recovery_active_stream_count == 0) {
            diagnostics_.active_video_stream_id = 0;
            diagnostics_.active_audio_stream_id = 0;
        }
    }
    enqueue_media_stream_events(std::move(source_recovery_closed_events));
    if (!source_recovery_closes.empty() && runtime_->client != nullptr) {
        axtp::sdk::CallOptions options;
        options.timeout = std::chrono::milliseconds(500);
        options.progress = [this, &device_id]() {
            publish_runtime_progress(device_id);
        };
        for (const auto& descriptor : source_recovery_closes) {
            const std::string method_prefix = std::string(media_kind_name(descriptor.kind));
            const nlohmann::json params{
                {"streamId", descriptor.key.stream_id},
                {"peerRole", "transmitter"},
                {"reason", "sourceRecovery"},
            };
            (void)runtime_->client->callJson(
                method_prefix + ".closeStream", params.dump(), options);
        }
        // A close response may have queued a terminal source event. Drain it
        // before opening the replacement so a late old-generation event cannot
        // close the newly opened same-ID stream.
        process_pending_media_source_state_events(device_id);
        if (!event_is_current()) {
            return;
        }
    }
    bool already_active = false;
    {
        std::lock_guard<std::mutex> lock(media_stream_mutex_);
        const auto active = std::find_if(
            active_media_streams_.begin(),
            active_media_streams_.end(),
            [&](const auto& entry) {
                return entry.second.descriptor.kind == event.kind;
            });
        already_active = active != active_media_streams_.end();
    }
    if (already_active) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (event.kind == MediaKind::Video) {
            video_source_recovery_pending_ = false;
            next_video_source_recovery_attempt_ = {};
        } else {
            audio_source_recovery_pending_ = false;
            next_audio_source_recovery_attempt_ = {};
        }
        return;
    }
    const auto recovery_attempt = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const bool pending = event.kind == MediaKind::Video
            ? video_source_recovery_pending_
            : audio_source_recovery_pending_;
        const auto next_attempt = event.kind == MediaKind::Video
            ? next_video_source_recovery_attempt_
            : next_audio_source_recovery_attempt_;
        if (pending && recovery_attempt < next_attempt) {
            return;
        }
    }
    const auto schedule_recovery_retry = [this](MediaKind kind) {
        const auto fallback = std::chrono::steady_clock::now() +
            kMediaConfigureRetryInterval;
        std::lock_guard<std::mutex> lock(mutex_);
        if (kind == MediaKind::Video) {
            video_source_recovery_pending_ = true;
            if (next_video_source_recovery_attempt_.time_since_epoch().count() == 0) {
                next_video_source_recovery_attempt_ =
                    next_video_configure_attempt_.time_since_epoch().count() == 0
                    ? fallback : next_video_configure_attempt_;
            }
        } else {
            audio_source_recovery_pending_ = true;
            if (next_audio_source_recovery_attempt_.time_since_epoch().count() == 0) {
                next_audio_source_recovery_attempt_ =
                    next_audio_configure_attempt_.time_since_epoch().count() == 0
                    ? fallback : next_audio_configure_attempt_;
            }
        }
    };

    // A paired source-recovery close must be followed by paired opens whenever
    // the other source is still streamable.  This keeps NT10's encoder reset
    // symmetric and avoids leaving an otherwise healthy audio leg pointing at
    // a receiver-pull stream which we explicitly closed.
    if (!source_recovery_closes.empty()) {
        const auto is_active_kind = [this](MediaKind kind) {
            std::lock_guard<std::mutex> lock(media_stream_mutex_);
            return std::find_if(
                active_media_streams_.begin(),
                active_media_streams_.end(),
                [kind](const auto& entry) {
                    return entry.second.descriptor.kind == kind;
                }) != active_media_streams_.end();
        };
        bool current_opened = is_active_kind(event.kind);
        if (!current_opened && event_is_current()) {
            current_opened = configure_media_stream_kind(
                device_id,
                event.kind,
                false,
                false,
                MediaStreamEventReason::SourceRecovery);
        }
        const auto other_kind = event.kind == MediaKind::Video
            ? MediaKind::Audio
            : MediaKind::Video;
        const bool other_was_closed = other_kind == MediaKind::Video
            ? source_recovery_video_closed
            : source_recovery_audio_closed;
        bool other_terminal = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            other_terminal = other_kind == MediaKind::Video
                ? video_source_terminal_
                : audio_source_terminal_;
        }
        bool other_opened = is_active_kind(other_kind);
        if (other_was_closed && !other_terminal) {
            if (!other_opened) {
                other_opened = configure_media_stream_kind(
                    device_id,
                    other_kind,
                    false,
                    false,
                    MediaStreamEventReason::SourceRecovery);
            }
        }
        if (!current_opened) {
            schedule_recovery_retry(event.kind);
        }
        if (other_was_closed && !other_terminal && !other_opened) {
            schedule_recovery_retry(other_kind);
        }
        if (current_opened || other_opened) {
            return;
        }
        return;
    }

    if (!event_is_current()) {
        return;
    }
    if (configure_media_stream_kind(
            device_id,
            event.kind,
            false,
            false,
            MediaStreamEventReason::SourceRecovery)) {
        return;
    }
    schedule_recovery_retry(event.kind);
}

void AxtpAdapter::run_pending_orphan_stream_closes(
    const std::string& device_id)
{
    std::queue<std::pair<MediaKind, std::uint32_t>> closes;
    {
        std::lock_guard<std::mutex> lock(pending_media_source_state_mutex_);
        closes.swap(pending_orphan_stream_closes_);
    }
    if (runtime_->client == nullptr) {
        return;
    }
    while (!closes.empty()) {
        const auto [kind, stream_id] = closes.front();
        closes.pop();
        axtp::sdk::CallOptions options;
        options.timeout = std::chrono::milliseconds(500);
        options.progress = [this, &device_id]() {
            publish_runtime_progress(device_id);
        };
        const nlohmann::json params{
            {"streamId", stream_id},
            {"peerRole", "transmitter"},
            {"reason", "sourceTerminalDuringOpen"},
        };
        (void)runtime_->client->callJson(
            std::string(media_kind_name(kind)) + ".closeStream",
            params.dump(),
            options);
    }
}

void AxtpAdapter::reset_session_for_device(const std::string& device_id)
{
    if (!device_context_) {
        // Host calls this after the final lease for the device is gone. Fence
        // first-use creation so a new leaf cannot appear while the old one is
        // being stopped, then retire the idle worker set from the manager.
        std::shared_ptr<DeviceContext> context;
        bool retirement_owned = false;
        {
            std::lock_guard<std::mutex> creation_lock(device_creation_mutex_);
            std::lock_guard<std::mutex> lock(device_context_mutex_);
            const auto found = device_contexts_.find(device_id);
            if (found != device_contexts_.end()) {
                context = found->second;
            }
            if (context && context->adapter) {
                // Mark the identity as retiring while the creation fence is
                // held, then release that fence before waiting. This prevents
                // a replacement leaf from being created, while allowing an
                // in-flight callback to call back into the manager without a
                // lock inversion.
                if (retiring_device_ids_.find(device_id) !=
                    retiring_device_ids_.end()) {
                    return;
                } else {
                    retiring_device_ids_.insert(device_id);
                    retirement_owned = true;
                    std::lock_guard<std::mutex> lifecycle_lock(
                        context->lifecycle_mutex);
                    context->retired = true;
                }
            }
        }
        if (retirement_owned && context && context->adapter) {
            {
                std::unique_lock<std::mutex> lifecycle_lock(
                    context->lifecycle_mutex);
                context->lifecycle_cv.wait(
                    lifecycle_lock,
                    [&context]() { return context->in_flight == 0; });
            }
            context->adapter->reset_session_for_device(device_id);
        }
        if (retirement_owned) {
            // Re-acquire the creation fence before publishing that a new
            // context may be constructed for this identity.
            std::lock_guard<std::mutex> creation_cleanup_lock(
                device_creation_mutex_);
            std::lock_guard<std::mutex> lock(device_context_mutex_);
            const auto found = device_contexts_.find(device_id);
            if (found != device_contexts_.end() && found->second == context) {
                device_contexts_.erase(found);
            }
            retiring_device_ids_.erase(device_id);
            if (fixed_selector_device_id_ == device_id) {
                fixed_selector_device_id_.clear();
            }
        }
        return;
    }
    recovery_generation_.fetch_add(1);
    cancel_control_calls(
        device_id, std::nullopt, "AXTP session was released");
    {
        std::lock_guard<std::mutex> lock(recovery_mutex_);
        recovery_requested_ = false;
        recovery_device_id_.clear();
        recovery_reason_.clear();
    }
    recovery_cv_.notify_all();
    {
        std::lock_guard<std::mutex> lock(media_delivery_session_mutex_);
        const auto binding = media_delivery_sessions_.find(device_id);
        if (binding != media_delivery_sessions_.end()) {
            media_binding_epoch_.fetch_add(1, std::memory_order_acq_rel);
            media_delivery_sessions_.erase(binding);
        }
    }
    std::thread stopped_pump;
    {
        std::lock_guard<std::mutex> session_lock(session_mutex_);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (active_device_id_ != device_id) {
                drop_pending_media_frames_for_device(device_id);
                return;
            }
            stopped_pump = request_stop_session_pump_locked();
        }
        if (stopped_pump.joinable()) {
            stopped_pump.join();
        }
        {
            std::lock_guard<std::mutex> client_lock(client_mutex_);
            if (runtime_->client != nullptr) {
                snapshot_transport_diagnostics_from_runtime(true);
                runtime_->active_transport = nullptr;
                runtime_->client->close();
                runtime_->client.reset();
            }
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            transport_counters_snapshot_owned_ = false;
            next_transport_diagnostics_snapshot_ = {};
            active_device_id_.clear();
            diagnostics_.open = false;
            next_media_configure_attempt_ = {};
            next_video_configure_attempt_ = {};
            next_audio_configure_attempt_ = {};
            video_configure_retry_attempt_ = 0;
            audio_configure_retry_attempt_ = 0;
            video_source_terminal_ = false;
            audio_source_terminal_ = false;
            video_source_recovery_pending_ = false;
            audio_source_recovery_pending_ = false;
            next_video_source_recovery_attempt_ = {};
            next_audio_source_recovery_attempt_ = {};
            diagnostics_.video_retry.last_error.clear();
            diagnostics_.video_retry.next_retry_in_ms = 0;
            diagnostics_.video_retry.terminal = false;
            diagnostics_.audio_retry.last_error.clear();
            diagnostics_.audio_retry.next_retry_in_ms = 0;
            diagnostics_.audio_retry.terminal = false;
        }
    }
    // A call can snapshot the recovery generation after the first queue drain
    // but before the session is marked closed above.  Drain once more after
    // publication of that closed state so it cannot remain queued behind the
    // now-stopped pump.
    cancel_control_calls(
        device_id, std::nullopt, "AXTP session was released");
    clear_media_streams();
    clear_video_stream_params_session();
    drop_pending_media_frames_for_device(device_id);
}

bool AxtpAdapter::open_session(const std::string& device_id, std::string& error)
{
    return open_session_status(device_id, error) == ControlStatus::Ok;
}

ControlStatus AxtpAdapter::open_session_status(const std::string& device_id,
                                               std::string& error,
                                               bool configure_media)
{
    if (!device_context_) {
        const auto context = device_context_for(device_id);
        if (!context || !context->adapter) {
            error = "AXTP device is not available";
            return ControlStatus::NotFound;
        }
        DeviceContextOperation context_operation(context);
        if (!context_operation) {
            error = "AXTP device context is being retired";
            return ControlStatus::Unavailable;
        }
        return context->adapter->open_session_status(
            device_id, error, configure_media);
    }
    std::lock_guard<std::mutex> session_lock(session_mutex_);
    ControlStatus status = ControlStatus::Unavailable;
    (void)ensure_session_locked(device_id, error, status, configure_media,
                                MediaStreamEventReason::InitialOpen);
    return status;
}

std::thread AxtpAdapter::request_stop_session_pump_locked()
{
    stop_session_pump_.store(true);
    if (!session_pump_.joinable()) {
        return {};
    }
    return std::move(session_pump_);
}

bool AxtpAdapter::has_media_delivery_session(const std::string& device_id) const
{
    std::lock_guard<std::mutex> lock(media_delivery_session_mutex_);
    const auto it = media_delivery_sessions_.find(device_id);
    return it != media_delivery_sessions_.end() && !it->second.empty();
}

void AxtpAdapter::set_session_health(SessionHealthState state,
                                     std::uint32_t probe_failures,
                                     std::string reason)
{
    std::lock_guard<std::mutex> lock(mutex_);
    session_health_ = state;
    health_probe_failures_ = probe_failures;
    if (!reason.empty()) {
        last_session_recovery_reason_ = std::move(reason);
    }
    diagnostics_.session_health = session_health_;
    diagnostics_.health_probe_failures = health_probe_failures_;
    diagnostics_.session_recoveries = session_recoveries_;
    diagnostics_.last_session_recovery_reason = last_session_recovery_reason_;
    diagnostics_.requested_probe_mode = config_.session_probe_mode;
    diagnostics_.effective_probe_mode = effective_probe_mode_;
    diagnostics_.negotiated_heartbeat_interval_ms = negotiated_heartbeat_interval_ms_;
    diagnostics_.inbound_activity_generation = inbound_activity_generation_;
    diagnostics_.heartbeat_attempts = heartbeat_attempts_;
    diagnostics_.heartbeat_acks = heartbeat_acks_;
    diagnostics_.heartbeat_timeouts = heartbeat_timeouts_;
    diagnostics_.legacy_probe_attempts = legacy_probe_attempts_;
    diagnostics_.legacy_probe_successes = legacy_probe_successes_;
    diagnostics_.legacy_fallbacks = legacy_fallbacks_;
    diagnostics_.legacy_fallback_reason = legacy_fallback_reason_;
}

bool AxtpAdapter::session_health_probe_due(std::chrono::steady_clock::time_point now)
{
    if (!config_.enable_session_health_probe) {
        return false;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (active_device_id_.empty() || !diagnostics_.open ||
        session_health_ == SessionHealthState::Recovering) {
        return false;
    }
    const auto configuredInterval = std::max<std::uint32_t>(
        1, config_.session_health_probe_interval_ms);
    const auto intervalMs = effective_probe_mode_ == SessionProbeMode::ControlHeartbeat &&
            negotiated_heartbeat_interval_ms_ >= 500
        ? negotiated_heartbeat_interval_ms_
        : configuredInterval;
    const auto interval = std::chrono::milliseconds(intervalMs);
    if (next_health_probe_.time_since_epoch().count() != 0 &&
        now < next_health_probe_) {
        return false;
    }
    if (last_transport_activity_.time_since_epoch().count() != 0 &&
        now - last_transport_activity_ < interval) {
        next_health_probe_ = last_transport_activity_ +
            (effective_probe_mode_ == SessionProbeMode::ControlHeartbeat
                ? heartbeat_schedule_delay(intervalMs, inbound_activity_generation_ + 1)
                : interval);
        last_probe_activity_generation_ = inbound_activity_generation_;
        return false;
    }
    next_health_probe_ = now +
        (effective_probe_mode_ == SessionProbeMode::ControlHeartbeat
            ? heartbeat_schedule_delay(intervalMs, heartbeat_attempts_ + 1)
            : interval);
    last_probe_activity_generation_ = inbound_activity_generation_;
    return true;
}

bool AxtpAdapter::run_session_health_probe(const std::string& device_id)
{
    if (runtime_->client == nullptr || !has_media_delivery_session(device_id)) {
        return true;
    }

    bool transportError = false;
    bool hadOtherActivity = false;
    SessionProbeMode mode;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        mode = effective_probe_mode_;
    }

    if (mode == SessionProbeMode::ControlHeartbeat) {
        const bool heartbeatAlive = run_control_heartbeat_probe(
            device_id, &transportError, &hadOtherActivity);
        if (heartbeatAlive || hadOtherActivity) {
            return true;
        }
        // Explicit control-heartbeat mode is an opt-in strict mode.  Auto is
        // the only mode allowed to spend one legacy RPC as a compatibility
        // fallback after a heartbeat failure.
        if (config_.session_probe_mode != SessionProbeMode::Auto) {
            return false;
        }
        const bool legacyAlive = run_legacy_capabilities_probe(device_id);
        if (!legacyAlive) {
            return false;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        effective_probe_mode_ = SessionProbeMode::LegacyRpc;
        ++legacy_fallbacks_;
        legacy_fallback_reason_ = transportError
            ? "heartbeat-transport-error"
            : "heartbeat-timeout-or-not-supported";
        return true;
    }

    if (mode == SessionProbeMode::LegacyRpc) {
        return run_legacy_capabilities_probe(device_id);
    }

    // Auto remains observable only before a physical session has completed
    // negotiation (for example in a test seam).  Apply the same preference
    // without changing the public contract.
    if (mode == SessionProbeMode::Auto && negotiated_heartbeat_interval_ms_ != 0) {
        const bool heartbeatAlive = run_control_heartbeat_probe(
            device_id, &transportError, &hadOtherActivity);
        if (heartbeatAlive || hadOtherActivity) {
            std::lock_guard<std::mutex> lock(mutex_);
            effective_probe_mode_ = SessionProbeMode::ControlHeartbeat;
            return true;
        }
        const bool legacyAlive = run_legacy_capabilities_probe(device_id);
        if (legacyAlive) {
            std::lock_guard<std::mutex> lock(mutex_);
            effective_probe_mode_ = SessionProbeMode::LegacyRpc;
            ++legacy_fallbacks_;
            legacy_fallback_reason_ = transportError
                ? "heartbeat-transport-error"
                : "heartbeat-timeout-or-not-supported";
        }
        return legacyAlive;
    }
    return run_legacy_capabilities_probe(device_id);
}

bool AxtpAdapter::run_control_heartbeat_probe(const std::string& device_id,
                                              bool* transportError,
                                              bool* hadOtherActivity)
{
    (void)device_id;
    if (transportError != nullptr) {
        *transportError = false;
    }
    if (hadOtherActivity != nullptr) {
        *hadOtherActivity = false;
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ++heartbeat_attempts_;
        last_probe_activity_generation_ = inbound_activity_generation_;
    }
    // HEARTBEAT owns the session pump while it waits for its ACK.  Keep the
    // same poll/progress boundary as business RPCs so media callbacks are
    // committed during a quiet or non-responsive probe instead of being held
    // behind the full timeout.  The hook never calls the client recursively.
    const auto probeProgress = [this, &device_id]() {
        publish_runtime_progress(device_id);
    };
    const auto result = runtime_->client->heartbeat(
        std::chrono::milliseconds(
            std::max<std::uint32_t>(1, config_.session_health_probe_timeout_ms)),
        probeProgress);
    sync_runtime_activity();
    commit_pending_media_batch();
    std::uint64_t activity = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        activity = inbound_activity_generation_;
        if (activity != last_probe_activity_generation_ && hadOtherActivity != nullptr) {
            *hadOtherActivity = true;
        }
        if (result.ok()) {
            ++heartbeat_acks_;
        } else if (result.code == axtp::ErrorCode::ControlHeartbeatTimeout) {
            ++heartbeat_timeouts_;
        }
    }
    if (result.ok()) {
        return true;
    }
    switch (result.code) {
    case axtp::ErrorCode::Unavailable:
    case axtp::ErrorCode::Timeout:
    case axtp::ErrorCode::RpcResponseTimeout:
    case axtp::ErrorCode::TransportReadFailed:
    case axtp::ErrorCode::TransportWriteFailed:
    case axtp::ErrorCode::TransportDisconnected:
    case axtp::ErrorCode::ControlOpenRequired:
    case axtp::ErrorCode::ControlSessionInvalid:
    case axtp::ErrorCode::ControlSessionExpired:
    case axtp::ErrorCode::ControlHeartbeatTimeout:
        if (transportError != nullptr) {
            *transportError = true;
        }
        return false;
    case axtp::ErrorCode::NotSupported:
    case axtp::ErrorCode::RpcMethodNotFound:
    case axtp::ErrorCode::RpcMethodNotSupported:
    case axtp::ErrorCode::RpcMethodDisabled:
        // The peer parsed the request but does not implement this probe.  It
        // is alive, yet Auto must pin the physical session to LegacyRpc so it
        // does not emit a failing heartbeat on every interval.
        return false;
    default:
        // A typed business error means the peer parsed and answered the
        // request, so it is valid liveness evidence.
        return true;
    }
}

bool AxtpAdapter::run_legacy_capabilities_probe(const std::string& device_id)
{
    (void)device_id;
    std::string method;
    nlohmann::json params = nlohmann::json::object();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ++legacy_probe_attempts_;
    }
    if (!runtime_->health_probe_method.empty()) {
        method = runtime_->health_probe_method;
        params = runtime_->health_probe_params;
    } else if (config_.enable_video) {
        method = capabilities_method_name(MediaKind::Video);
        params = nlohmann::json{{"source", config_.video_source}};
    } else if (config_.enable_audio) {
        method = capabilities_method_name(MediaKind::Audio);
        params = nlohmann::json{{"source", config_.audio_source}};
    } else {
        return true;
    }
    axtp::sdk::CallOptions options;
    options.timeout = std::chrono::milliseconds(
        std::max<std::uint32_t>(1, config_.session_health_probe_timeout_ms));
    // A legacy capabilities probe is still a synchronous RPC.  It must carry
    // the same non-recursive media progress hook as every other control call.
    options.progress = [this, &device_id]() {
        publish_runtime_progress(device_id);
    };
    (void)runtime_->client->callJson(method, params.dump(), options);
    const auto error = runtime_->client->lastError();
    sync_runtime_activity();
    commit_pending_media_batch();
    process_pending_media_source_state_events(device_id);
    if (error.ok()) {
        note_inbound_activity();
        std::lock_guard<std::mutex> lock(mutex_);
        ++legacy_probe_successes_;
        return true;
    }
    switch (error.code) {
    case axtp::ErrorCode::Unavailable:
    case axtp::ErrorCode::Timeout:
    case axtp::ErrorCode::RpcResponseTimeout:
    case axtp::ErrorCode::TransportReadFailed:
    case axtp::ErrorCode::TransportWriteFailed:
    case axtp::ErrorCode::TransportDisconnected:
    case axtp::ErrorCode::ControlOpenRequired:
    case axtp::ErrorCode::ControlSessionInvalid:
    case axtp::ErrorCode::ControlSessionExpired:
    case axtp::ErrorCode::ControlHeartbeatTimeout:
        return false;
    default:
        std::lock_guard<std::mutex> lock(mutex_);
        ++legacy_probe_successes_;
        return true;
    }
}

void AxtpAdapter::request_session_recovery(const std::string& device_id,
                                           std::string reason)
{
    std::uint32_t probe_failures = 0;
    {
        std::lock_guard<std::mutex> lock(recovery_mutex_);
        if (stop_recovery_worker_ || recovery_requested_) {
            return;
        }
        recovery_generation_.fetch_add(1);
        recovery_requested_ = true;
        recovery_device_id_ = device_id;
        recovery_reason_ = std::move(reason);
        recovery_attempt_ = 0;
    }
    // health_probe_failures_ is owned by the adapter state mutex.  Take a
    // snapshot before publishing the recovery state; the pump may update the
    // counter concurrently with an external release/recovery request.
    {
        std::lock_guard<std::mutex> lock(mutex_);
        probe_failures = health_probe_failures_;
    }
    set_session_health(
        SessionHealthState::Recovering,
        probe_failures,
        "health-probe-timeout");
    recovery_cv_.notify_one();
}

bool AxtpAdapter::recover_session_once(const std::string& device_id,
                                       std::string& error,
                                       std::uint64_t recovery_generation)
{
    if (!has_media_delivery_session(device_id) ||
        recovery_generation_.load() != recovery_generation) {
        error = "media session was released during recovery";
        return false;
    }
    cancel_control_calls(
        device_id, std::nullopt, "AXTP physical session is recovering");

    std::thread stopped_pump;
    {
        std::lock_guard<std::mutex> session_lock(session_mutex_);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!active_device_id_.empty() && active_device_id_ != device_id) {
                error = "AXTP session device changed during recovery";
                return false;
            }
            stopped_pump = request_stop_session_pump_locked();
        }
        if (stopped_pump.joinable()) {
            stopped_pump.join();
        }
        {
            std::lock_guard<std::mutex> client_lock(client_mutex_);
            if (runtime_->client != nullptr) {
                snapshot_transport_diagnostics_from_runtime(true);
                runtime_->active_transport = nullptr;
                runtime_->client->close();
                runtime_->client.reset();
            }
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            transport_counters_snapshot_owned_ = false;
            next_transport_diagnostics_snapshot_ = {};
            active_device_id_.clear();
            diagnostics_.open = false;
        }
    }
    // request_session_recovery() invalidates queued operations before this
    // teardown starts. Repeat the drain after making the old session visibly
    // unavailable to cover a caller that raced that first drain.
    cancel_control_calls(
        device_id, std::nullopt, "AXTP physical session is recovering");

    // The old physical session is gone; publish its terminal lifecycle before
    // the replacement open and never send closeStream to the dead device.
    clear_media_streams(MediaStreamEventReason::SessionRecovery);
    // Raw stream callbacks can race the pump shutdown.  They carry payloads
    // from the old physical session and must not be re-bound to the newly
    // created descriptor after recovery.
    drop_pending_media_frames_for_device(device_id);
    notify_media_dispatch();
    clear_video_stream_params_session(true);

    if (!has_media_delivery_session(device_id) ||
        recovery_generation_.load() != recovery_generation) {
        error = "media session was released during recovery";
        return false;
    }

    std::lock_guard<std::mutex> session_lock(session_mutex_);
    ControlStatus status = ControlStatus::Unavailable;
    if (!ensure_session_locked(
            device_id,
            error,
            status,
            true,
            MediaStreamEventReason::SessionRecovery)) {
        if (error.empty()) {
            error = "AXTP session recovery failed: " + error_name(
                runtime_->client != nullptr
                    ? runtime_->client->lastError().code
                    : axtp::ErrorCode::Unavailable);
        }
        return false;
    }
    notify_media_dispatch();
    return true;
}

void AxtpAdapter::run_session_recovery_worker()
{
    for (;;) {
        std::string device_id;
        std::string reason;
        std::uint64_t generation = 0;
        {
            std::unique_lock<std::mutex> lock(recovery_mutex_);
            recovery_cv_.wait(lock, [this]() {
                return stop_recovery_worker_ || recovery_requested_;
            });
            if (stop_recovery_worker_) {
                return;
            }
            recovery_requested_ = false;
            device_id = recovery_device_id_;
            reason = recovery_reason_;
            generation = recovery_generation_.load();
            recovery_attempt_ = 0;
        }

        for (;;) {
            if (!has_media_delivery_session(device_id) ||
                recovery_generation_.load() != generation) {
                break;
            }
            std::string error;
            if (recover_session_once(device_id, error, generation)) {
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    ++session_recoveries_;
                    last_session_recovery_reason_ = reason;
                    session_health_ = SessionHealthState::Healthy;
                    health_probe_failures_ = 0;
                    diagnostics_.session_health = session_health_;
                    diagnostics_.health_probe_failures = 0;
                    diagnostics_.session_recoveries = session_recoveries_;
                    diagnostics_.last_session_recovery_reason =
                        last_session_recovery_reason_;
                    diagnostics_.last_event = "session-recovered";
                }
                break;
            }

            std::uint32_t attempt = 0;
            {
                std::lock_guard<std::mutex> lock(recovery_mutex_);
                // A new request can reset the retry counter while this worker
                // is between attempts.  Keep the increment serialized with
                // that reset so the backoff sequence is deterministic and
                // race-free.
                attempt = ++recovery_attempt_;
            }
            const auto initial = std::max<std::uint32_t>(
                1, config_.session_recovery_backoff_initial_ms);
            const auto maximum = std::max(
                initial,
                config_.session_recovery_backoff_max_ms);
            const auto backoff = std::min<std::uint32_t>(
                maximum,
                initial << std::min<std::uint32_t>(attempt - 1, 8));
            set_session_health(SessionHealthState::Failed, health_probe_failures_, error);
            {
                std::lock_guard<std::mutex> lock(mutex_);
                diagnostics_.last_event = "session-recovery-retry-" +
                    std::to_string(attempt);
            }
            std::unique_lock<std::mutex> lock(recovery_mutex_);
            if (recovery_cv_.wait_for(lock, std::chrono::milliseconds(backoff), [this, generation]() {
                    return stop_recovery_worker_ ||
                        recovery_generation_.load() != generation;
                })) {
                break;
            }
        }
    }
}

bool AxtpAdapter::ensure_session_locked(const std::string& device_id,
                                        std::string& error,
                                        ControlStatus& status,
                                        bool configure_media,
                                        MediaStreamEventReason open_reason)
{
    status = ControlStatus::Unavailable;
    // Capture the physical recovery generation before entering the transport
    // handshake. A concurrent release/recovery increments it and the SDK
    // app-ready cancellation hook below can then retire this wait promptly.
    const auto opening_recovery_generation = recovery_generation_.load();
    std::thread stopped_pump;
    bool session_ready = false;
    bool media_ready = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        session_ready = diagnostics_.open && active_device_id_ == device_id;
        media_ready = diagnostics_.active_media_streams != 0 ||
            !config_.enable_media ||
            (!config_.enable_video && !config_.enable_audio);
        if (!active_device_id_.empty() && active_device_id_ != device_id) {
            error = "AXTP session busy for active device " + active_device_id_;
            status = ControlStatus::Busy;
            return false;
        }
        if (session_ready && configure_media && !media_ready) {
            // Once connected, only the pump may use AxtpClient.  Convert a
            // late media lease into pump work instead of issuing openStream
            // RPCs on the lease-accepting thread.
            next_media_configure_attempt_ = std::chrono::steady_clock::now();
            media_configure_attempts_ = 0;
        }
    }
    if (session_ready) {
        status = ControlStatus::Ok;
        control_cv_.notify_all();
        return true;
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopped_pump = request_stop_session_pump_locked();
    }
    if (stopped_pump.joinable()) {
        stopped_pump.join();
    }
    {
        std::lock_guard<std::mutex> client_lock(client_mutex_);
        if (runtime_->client != nullptr) {
            snapshot_transport_diagnostics_from_runtime(true);
            runtime_->active_transport = nullptr;
            runtime_->client->close();
            runtime_->client.reset();
        }
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        transport_counters_snapshot_owned_ = false;
        next_transport_diagnostics_snapshot_ = {};
        if (!active_device_id_.empty()) {
            active_device_id_.clear();
            diagnostics_.open = false;
        }
        effective_probe_mode_ = config_.session_probe_mode;
        negotiated_heartbeat_interval_ms_ = 0;
        inbound_activity_generation_ = 0;
        last_probe_activity_generation_ = 0;
        last_runtime_activity_generation_ = 0;
        heartbeat_attempts_ = 0;
        heartbeat_acks_ = 0;
        heartbeat_timeouts_ = 0;
        legacy_probe_attempts_ = 0;
        legacy_probe_successes_ = 0;
        legacy_fallbacks_ = 0;
        legacy_fallback_reason_.clear();
        next_media_configure_attempt_ = {};
        next_video_configure_attempt_ = {};
        next_audio_configure_attempt_ = {};
        video_configure_retry_attempt_ = 0;
        audio_configure_retry_attempt_ = 0;
        video_source_terminal_ = false;
        audio_source_terminal_ = false;
        video_source_recovery_pending_ = false;
        audio_source_recovery_pending_ = false;
        next_video_source_recovery_attempt_ = {};
        next_audio_source_recovery_attempt_ = {};
        diagnostics_.video_retry.last_error.clear();
        diagnostics_.video_retry.next_retry_in_ms = 0;
        diagnostics_.video_retry.terminal = false;
        diagnostics_.audio_retry.last_error.clear();
        diagnostics_.audio_retry.next_retry_in_ms = 0;
        diagnostics_.audio_retry.terminal = false;
    }
    clear_media_streams();
    clear_video_stream_params_session();
    TransportSelector selector;
    {
        std::lock_guard<std::mutex> selector_lock(selector_mutex_);
        selector = config_.selector;
    }
    if (selector.kind != TransportKind::Hid) {
        error = "AXTP adapter is configured for a non-HID selector";
        return false;
    }

    auto hid_options = detail::hid_options_from_selector(selector);
    hid_options.reportTrace = [this](const axent::transport::HidReportTrace& trace) {
        record_transport_trace(
            trace_event_name(trace.kind),
            trace.kind == axent::transport::HidReportTraceKind::AcceptedReport,
            trace.kind == axent::transport::HidReportTraceKind::WriteReport,
            trace.kind == axent::transport::HidReportTraceKind::ReadError,
            trace.kind == axent::transport::HidReportTraceKind::WriteError,
            trace.kind == axent::transport::HidReportTraceKind::DroppedReportId,
            trace.message);
    };
    auto transport = runtime_->factory ? runtime_->factory->create(hid_options) : nullptr;
    if (transport == nullptr) {
        error = "AXTP HID transport target is unavailable";
        return false;
    }

    axtp::sdk::ClientOptions client_options;
    client_options.autoOpen = true;
    client_options.autoIdentify = false;
    client_options.requestedHeartbeatInterval = std::chrono::milliseconds(
        std::clamp<std::uint32_t>(config_.requested_heartbeat_interval_ms, 1U, 60000U));
    auto client = std::make_unique<axtp::sdk::AxtpClient>(client_options);
    auto register_source_state_event = [this, &client](
                                           axtp::EventId event_id,
                                           MediaKind kind,
                                           const char* event_name) {
        const auto wire_id = static_cast<std::uint32_t>(event_id);
        client->registerEventHandler(
            wire_id,
            [this, wire_id, kind, event_name](const axtp::RpcPayload& payload) {
                const auto parsed = parse_media_source_state_event(
                    payload, kind, wire_id, event_name);
                MediaSourceStateEvent event;
                event.event_id = parsed.event_id;
                event.event_name = parsed.event_name;
                event.kind = parsed.kind;
                event.source = parsed.source;
                event.state = parsed.state;
                event.reason = parsed.reason;
                event.active_stream_id = parsed.active_stream_id;
                event.has_active_stream_id = parsed.has_active_stream_id;
                event.valid = parsed.valid;
                enqueue_media_source_state_event(std::move(event));
            });
    };
    register_source_state_event(
        axtp::EventId::VideoStreamSourceStateChanged,
        MediaKind::Video,
        "video.streamSourceStateChanged");
    register_source_state_event(
        axtp::EventId::AudioStreamSourceStateChanged,
        MediaKind::Audio,
        "audio.streamSourceStateChanged");
    client->setStreamHandler(
        [this, device_id](const axtp::BrokerContext&, const axtp::StreamPayload& stream) {
            handle_stream_payload(
                device_id,
                stream.streamId,
                stream.seqId,
                stream.cursor,
                stream.data,
                stream.meta.ingressToken);
        });
    client->setIngressTokenProvider([this, device_id]() {
        // A standalone adapter has no Host lease and must retain the legacy
        // unbound-frame behavior.  Only a currently bound logical device gets
        // a non-zero provenance token.
        std::lock_guard<std::mutex> lock(media_delivery_session_mutex_);
        if (media_delivery_sessions_.find(device_id) == media_delivery_sessions_.end()) {
            return std::uint64_t{0};
        }
        return media_binding_epoch_.load(std::memory_order_acquire);
    });
    auto* active_transport = transport.get();
    client->attachTransport(std::move(transport));

    axtp::sdk::AppReadyOptions ready_options;
    ready_options.timeout = std::chrono::milliseconds(5000);
    std::string last_ready_event;
    ready_options.trace = [&last_ready_event](const axtp::sdk::AppReadyTraceEvent& event) {
        last_ready_event = "app-ready:" + event.stage + ":" + event.action;
    };
    ready_options.progress = [this, device_id]() {
        // The handshake owns a local AxtpClient until it succeeds.  Drain
        // staged media/lifecycle work without recursively touching that
        // client; the regular session pump takes over after publication.
        publish_runtime_progress(device_id);
    };
    ready_options.cancelled = [this, opening_recovery_generation]() {
        return recovery_generation_.load() != opening_recovery_generation;
    };
    const auto ready = client->ensureAppReady(ready_options);
    const auto negotiatedHeartbeat = client->negotiatedHeartbeatIntervalMs();
    std::unique_lock<std::mutex> client_lock(client_mutex_);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        runtime_->client = std::move(client);
        runtime_->active_transport = active_transport;
        transport_counters_snapshot_owned_ = false;
        next_transport_diagnostics_snapshot_ = {};
        if (!last_ready_event.empty()) {
            diagnostics_.last_event = last_ready_event;
        }
        refresh_diagnostics_locked();
    }
    if (!ready.ok) {
        error = "AXTP app-ready failed at " + ready.stage + ": " + error_name(ready.statusCode);
        {
            runtime_->active_transport = nullptr;
            runtime_->client->close();
            runtime_->client.reset();
        }
        client_lock.unlock();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            transport_counters_snapshot_owned_ = false;
            next_transport_diagnostics_snapshot_ = {};
            active_device_id_.clear();
            diagnostics_.open = false;
        }
        return false;
    }

    snapshot_transport_diagnostics_from_runtime(true);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        negotiated_heartbeat_interval_ms_ = negotiatedHeartbeat.value_or(0);
        effective_probe_mode_ = config_.session_probe_mode;
        legacy_fallback_reason_.clear();
        if (effective_probe_mode_ == SessionProbeMode::Auto) {
            effective_probe_mode_ = negotiatedHeartbeat.has_value()
                ? SessionProbeMode::ControlHeartbeat
                : SessionProbeMode::LegacyRpc;
        } else if (effective_probe_mode_ == SessionProbeMode::ControlHeartbeat &&
                   !negotiatedHeartbeat.has_value()) {
            legacy_fallback_reason_ = "peer-did-not-advertise-valid-interval";
        }
        inbound_activity_generation_ = 0;
        last_probe_activity_generation_ = 0;
        heartbeat_attempts_ = 0;
        heartbeat_acks_ = 0;
        heartbeat_timeouts_ = 0;
        legacy_probe_attempts_ = 0;
        legacy_probe_successes_ = 0;
        legacy_fallbacks_ = 0;
    }

    std::string logical_session_id;
    {
        std::lock_guard<std::mutex> lock(media_delivery_session_mutex_);
        const auto binding = media_delivery_sessions_.find(device_id);
        if (binding != media_delivery_sessions_.end()) {
            logical_session_id = binding->second;
        }
    }
    {
        std::lock_guard<std::mutex> lock(runtime_->video_params_mutex);
        runtime_->session_video_frame_rate = config_.video_frame_rate;
        runtime_->video_params_state = {};
        runtime_->video_params_state.session_id = std::move(logical_session_id);
        runtime_->video_params_state.source = config_.video_source;
        runtime_->video_params_state.desired_frame_rate = config_.video_frame_rate;
        runtime_->video_params_state.state = VideoStreamParamsStateKind::Idle;
        runtime_->video_params_state.phase = VideoStreamParamsPhase::Idle;
        runtime_->suppress_video_auto_open = false;
        runtime_->suppress_paired_audio_auto_open = false;
    }

    if (configure_media) {
        std::lock_guard<std::mutex> lock(mutex_);
        next_media_configure_attempt_ = std::chrono::steady_clock::now() + kMediaConfigureRetryInterval;
        media_configure_attempts_ = 0;
    }
    if (configure_media) {
        configure_media_streams(device_id, open_reason);
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        ++runtime_->physical_session_generation;
        active_device_id_ = device_id;
        diagnostics_.open = true;
        if (diagnostics_.active_media_streams == 0 &&
            (diagnostics_.last_event.empty() || diagnostics_.last_event.rfind("app-ready", 0) == 0)) {
            diagnostics_.last_event = "app-ready";
        }
        session_health_ = SessionHealthState::Healthy;
        health_probe_failures_ = 0;
        last_transport_activity_ = std::chrono::steady_clock::now();
        const auto initialProbeInterval = effective_probe_mode_ == SessionProbeMode::ControlHeartbeat &&
                negotiated_heartbeat_interval_ms_ >= 500
            ? heartbeat_schedule_delay(
                negotiated_heartbeat_interval_ms_, inbound_activity_generation_ + 1)
            : std::chrono::milliseconds(config_.session_health_probe_interval_ms);
        next_health_probe_ = last_transport_activity_ + initialProbeInterval;
        refresh_diagnostics_locked();
        stop_session_pump_.store(false);
        session_pump_ = std::thread([this, device_id]() {
            while (!stop_session_pump_.load()) {
                bool request_recovery = false;
                std::string recovery_reason;
                {
                    std::lock_guard<std::mutex> client_lock(client_mutex_);
                    if (runtime_->client == nullptr) {
                        break;
                    }
                    bool retry_media_configure = false;
                    std::uint32_t retry_attempt = 0;
                    std::string retry_device_id;
                    const auto now = std::chrono::steady_clock::now();
                    {
                        std::lock_guard<std::mutex> lock(mutex_);
                        if (media_configure_retry_due_locked(now)) {
                            retry_media_configure = true;
                            retry_attempt = ++media_configure_attempts_;
                            next_media_configure_attempt_ = now + kMediaConfigureRetryInterval;
                            diagnostics_.last_event =
                                "media-open-retry-" + std::to_string(retry_attempt);
                            retry_device_id = active_device_id_;
                        }
                    }
                    if (retry_media_configure) {
                        configure_media_streams(retry_device_id);
                    }
                    runtime_->client->poll();
                    sync_runtime_activity();
                    // Source transitions are reconciled after the complete
                    // runtime poll. AXTP receiver-pull requires a successful
                    // replacement openStream response before the device emits
                    // frames for the new generation; any frame already in the
                    // same pre-open poll therefore belongs to the old one.
                    process_pending_media_source_state_events(device_id);
                    run_pending_orphan_stream_closes(device_id);
                    advance_video_reconfigure(device_id);
                    retry_pending_media_source_recoveries(device_id, now);
                    expire_pending_control_calls(
                        std::chrono::steady_clock::now());
                    process_next_control_call(device_id);
                    if (session_health_probe_due(now)) {
                        if (run_session_health_probe(device_id)) {
                            set_session_health(SessionHealthState::Healthy, 0);
                        } else {
                            std::uint32_t failures = 0;
                            {
                                std::lock_guard<std::mutex> lock(mutex_);
                                failures = ++health_probe_failures_;
                                session_health_ = failures >=
                                        std::max<std::uint32_t>(
                                            1, config_.session_health_failure_threshold)
                                    ? SessionHealthState::Recovering
                                    : SessionHealthState::Suspect;
                                diagnostics_.session_health = session_health_;
                                diagnostics_.health_probe_failures = failures;
                                diagnostics_.last_event =
                                    "session-health-probe-failed-" + std::to_string(failures);
                            }
                            if (failures >= std::max<std::uint32_t>(
                                    1, config_.session_health_failure_threshold)) {
                                request_recovery = true;
                                recovery_reason = "health-probe-timeout";
                            }
                        }
                    }
                    snapshot_transport_diagnostics_from_runtime();
                }
                if (request_recovery) {
                    request_session_recovery(device_id, std::move(recovery_reason));
                }
                commit_pending_media_batch();
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        });
    }
    status = ControlStatus::Ok;
    return true;
}

bool AxtpAdapter::media_configure_retry_due_locked(std::chrono::steady_clock::time_point now) const
{
    if (!config_.enable_media ||
        (!config_.enable_video && !config_.enable_audio)) {
        return false;
    }
    const bool video_due = config_.enable_video && diagnostics_.active_video_stream_id == 0 &&
        !video_source_terminal_ && !video_source_recovery_pending_ &&
        (next_video_configure_attempt_.time_since_epoch().count() == 0 ||
         now >= next_video_configure_attempt_);
    const bool audio_due = config_.enable_audio && diagnostics_.active_audio_stream_id == 0 &&
        !audio_source_terminal_ && !audio_source_recovery_pending_ &&
        (next_audio_configure_attempt_.time_since_epoch().count() == 0 ||
         now >= next_audio_configure_attempt_);
    return video_due || audio_due;
}

void AxtpAdapter::configure_media_streams(
    const std::string& device_id,
    MediaStreamEventReason open_reason)
{
    bool configure_video = false;
    bool configure_audio = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto now = std::chrono::steady_clock::now();
        configure_video = config_.enable_video && diagnostics_.active_video_stream_id == 0 &&
            !video_source_terminal_ && !video_source_recovery_pending_ &&
            (open_reason != MediaStreamEventReason::InitialOpen ||
             next_video_configure_attempt_.time_since_epoch().count() == 0 ||
             now >= next_video_configure_attempt_);
        configure_audio = config_.enable_audio && diagnostics_.active_audio_stream_id == 0 &&
            !audio_source_terminal_ && !audio_source_recovery_pending_ &&
            (open_reason != MediaStreamEventReason::InitialOpen ||
             next_audio_configure_attempt_.time_since_epoch().count() == 0 ||
             now >= next_audio_configure_attempt_);
    }
    if (configure_video) {
        configure_media_stream_kind(
            device_id, MediaKind::Video, true, false, open_reason);
    }
    if (configure_audio) {
        configure_media_stream_kind(
            device_id, MediaKind::Audio, true, false, open_reason);
    }
}

void AxtpAdapter::retry_pending_media_source_recoveries(
    const std::string& device_id,
    std::chrono::steady_clock::time_point now)
{
    // Retry only the missing kind whose event-driven recovery failed. The
    // other kind remains open, and this scheduling is independent of HID's
    // transport-level read-error backoff.
    const auto retry_kind = [&](MediaKind kind) {
        bool retry = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto& pending = kind == MediaKind::Video
                ? video_source_recovery_pending_
                : audio_source_recovery_pending_;
            const auto terminal = kind == MediaKind::Video
                ? video_source_terminal_
                : audio_source_terminal_;
            auto& next_attempt = kind == MediaKind::Video
                ? next_video_source_recovery_attempt_
                : next_audio_source_recovery_attempt_;
            if (pending && !terminal && now >= next_attempt) {
                retry = true;
                next_attempt = now + kMediaConfigureRetryInterval;
            }
        }
        if (!retry) {
            return;
        }
        configure_media_stream_kind(
            device_id, kind, false, false, MediaStreamEventReason::SourceRecovery);
        // configure_media_stream_kind() uses public runtime callJson and may
        // itself dispatch source events. Preserve the same lifecycle fence as
        // AxtpAdapter::call() before any callbacks are drained or another kind
        // is considered for retry.
        process_pending_media_source_state_events(device_id);
    };
    retry_kind(MediaKind::Video);
    retry_kind(MediaKind::Audio);
}

void AxtpAdapter::advance_video_reconfigure(const std::string& device_id)
{
    VideoReconfigureOperation operation;
    VideoStreamParamsState state;
    {
        std::lock_guard<std::mutex> lock(runtime_->video_params_mutex);
        if (!runtime_->video_reconfigure.has_value() || runtime_->client == nullptr) {
            return;
        }
        operation = *runtime_->video_reconfigure;
        state = runtime_->video_params_state;
    }

    auto publish_state = [this](VideoStreamParamsState next) {
        {
            std::lock_guard<std::mutex> lock(runtime_->video_params_mutex);
            runtime_->video_params_state = next;
        }
        notify_video_stream_params_state(std::move(next));
    };

    auto finish_failed = [&](std::uint32_t code,
                             std::string message,
                             bool preserve_previous_stream) {
        VideoStreamParamsState failed;
        {
            std::lock_guard<std::mutex> lock(runtime_->video_params_mutex);
            runtime_->session_video_frame_rate = operation.previous_frame_rate;
            runtime_->video_reconfigure.reset();
            runtime_->suppress_video_auto_open = !preserve_previous_stream;
            runtime_->suppress_paired_audio_auto_open = !preserve_previous_stream &&
                (operation.previous_audio_descriptor.has_value() || !operation.previous_audio_open_params.empty());
            auto& current = runtime_->video_params_state;
            current.desired_frame_rate = operation.previous_frame_rate;
            current.state = VideoStreamParamsStateKind::Failed;
            current.phase = VideoStreamParamsPhase::Failed;
            current.rollback_applied = false;
            current.last_error = VideoStreamParamsError{code, std::move(message)};
            if (!preserve_previous_stream) {
                current.active_stream_id.reset();
                current.effective_frame_rate.reset();
            }
            failed = current;
        }
        notify_video_stream_params_state(std::move(failed));
    };

    auto mark_previous_closed = [&](const MediaStreamDescriptor& descriptor) {
        bool removed = false;
        std::uint32_t active_count = 0;
        {
            std::lock_guard<std::mutex> lock(media_stream_mutex_);
            const auto it = active_media_streams_.find(descriptor.key.stream_id);
            if (it != active_media_streams_.end() &&
                it->second.descriptor.kind == descriptor.kind &&
                it->second.descriptor.key.generation == descriptor.key.generation) {
                active_media_streams_.erase(it);
                removed = true;
            }
            active_count = static_cast<std::uint32_t>(active_media_streams_.size());
        }
        if (removed) {
            enqueue_media_stream_events({
                {MediaStreamEventKind::Closed, descriptor},
            });
            std::lock_guard<std::mutex> lock(mutex_);
            if (descriptor.kind == MediaKind::Video) {
                diagnostics_.active_video_stream_id = 0;
            } else {
                diagnostics_.active_audio_stream_id = 0;
            }
            diagnostics_.active_media_streams = active_count;
        }
    };

    const auto advance_close = [&](MediaKind kind,
                                   const std::optional<MediaStreamDescriptor>& descriptor,
                                   bool& close_sent,
                                   bool& close_terminal,
                                   std::uint32_t& failure_code,
                                   std::string& failure_message) {
        if (!descriptor.has_value() || close_terminal) {
            close_terminal = true;
            return true;
        }

        nlohmann::json close_result = nlohmann::json::object();
        const auto method_prefix = std::string(media_kind_name(kind));
        if (!close_sent) {
            axtp::sdk::CallOptions options;
            options.timeout = std::chrono::milliseconds(5000);
            options.progress = [this, &device_id]() {
                publish_runtime_progress(device_id);
            };
            const nlohmann::json params{
                {"streamId", descriptor->key.stream_id},
                {"peerRole", "transmitter"},
                {"reason", "encodingReconfigure"},
            };
            const auto text = runtime_->client->callJson(
                method_prefix + ".closeStream", params.dump(), options);
            const auto error = runtime_->client->lastError();
            if (!error.ok()) {
                failure_code = static_cast<std::uint32_t>(error.code);
                failure_message = error.message.empty()
                    ? method_prefix + ".closeStream failed"
                    : error.message;
                return false;
            }
            close_result = parse_json_object(text).value_or(nlohmann::json::object());
            close_sent = true;
        } else {
            if (std::chrono::steady_clock::now() >= operation.close_deadline) {
                failure_code = kStatusMediaStreamStopFailed;
                failure_message = method_prefix +
                    " stream did not reach a terminal state";
                return false;
            }
            axtp::sdk::CallOptions options;
            options.timeout = std::chrono::milliseconds(500);
            options.progress = [this, &device_id]() {
                publish_runtime_progress(device_id);
            };
            const nlohmann::json params{
                {"streamId", descriptor->key.stream_id},
            };
            const auto text = runtime_->client->callJson(
                method_prefix + ".getStreamState", params.dump(), options);
            if (runtime_->client->lastError().ok()) {
                close_result = parse_json_object(text).value_or(nlohmann::json::object());
            }
        }

        const auto close_state = ascii_lower(json_string_or(close_result, "state"));
        if (close_state == "failed") {
            failure_code = kStatusMediaStreamStopFailed;
            failure_message = method_prefix + " stream close entered failed state";
            return false;
        }
        if (close_state == "closed" || close_result.value("alreadyClosed", false)) {
            close_terminal = true;
            mark_previous_closed(*descriptor);
        }
        return true;
    };

    // NA20 only applies NT10 encoder changes after both receiver-pull legs
    // stop. Send both close requests before opening either replacement, then
    // treat each stream generation as an independent lifecycle boundary.
    const bool has_close_targets = operation.previous_video_descriptor.has_value() ||
        operation.previous_audio_descriptor.has_value();
    if (has_close_targets &&
        (!operation.video_close_terminal || !operation.audio_close_terminal)) {
        if (!operation.video_close_sent && !operation.audio_close_sent) {
            operation.close_deadline = std::chrono::steady_clock::now() +
                std::chrono::seconds(5);
        }
        std::uint32_t close_failure_code = 0;
        std::string close_failure_message;
        if (!advance_close(
                MediaKind::Video,
                operation.previous_video_descriptor,
                operation.video_close_sent,
                operation.video_close_terminal,
                close_failure_code,
                close_failure_message)) {
            finish_failed(close_failure_code, std::move(close_failure_message), true);
            return;
        }
        if (!advance_close(
                MediaKind::Audio,
                operation.previous_audio_descriptor,
                operation.audio_close_sent,
                operation.audio_close_terminal,
                close_failure_code,
                close_failure_message)) {
            finish_failed(close_failure_code, std::move(close_failure_message), false);
            return;
        }
        {
            std::lock_guard<std::mutex> lock(runtime_->video_params_mutex);
            if (runtime_->video_reconfigure.has_value()) {
                *runtime_->video_reconfigure = operation;
            }
        }
        if (!operation.video_close_terminal || !operation.audio_close_terminal) {
            return;
        }
        state.active_stream_id.reset();
        state.phase = VideoStreamParamsPhase::Opening;
        publish_state(state);
    }

    {
        std::lock_guard<std::mutex> lock(runtime_->video_params_mutex);
        if (!runtime_->video_reconfigure.has_value()) {
            return;
        }
        runtime_->video_params_state.phase = VideoStreamParamsPhase::Opening;
        runtime_->video_params_state.state = VideoStreamParamsStateKind::Pending;
        state = runtime_->video_params_state;
    }

    // A device may legally reuse the numeric stream id after close. The
    // generation assigned by configure_media_stream_kind() is the lifecycle
    // boundary, so stale frames/events remain isolated without rejecting the
    // replacement or making rollback impossible on NA20.
    if (!operation.video_opened) {
        const bool opened = configure_media_stream_kind(
            device_id,
            MediaKind::Video,
            false,
            true,
            MediaStreamEventReason::ParameterReconfigure);
        if (opened) {
            operation.video_opened = true;
            std::lock_guard<std::mutex> lock(runtime_->video_params_mutex);
            if (runtime_->video_reconfigure.has_value()) {
                *runtime_->video_reconfigure = operation;
            }
        } else {
            const auto open_error = runtime_->client->lastError();
            std::string last_adapter_event;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                last_adapter_event = diagnostics_.last_event;
            }
            const auto error_code = open_error.ok()
                ? (last_adapter_event.find("source-waiting") != std::string::npos ||
                   last_adapter_event.find("capabilities-unavailable") != std::string::npos
                       ? kStatusMediaSourceUnavailable
                       : kStatusMediaStreamStartFailed)
                : static_cast<std::uint32_t>(open_error.code);
            const auto error_message = open_error.message.empty()
                ? std::string("video.openStream failed")
                : open_error.message;

            if (!operation.rollback &&
                (operation.previous_video_descriptor.has_value() ||
                 !operation.previous_video_open_params.empty())) {
                VideoStreamParamsState rollback_state;
                {
                    std::lock_guard<std::mutex> lock(runtime_->video_params_mutex);
                    if (!runtime_->video_reconfigure.has_value()) {
                        return;
                    }
                    runtime_->session_video_frame_rate = operation.previous_frame_rate;
                    operation.rollback = true;
                    operation.video_opened = false;
                    operation.audio_opened = false;
                    *runtime_->video_reconfigure = operation;
                    auto& current = runtime_->video_params_state;
                    current.desired_frame_rate = operation.previous_frame_rate;
                    current.phase = VideoStreamParamsPhase::Opening;
                    current.last_error = VideoStreamParamsError{error_code, error_message};
                    rollback_state = current;
                }
                notify_video_stream_params_state(std::move(rollback_state));
                return;
            }

            finish_failed(error_code, error_message, false);
            return;
        }
    }

    const bool should_reopen_audio = operation.previous_audio_descriptor.has_value() ||
        !operation.previous_audio_open_params.empty();
    if (should_reopen_audio && !operation.audio_opened) {
        if (!configure_media_stream_kind(
                device_id,
                MediaKind::Audio,
                false,
                true,
                MediaStreamEventReason::ParameterReconfigure)) {
            const auto open_error = runtime_->client->lastError();
            const auto error_code = open_error.ok()
                ? kStatusMediaStreamStartFailed
                : static_cast<std::uint32_t>(open_error.code);
            const auto error_message = open_error.message.empty()
                ? std::string("audio.openStream failed")
                : open_error.message;

            if (!operation.rollback) {
                std::optional<MediaStreamDescriptor> replacement_video;
                {
                    std::lock_guard<std::mutex> lock(media_stream_mutex_);
                    const auto active = std::find_if(
                        active_media_streams_.begin(), active_media_streams_.end(),
                        [](const auto& entry) {
                            return entry.second.descriptor.kind == MediaKind::Video;
                        });
                    if (active != active_media_streams_.end()) {
                        replacement_video = active->second.descriptor;
                    }
                }
                VideoStreamParamsState rollback_state;
                {
                    std::lock_guard<std::mutex> lock(runtime_->video_params_mutex);
                    if (!runtime_->video_reconfigure.has_value()) {
                        return;
                    }
                    runtime_->session_video_frame_rate = operation.previous_frame_rate;
                    operation.rollback = true;
                    operation.previous_video_descriptor = replacement_video;
                    operation.previous_audio_descriptor.reset();
                    operation.video_close_sent = false;
                    operation.audio_close_sent = false;
                    operation.video_close_terminal = !replacement_video.has_value();
                    operation.audio_close_terminal = true;
                    operation.video_opened = false;
                    operation.audio_opened = false;
                    *runtime_->video_reconfigure = operation;
                    auto& current = runtime_->video_params_state;
                    current.desired_frame_rate = operation.previous_frame_rate;
                    current.phase = replacement_video.has_value()
                        ? VideoStreamParamsPhase::Closing
                        : VideoStreamParamsPhase::Opening;
                    current.last_error = VideoStreamParamsError{error_code, error_message};
                    rollback_state = current;
                }
                notify_video_stream_params_state(std::move(rollback_state));
                return;
            }

            finish_failed(error_code, error_message, false);
            return;
        }
        operation.audio_opened = true;
    }

    if (operation.video_opened && (!should_reopen_audio || operation.audio_opened)) {
        VideoStreamParamsState completed;
        {
            std::lock_guard<std::mutex> lock(runtime_->video_params_mutex);
            auto& current = runtime_->video_params_state;
            current.state = operation.rollback
                ? VideoStreamParamsStateKind::RolledBack
                : VideoStreamParamsStateKind::Applied;
            current.phase = operation.rollback
                ? VideoStreamParamsPhase::RolledBack
                : VideoStreamParamsPhase::Streaming;
            current.rollback_applied = operation.rollback;
            current.last_error.reset();
            runtime_->video_reconfigure.reset();
            runtime_->suppress_video_auto_open = false;
            runtime_->suppress_paired_audio_auto_open = false;
            completed = current;
        }
        notify_video_stream_params_state(std::move(completed));
        return;
    }
}

bool AxtpAdapter::configure_media_stream_kind(
    const std::string& device_id,
    MediaKind kind,
    bool update_retry_state,
    bool from_video_reconfigure,
    MediaStreamEventReason open_reason)
{
    if (!config_.enable_media || runtime_->client == nullptr ||
        (kind != MediaKind::Video && kind != MediaKind::Audio)) {
        return false;
    }
    const bool is_video = kind == MediaKind::Video;
    const auto codec_text = [](MediaCodec codec) -> const char* {
        switch (codec) {
        case MediaCodec::H264: return "h264";
        case MediaCodec::H265: return "h265";
        case MediaCodec::Aac: return "aac";
        case MediaCodec::Pcm: return "pcm";
        case MediaCodec::Opaque: return "opaque";
        case MediaCodec::Unknown: default: return "";
        }
    };
    const bool force_video_open = is_video && std::any_of(
        config_.video_codec_preferences.begin(),
        config_.video_codec_preferences.end(),
        [](MediaCodec codec) { return codec == MediaCodec::H265; });
    const bool video_decode_bypass = is_video &&
        config_.video_open_codec_override.has_value() &&
        config_.video_decode_codec_override.has_value() &&
        *config_.video_open_codec_override != *config_.video_decode_codec_override;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& retry = is_video ? diagnostics_.video_retry : diagnostics_.audio_retry;
        ++retry.configure_attempts;
        retry.next_retry_in_ms = 0;
        if (is_video) {
            diagnostics_.video_codec_capabilities_bypassed = false;
            diagnostics_.video_open_request_codec.clear();
            diagnostics_.video_decode_codec.clear();
            diagnostics_.video_codec_decode_bypassed = video_decode_bypass;
            diagnostics_.video_capabilities_last_status = "pending";
            diagnostics_.video_capabilities_last_error.clear();
            diagnostics_.video_open_last_status = "capabilities-pending";
            diagnostics_.video_open_last_error.clear();
            diagnostics_.video_open_last_request.clear();
        }
    }
    const auto mark_retry_failure = [this, is_video, kind](std::string error,
                                                           bool terminal = false,
                                                           bool source_waiting = false) {
        const auto now = std::chrono::steady_clock::now();
        std::lock_guard<std::mutex> lock(mutex_);
        auto& retry = is_video ? diagnostics_.video_retry : diagnostics_.audio_retry;
        retry.last_error = std::move(error);
        retry.terminal = terminal;
        auto& next_configure = is_video
            ? next_video_configure_attempt_
            : next_audio_configure_attempt_;
        auto& retry_attempt = is_video
            ? video_configure_retry_attempt_
            : audio_configure_retry_attempt_;
        if (terminal) {
            if (is_video) {
                video_source_terminal_ = true;
                video_source_recovery_pending_ = false;
            } else {
                audio_source_terminal_ = true;
                audio_source_recovery_pending_ = false;
            }
            next_configure = {};
            retry.next_retry_in_ms = 0;
            return;
        }
        if (source_waiting) {
            if (is_video) {
                video_source_recovery_pending_ = true;
                next_video_source_recovery_attempt_ = now + kSourceWaitingFallbackInterval;
                next_configure = next_video_source_recovery_attempt_;
            } else {
                audio_source_recovery_pending_ = true;
                next_audio_source_recovery_attempt_ = now + kSourceWaitingFallbackInterval;
                next_configure = next_audio_source_recovery_attempt_;
            }
            retry.next_retry_in_ms = static_cast<std::uint64_t>(
                kSourceWaitingFallbackInterval.count() * 1000);
            return;
        }
        if (is_video) {
            video_source_recovery_pending_ = false;
        } else {
            audio_source_recovery_pending_ = false;
        }
        ++retry_attempt;
        next_configure = now + media_retry_delay(retry_attempt, false, kind);
        retry.next_retry_in_ms = static_cast<std::uint64_t>(
            std::max<std::int64_t>(1,
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    next_configure - now).count()));
    };
    std::optional<VideoStreamParamsState> video_params_update;
    if (!from_video_reconfigure) {
        std::lock_guard<std::mutex> lock(runtime_->video_params_mutex);
        if (runtime_->video_reconfigure.has_value() ||
            (kind == MediaKind::Video && runtime_->suppress_video_auto_open) ||
            (kind == MediaKind::Audio && runtime_->suppress_paired_audio_auto_open)) {
            return false;
        }
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if ((kind == MediaKind::Video && video_source_terminal_) ||
            (kind == MediaKind::Audio && audio_source_terminal_)) {
            return false;
        }
    }

    // callJson() keeps polling media through its progress hook.  Do not let
    // that hook commit frames while this open/reopen is still selecting and
    // publishing the replacement generation.  On every return path, append
    // any surviving ingress only after the lifecycle entries produced here.
    auto& lifecycle_transition_depth = is_video
        ? video_lifecycle_transition_depth_
        : audio_lifecycle_transition_depth_;
    lifecycle_transition_depth.fetch_add(1, std::memory_order_acq_rel);
    struct LifecycleTransitionGuard {
        AxtpAdapter& adapter;
        std::atomic<std::uint32_t>& depth;
        MediaKind kind;
        ~LifecycleTransitionGuard()
        {
            {
                std::lock_guard<std::mutex> lock(
                    adapter.pending_media_source_state_mutex_);
                auto& candidates = kind == MediaKind::Video
                    ? adapter.pending_video_open_terminal_orders_
                    : adapter.pending_audio_open_terminal_orders_;
                candidates.clear();
            }
            if (depth.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                adapter.commit_pending_media_batch();
            }
        }
    } lifecycle_transition_guard{*this, lifecycle_transition_depth, kind};

    axtp::ErrorCode last_call_error = axtp::ErrorCode::Success;
    std::string last_call_error_message;
    auto call_json = [this, &device_id, &last_call_error, &last_call_error_message](
                         const std::string& method, const nlohmann::json& params)
        -> std::optional<nlohmann::json> {
        axtp::sdk::CallOptions options;
        options.timeout = std::chrono::milliseconds(5000);
        options.progress = [this, &device_id]() {
            publish_runtime_progress(device_id);
        };
        const auto text = runtime_->client->callJson(method, params.dump(), options);
        const auto error = runtime_->client->lastError();
        last_call_error = error.code;
        last_call_error_message = error.message;
        if (!error.ok()) {
            return std::nullopt;
        }
        return parse_json_object(text);
    };

    const std::string source = is_video ? config_.video_source : config_.audio_source;
    // Snapshot the frame-rate selected for this particular open before the
    // capabilities RPC pumps any more inbound events.  A replacement open
    // must use the reconfigure operation's immutable target (or its previous
    // value during rollback), rather than re-reading ambient session state
    // after close/source lifecycle events have been dispatched.
    std::optional<std::uint32_t> video_open_frame_rate;
    if (is_video) {
        std::lock_guard<std::mutex> lock(runtime_->video_params_mutex);
        if (from_video_reconfigure && runtime_->video_reconfigure.has_value()) {
            const auto& operation = *runtime_->video_reconfigure;
            video_open_frame_rate = operation.rollback
                ? operation.previous_frame_rate
                : operation.requested_frame_rate;
        } else {
            video_open_frame_rate = runtime_->session_video_frame_rate;
        }
    }
    const nlohmann::json source_params{{"source", source}};
    std::optional<nlohmann::json> capabilities;
    const auto cachedCapabilities = runtime_->capabilities_cache.find(kind);
    if (cachedCapabilities != runtime_->capabilities_cache.end()) {
        capabilities = cachedCapabilities->second;
    } else {
        capabilities = call_json(capabilities_method_name(kind), source_params);
        if (capabilities.has_value() && capabilities_are_streamable(*capabilities, source)) {
            runtime_->capabilities_cache[kind] = *capabilities;
        }
    }
    if (!capabilities.has_value()) {
        const bool terminal = runtime_error_is_terminal(last_call_error);
        if (!force_video_open) {
            mark_retry_failure(
                last_call_error_message.empty()
                    ? std::string(media_kind_name(kind)) + " capabilities unavailable"
                    : last_call_error_message,
                terminal);
            std::lock_guard<std::mutex> lock(mutex_);
            diagnostics_.last_event =
                std::string(media_kind_name(kind)) + "-capabilities-unavailable";
            return false;
        }
        // Capabilities are an observation for the current H.265 bring-up
        // path, not a gate.  Some peers answer this read-only RPC late or not
        // at all while still accepting an explicit video.openStream request.
        std::lock_guard<std::mutex> lock(mutex_);
        diagnostics_.video_codec_capabilities_bypassed = true;
        diagnostics_.video_capabilities_last_status = "rpc-error-bypassed";
        diagnostics_.video_capabilities_last_error = "code=" +
            std::to_string(static_cast<std::uint16_t>(last_call_error)) +
            (last_call_error_message.empty() ? std::string{} :
                " message=" + last_call_error_message);
        diagnostics_.video_open_last_status = "capabilities-error-bypassed";
        diagnostics_.video_open_last_error.clear();
    } else {
        // A successful capabilities response is a safe, read-only liveness
        // probe for this device/session. Keep the method that actually worked
        // instead of inventing a transport-specific heartbeat request.
        runtime_->health_probe_method = capabilities_method_name(kind);
        runtime_->health_probe_params = source_params;
        if (!capabilities_are_streamable(*capabilities, source)) {
            if (!force_video_open) {
                mark_retry_failure(
                    std::string(media_kind_name(kind)) + " source waiting", false, true);
                std::lock_guard<std::mutex> lock(mutex_);
                diagnostics_.last_event = std::string(media_kind_name(kind)) + "-source-waiting";
                return false;
            }
            // Keep the response for diagnostics/frame-rate extraction, but do
            // not prevent the explicit video.openStream request.
            std::lock_guard<std::mutex> lock(mutex_);
            diagnostics_.video_codec_capabilities_bypassed = true;
            diagnostics_.video_capabilities_last_status = "source-state-bypassed";
            diagnostics_.video_capabilities_last_error.clear();
            diagnostics_.video_open_last_status = "source-state-bypassed";
            diagnostics_.video_open_last_error.clear();
        } else if (is_video) {
            std::lock_guard<std::mutex> lock(mutex_);
            diagnostics_.video_capabilities_last_status = "ok";
            diagnostics_.video_capabilities_last_error.clear();
        }
    }
    if (is_video) {
        std::vector<std::uint32_t> frame_rates;
        bool supports_reconfigure = true;
        if (capabilities.has_value() && capabilities->contains("sources") &&
            (*capabilities)["sources"].is_array()) {
            for (const auto& entry : (*capabilities)["sources"]) {
                if (!source_matches(entry, source)) {
                    continue;
                }
                if (entry.contains("frameRates") && entry["frameRates"].is_array()) {
                    for (const auto& value : entry["frameRates"]) {
                        const auto frame_rate = json_u32_or(
                            nlohmann::json{{"value", value}}, "value", 0);
                        if (frame_rate != 0) {
                            frame_rates.push_back(frame_rate);
                        }
                    }
                }
                if (entry.contains("supportsReconfigure") &&
                    entry["supportsReconfigure"].is_boolean()) {
                    supports_reconfigure = entry["supportsReconfigure"].get<bool>();
                }
                break;
            }
        }
        std::lock_guard<std::mutex> lock(runtime_->video_params_mutex);
        runtime_->video_frame_rates = std::move(frame_rates);
        runtime_->video_supports_active_reconfigure = supports_reconfigure;
    }

    nlohmann::json open_params;
    std::vector<std::string> video_codec_candidates;
    std::vector<std::string> device_video_codecs;
    if (is_video) {
        {
            std::lock_guard<std::mutex> lock(runtime_->video_params_mutex);
            if (runtime_->video_reconfigure.has_value() &&
                runtime_->video_reconfigure->previous_video_open_params.is_object() &&
                !runtime_->video_reconfigure->previous_video_open_params.empty()) {
                open_params = runtime_->video_reconfigure->previous_video_open_params;
                open_params.erase("frameRate");
                open_params.erase("streamId");
                open_params.erase("state");
                const auto previous_codec = ascii_lower(
                    json_string_or(open_params, "codec", "h264"));
                video_codec_candidates.push_back(
                    previous_codec == "hevc" ? "h265" : previous_codec);
            } else {
                open_params = nlohmann::json{
                    {"source", source},
                    {"peerRole", "transmitter"},
                    {"codec", "h264"},
                    {"transportFormat", "annexb"},
                    {"alignment", "au"},
                    {"streamProfile", "media.video"},
                    {"cursorUnit", "timestampUs"},
                };
                bool codec_list_published = false;
                if (capabilities.has_value() && capabilities->is_object() &&
                    capabilities->contains("sources") &&
                    (*capabilities)["sources"].is_array()) {
                    for (const auto& entry : (*capabilities)["sources"]) {
                        if (!source_matches(entry, source)) continue;
                        if (entry.contains("codecs") && entry["codecs"].is_array()) {
                            codec_list_published = true;
                            for (const auto& value : entry["codecs"]) {
                                if (!value.is_string()) continue;
                                auto available = ascii_lower(value.get<std::string>());
                                if (available == "hevc") available = "h265";
                                if ((available == "h264" || available == "h265") &&
                                    std::find(device_video_codecs.begin(), device_video_codecs.end(),
                                              available) == device_video_codecs.end()) {
                                    device_video_codecs.push_back(std::move(available));
                                }
                            }
                        }
                        break;
                    }
                }
                if (!codec_list_published && capabilities.has_value()) {
                    // Old peers did not publish a codec list and are defined
                    // by the compatibility contract to support H.264 only.
                    device_video_codecs.push_back("h264");
                }
                // H.265 bring-up may deliberately ignore the advertised
                // codec list.  For the legacy H.264 path retain the existing
                // capability intersection behavior.
                const auto codec_preferences = config_.video_codec_preferences.empty()
                    ? std::vector<MediaCodec>{MediaCodec::H264}
                    : config_.video_codec_preferences;
                for (const auto preference : codec_preferences) {
                    const std::string candidate = preference == MediaCodec::H265 ? "h265" :
                        preference == MediaCodec::H264 ? "h264" : "";
                    const auto sourceSupports = [&](const std::string& value) {
                        return std::find(device_video_codecs.begin(), device_video_codecs.end(),
                                         value) != device_video_codecs.end();
                    };
                    if (!candidate.empty() &&
                        (force_video_open || sourceSupports(candidate))) {
                        video_codec_candidates.push_back(candidate);
                    }
                }
                if (video_codec_candidates.empty()) {
                    {
                        std::lock_guard<std::mutex> diagnostics_lock(mutex_);
                        diagnostics_.device_video_codecs = device_video_codecs;
                        diagnostics_.requested_video_codec.clear();
                        diagnostics_.negotiated_video_codec.clear();
                        diagnostics_.video_codec_fallback_reason.clear();
                    }
                    mark_retry_failure("video-codec-unsupported", true);
                    return false;
                }
                {
                    std::lock_guard<std::mutex> diagnostics_lock(mutex_);
                    diagnostics_.video_codec_capabilities_bypassed = force_video_open;
                    diagnostics_.device_video_codecs = device_video_codecs;
                    diagnostics_.requested_video_codec.clear();
                    diagnostics_.negotiated_video_codec.clear();
                    diagnostics_.video_codec_fallback_reason =
                        !codec_preferences.empty() &&
                        codec_preferences.front() == MediaCodec::H265 &&
                        video_codec_candidates.front() != "h265"
                            ? "device-unsupported" : "";
                }
            }
            if (video_open_frame_rate.has_value()) {
                open_params["frameRate"] = *video_open_frame_rate;
            }
        }
    } else {
        {
            std::lock_guard<std::mutex> lock(runtime_->video_params_mutex);
            if (runtime_->video_reconfigure.has_value() &&
                runtime_->video_reconfigure->previous_audio_open_params.is_object() &&
                !runtime_->video_reconfigure->previous_audio_open_params.empty()) {
                open_params = runtime_->video_reconfigure->previous_audio_open_params;
                open_params.erase("streamId");
                open_params.erase("state");
            }
        }
        if (open_params.empty()) {
            open_params = nlohmann::json{
                {"source", source},
                {"peerRole", "transmitter"},
                {"codec", "aac"},
                {"transportFormat", "adts"},
                {"sampleRate", config_.audio_sample_rate == 0 ? 48000 : config_.audio_sample_rate},
                {"channels", choose_audio_channels(*capabilities, source, config_.audio_channels)},
                {"streamProfile", "media.audio"},
                {"cursorUnit", "timestampUs"},
            };
        }
    }

    std::optional<nlohmann::json> response;
    std::uint32_t stream_id = 0;
    std::string requested_video_codec;
    std::string open_failure;
    bool open_failure_terminal = false;
    bool open_failure_source_waiting = false;
    bool video_codec_fallback = false;
    std::string negotiated_video_codec;
    const auto open_candidate = [&](const std::string& video_codec) {
        const std::string open_request_codec = is_video && video_decode_bypass
            ? codec_text(*config_.video_open_codec_override)
            : video_codec;
        if (is_video) {
            open_params["codec"] = open_request_codec;
            requested_video_codec = video_codec;
            std::lock_guard<std::mutex> diagnostics_lock(mutex_);
            ++diagnostics_.video_open_stream_attempts;
            diagnostics_.requested_video_codec = video_codec;
            diagnostics_.video_open_request_codec = open_request_codec;
            diagnostics_.video_decode_codec = video_decode_bypass
                ? codec_text(*config_.video_decode_codec_override) : video_codec;
            diagnostics_.video_open_last_status = "requesting";
            diagnostics_.video_open_last_error.clear();
            diagnostics_.video_open_last_request = open_params.dump();
        }
        response = call_json(open_stream_method_name(kind), open_params);
        if (!response.has_value()) {
            open_failure_terminal = runtime_error_is_terminal(last_call_error);
            const auto errorText = ascii_lower(last_call_error_message);
            open_failure_source_waiting = !open_failure_terminal &&
                (errorText.find("source waiting") != std::string::npos ||
                 errorText.find("source unavailable") != std::string::npos ||
                 errorText.find("source disconnected") != std::string::npos);
            open_failure = last_call_error_message.empty()
                ? std::string(media_kind_name(kind)) + " open failed code=" +
                    std::to_string(static_cast<std::uint16_t>(last_call_error))
                : last_call_error_message;
            if (is_video) {
                std::lock_guard<std::mutex> diagnostics_lock(mutex_);
                diagnostics_.video_open_last_status = open_failure_source_waiting
                    ? "source-waiting" : open_failure_terminal ? "terminal-error" : "rpc-error";
                diagnostics_.video_open_last_error = open_failure;
            }
            return false;
        }

        stream_id = json_u32_or(*response, "streamId", 0);
        if (stream_id == 0) {
            open_failure = std::string(media_kind_name(kind)) +
                " open returned no stream id";
            if (is_video) {
                std::lock_guard<std::mutex> diagnostics_lock(mutex_);
                diagnostics_.video_open_last_status = "invalid-response";
                diagnostics_.video_open_last_error = open_failure;
            }
            response.reset();
            return false;
        }

        if (!is_video) return true;

        const auto response_codec_text = ascii_lower(
            json_string_or(*response, "codec", json_string_or(*response, "format")));
        const auto effective_codec = response_codec_text.empty()
            ? (video_codec == "h264" ? MediaCodec::H264 : MediaCodec::Unknown)
            : codec_for_open_result(kind, *response);
        const auto requested_codec = video_codec == "h265"
            ? MediaCodec::H265 : MediaCodec::H264;
        const auto open_contract_codec = open_request_codec == "h265"
            ? MediaCodec::H265 : MediaCodec::H264;
        if (effective_codec == open_contract_codec &&
            (video_decode_bypass || effective_codec == requested_codec)) {
            negotiated_video_codec = codec_text(effective_codec);
            if (requested_codec == MediaCodec::H265) {
                const auto transport_format = ascii_lower(json_string_or(
                    *response, "transportFormat", json_string_or(
                        open_params, "transportFormat", "annexb")));
                const auto alignment = ascii_lower(json_string_or(
                    *response, "alignment", json_string_or(open_params, "alignment", "au")));
                const auto reorder_depth = json_u32_or(
                    *response, "reorderDepth", json_u32_or(open_params, "reorderDepth", 0));
                if ((!transport_format.empty() && transport_format != "annexb") ||
                    (!alignment.empty() && alignment != "au") || reorder_depth != 0) {
                    call_json(std::string(media_kind_name(kind)) + ".closeStream",
                        nlohmann::json{{"streamId", stream_id}});
                    stream_id = 0;
                    response.reset();
                    open_failure = "video-codec-contract-unsupported";
                    std::lock_guard<std::mutex> diagnostics_lock(mutex_);
                    diagnostics_.video_open_last_status = "contract-rejected";
                    diagnostics_.video_open_last_error = open_failure;
                    return false;
                }
            }
            {
                std::lock_guard<std::mutex> diagnostics_lock(mutex_);
                diagnostics_.video_open_last_status = "accepted";
                diagnostics_.video_open_last_error.clear();
            }
            return true;
        }

        // A peer stream with an absent or mismatched codec is never published.
        // H.264 alone keeps the legacy missing-codec compatibility above;
        // H.265 must always be explicitly confirmed by the response.
        call_json(std::string(media_kind_name(kind)) + ".closeStream",
            nlohmann::json{{"streamId", stream_id}});
        stream_id = 0;
        response.reset();
        open_failure = "video-codec-negotiation-mismatch";
        {
            std::lock_guard<std::mutex> diagnostics_lock(mutex_);
            diagnostics_.video_open_last_status = "codec-mismatch";
            diagnostics_.video_open_last_error = open_failure;
        }
        return false;
    };

    if (is_video) {
        for (std::size_t index = 0; index < video_codec_candidates.size(); ++index) {
            const auto& candidate = video_codec_candidates[index];
            if (open_candidate(candidate)) break;
            const bool can_fallback = candidate == "h265" &&
                (!open_failure_terminal || last_call_error == axtp::ErrorCode::NotSupported) &&
                !open_failure_source_waiting &&
                index + 1 < video_codec_candidates.size();
            if (!can_fallback) break;
            std::lock_guard<std::mutex> lock(mutex_);
            diagnostics_.last_event = "video-codec-fallback-h265-to-h264";
            diagnostics_.video_codec_fallback_reason = open_failure.empty()
                ? "h265-open-rejected" : open_failure;
            video_codec_fallback = true;
        }
    } else {
        open_candidate({});
    }

    if (!response.has_value()) {
        mark_retry_failure(
            open_failure.empty()
                ? std::string(media_kind_name(kind)) + " open failed"
                : open_failure,
            open_failure_terminal,
            open_failure_source_waiting);
        std::lock_guard<std::mutex> lock(mutex_);
        diagnostics_.last_event = std::string(media_kind_name(kind)) + "-open-failed";
        return false;
    }

    bool terminal_candidate_matches = false;
    {
        std::lock_guard<std::mutex> lock(pending_media_source_state_mutex_);
        auto& candidates = is_video
            ? pending_video_open_terminal_orders_
            : pending_audio_open_terminal_orders_;
        const auto candidate = candidates.find(stream_id);
        terminal_candidate_matches = candidate != candidates.end();
        if (terminal_candidate_matches) {
            // The event could not be identified until openStream returned its
            // future stream ID. Once it matches, make its source-state order
            // authoritative so an older receiving event deferred by the same
            // in-flight open cannot reopen the just-terminated stream.
            auto& latest_order = is_video
                ? latest_video_source_state_order_
                : latest_audio_source_state_order_;
            latest_order = std::max(
                latest_order,
                candidate->second);
        }
        candidates.clear();
    }
    bool source_terminal = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& terminal = is_video
            ? video_source_terminal_
            : audio_source_terminal_;
        if (terminal_candidate_matches) {
            terminal = true;
        }
        source_terminal = terminal;
    }
    if (source_terminal) {
        // A terminal source event may arrive while openStream is waiting and
        // is reconciled by the non-recursive progress hook.  The response has
        // already allocated a peer stream, so defer one orphan close to the
        // next top-level pump iteration, but never publish this descriptor.
        std::lock_guard<std::mutex> lock(pending_media_source_state_mutex_);
        pending_orphan_stream_closes_.push({kind, stream_id});
        return false;
    }

    MediaStreamDescriptor descriptor;
    // The adapter owns a device-scoped stream generation. AxentHost binds
    // the final lease session id when publishing/replaying this descriptor.
    descriptor.key.session_id.clear();
    descriptor.key.stream_id = stream_id;
    descriptor.device_id = device_id;
    descriptor.kind = kind;
    descriptor.codec = is_video
        ? (video_decode_bypass
            ? *config_.video_decode_codec_override
            : codec_for_open_result(kind, *response))
        : codec_for_open_result(kind, *response);
    descriptor.source = json_string_or(*response, "source", source);
    descriptor.transport_format = json_string_or(
        *response, "transportFormat", json_string_or(open_params, "transportFormat"));
    descriptor.alignment = json_string_or(
        *response, "alignment", json_string_or(open_params, "alignment", "au"));
    descriptor.stream_profile = json_string_or(
        *response, "streamProfile", json_string_or(open_params, "streamProfile"));
    descriptor.cursor_unit = json_string_or(
        *response, "cursorUnit", json_string_or(open_params, "cursorUnit", "timestampUs"));
    descriptor.sample_rate = json_u32_or(
        *response, "sampleRate", json_u32_or(open_params, "sampleRate", 0));
    descriptor.channels = json_u32_or(
        *response, "channels", json_u32_or(open_params, "channels", 0));
    descriptor.width = json_u32_or(
        *response, "width", json_u32_or(open_params, "width", 0));
    descriptor.height = json_u32_or(
        *response, "height", json_u32_or(open_params, "height", 0));
    descriptor.frame_rate = json_u32_or(
        *response, "frameRate", json_u32_or(open_params, "frameRate", 0));
    descriptor.reorder_depth = json_u32_or(
        *response, "reorderDepth", json_u32_or(open_params, "reorderDepth", 0));
    std::optional<MediaStreamDescriptor> replaced_descriptor;
    std::uint32_t active_stream_count = 0;
    {
        std::lock_guard<std::mutex> lock(media_stream_mutex_);
        auto& generation = media_stream_generations_[stream_id];
        ++generation;
        if (generation == 0) {
            generation = 1;
        }
        descriptor.key.generation = generation;
        const auto existing = active_media_streams_.find(stream_id);
        if (existing != active_media_streams_.end()) {
            replaced_descriptor = existing->second.descriptor;
        }
        active_media_streams_[stream_id] = ActiveMediaStream{descriptor};
        active_stream_count = static_cast<std::uint32_t>(active_media_streams_.size());
        if (source_recovery_cycle_active_) {
            const auto kind_bit = kind == MediaKind::Video ? std::uint8_t{0x01}
                                                            : std::uint8_t{0x02};
            source_recovery_reopen_mask_ &= static_cast<std::uint8_t>(~kind_bit);
            if (source_recovery_reopen_mask_ == 0) {
                source_recovery_cycle_active_ = false;
                source_recovery_close_sent_ = false;
                pending_video_source_recovery_close_.reset();
                pending_audio_source_recovery_close_.reset();
            }
        }
    }
    std::vector<MediaStreamEvent> lifecycle_events;
    if (replaced_descriptor.has_value()) {
        lifecycle_events.push_back(
            {MediaStreamEventKind::Closed,
             std::move(*replaced_descriptor),
             open_reason});
    }
    lifecycle_events.push_back(
        {MediaStreamEventKind::Opened, descriptor, open_reason});
    enqueue_media_stream_events(std::move(lifecycle_events));
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (update_retry_state) {
            diagnostics_.last_event = video_codec_fallback
                ? "video-codec-fallback-h265-to-h264"
                : std::string(media_kind_name(kind)) + "-stream-open";
        }
        auto& retry = is_video ? diagnostics_.video_retry : diagnostics_.audio_retry;
        retry.last_error.clear();
        retry.next_retry_in_ms = 0;
        retry.terminal = false;
        if (kind == MediaKind::Video) {
            diagnostics_.active_video_stream_id = stream_id;
            diagnostics_.requested_video_codec = requested_video_codec;
            diagnostics_.negotiated_video_codec = negotiated_video_codec.empty()
                ? requested_video_codec : negotiated_video_codec;
            diagnostics_.video_decode_codec = codec_text(descriptor.codec);
            diagnostics_.video_codec_decode_bypassed = video_decode_bypass;
        } else {
            diagnostics_.active_audio_stream_id = stream_id;
        }
        diagnostics_.active_media_streams = active_stream_count;
        if (update_retry_state) {
            media_configure_attempts_ = 0;
            next_media_configure_attempt_ = {};
        }
        if (kind == MediaKind::Video) {
            video_configure_retry_attempt_ = 0;
            next_video_configure_attempt_ = {};
            video_source_terminal_ = false;
            video_source_recovery_pending_ = false;
            next_video_source_recovery_attempt_ = {};
        } else {
            audio_configure_retry_attempt_ = 0;
            next_audio_configure_attempt_ = {};
            audio_source_terminal_ = false;
            audio_source_recovery_pending_ = false;
            next_audio_source_recovery_attempt_ = {};
        }
    }
    if (kind == MediaKind::Video) {
        std::lock_guard<std::mutex> lock(runtime_->video_params_mutex);
        runtime_->active_video_open_params = open_params;
        for (const char* key : {"source", "peerRole", "codec", "transportFormat",
                                "alignment", "reorderDepth", "streamProfile",
                                "cursorUnit", "syncGroupId", "castSessionId"}) {
            if (response->contains(key)) {
                runtime_->active_video_open_params[key] = (*response)[key];
            }
        }
        runtime_->video_params_state.source = descriptor.source;
        runtime_->video_params_state.desired_frame_rate =
            runtime_->session_video_frame_rate;
        runtime_->video_params_state.effective_frame_rate = descriptor.frame_rate == 0
            ? runtime_->session_video_frame_rate
            : std::optional<std::uint32_t>(descriptor.frame_rate);
        runtime_->video_params_state.stream_profile = descriptor.stream_profile;
        runtime_->video_params_state.active_stream_id = stream_id;
        if (!runtime_->video_reconfigure.has_value()) {
            runtime_->video_params_state.state = VideoStreamParamsStateKind::Applied;
            runtime_->video_params_state.phase = VideoStreamParamsPhase::Streaming;
            runtime_->video_params_state.rollback_applied = false;
            runtime_->video_params_state.last_error.reset();
            video_params_update = runtime_->video_params_state;
        }
    } else {
        std::lock_guard<std::mutex> lock(runtime_->video_params_mutex);
        runtime_->active_audio_open_params = open_params;
        for (const char* key : {"source", "peerRole", "codec", "transportFormat",
                                "sampleRate", "channels", "streamProfile",
                                "cursorUnit", "syncGroupId", "castSessionId"}) {
            if (response->contains(key)) {
                runtime_->active_audio_open_params[key] = (*response)[key];
            }
        }
    }
    if (video_params_update.has_value()) {
        notify_video_stream_params_state(std::move(*video_params_update));
    }
    return true;
}

MediaFrame AxtpAdapter::frame_from_stream(const std::string& device_id,
                                          std::uint32_t stream_id,
                                          std::uint32_t sequence_id,
                                          std::uint64_t cursor,
                                          std::vector<std::uint8_t> data,
                                          std::uint64_t ingress_token) const
{
    MediaFrame frame;
    frame.device_id = device_id;
    frame.stream_id = stream_id;
    frame.sequence_id = sequence_id;
    frame.cursor = cursor;
    frame.timestamp_us = cursor;
    frame.payload = std::move(data);
    frame.flags = MediaFrameFlag::EndOfFrame;
    frame.binding_epoch = ingress_token;
    {
        std::lock_guard<std::mutex> lock(media_delivery_session_mutex_);
        const auto binding = media_delivery_sessions_.find(device_id);
        if (binding != media_delivery_sessions_.end()) {
            frame.session_id = binding->second;
        }
    }
    {
        std::lock_guard<std::mutex> lock(media_stream_mutex_);
        const auto it = active_media_streams_.find(stream_id);
        if (it != active_media_streams_.end()) {
            frame.generation = it->second.descriptor.key.generation;
            frame.kind = it->second.descriptor.kind;
            frame.codec = it->second.descriptor.codec;
            return frame;
        }
        const auto known_generation = media_stream_generations_.find(stream_id);
        if (known_generation != media_stream_generations_.end()) {
            // Preserve the last known generation for a configured stream that
            // has already closed. is_current_media_frame() will reject it
            // because there is no matching active descriptor. Truly unknown
            // legacy streams keep generation zero and retain compatibility.
            frame.generation = known_generation->second;
            return frame;
        }
    }
    if ((stream_id & 0xF000U) == 0x1000U) {
        frame.kind = MediaKind::Video;
        frame.codec = MediaCodec::H264;
    } else if ((stream_id & 0xF000U) == 0x2000U) {
        frame.kind = MediaKind::Audio;
        frame.codec = MediaCodec::Aac;
    } else {
        frame.kind = MediaKind::Unknown;
        frame.codec = MediaCodec::Opaque;
    }
    return frame;
}

void AxtpAdapter::handle_stream_payload(const std::string& device_id,
                                        std::uint32_t stream_id,
                                        std::uint32_t sequence_id,
                                        std::uint64_t cursor,
                                        std::vector<std::uint8_t> data,
                                        std::uint64_t ingress_token)
{
    if (!device_context_) {
        const auto context = find_device_context(device_id);
        if (context && context->adapter) {
            DeviceContextOperation context_operation(context);
            if (!context_operation) {
                return;
            }
            context->adapter->handle_stream_payload(
                device_id,
                stream_id,
                sequence_id,
                cursor,
                std::move(data),
                ingress_token);
        }
        return;
    }
    note_inbound_activity();
    // This must happen at ingress, rather than during commit.  poll() queues
    // source-state events for later reconciliation, so by commit time a
    // same-numeric-ID stream may already describe the replacement generation
    // and the logical Host lease may have changed as well.
    auto frame = frame_from_stream(
        device_id, stream_id, sequence_id, cursor, std::move(data), ingress_token);
    {
        std::lock_guard<std::mutex> lock(pending_media_ingress_mutex_);
        pending_media_ingress_frames_.push(
            PendingMediaIngressFrame{std::move(frame)});
    }
}

bool AxtpAdapter::is_current_media_frame(const MediaFrame& frame) const
{
    if (!device_context_) {
        const auto context = find_device_context(frame.device_id);
        if (!context || !context->adapter) {
            return false;
        }
        DeviceContextOperation context_operation(context);
        return context_operation && context->adapter->is_current_media_frame(frame);
    }
    // A physical stream generation can outlive several logical Host leases.
    // The session id captured when the frame entered staging is therefore a
    // second, independent lifetime fence.  Without this check a frame that
    // was already committed to the dispatcher FIFO before unbind could be
    // delivered after a replacement lease acquired the same physical stream.
    // Keep unbound direct-adapter/test delivery compatible: a frame with an
    // empty session id is valid only while no logical binding exists.
    {
        std::lock_guard<std::mutex> lock(media_delivery_session_mutex_);
        const auto binding = media_delivery_sessions_.find(frame.device_id);
        if (binding == media_delivery_sessions_.end()) {
            if (!frame.session_id.empty() || frame.binding_epoch != 0) {
                return false;
            }
        } else if (frame.session_id.empty() || binding->second != frame.session_id ||
                   frame.binding_epoch == 0 ||
                   frame.binding_epoch !=
                       media_binding_epoch_.load(std::memory_order_acquire)) {
            // The frame may have been decoded into the runtime Core event
            // queue before unbind/rebind, then delivered to this adapter only
            // afterwards. Session IDs alone are insufficient because the
            // deferred callback observes the replacement binding when it is
            // finally materialized.
            return false;
        }
    }
    if (frame.generation == 0) {
        return true;
    }
    std::lock_guard<std::mutex> lock(media_stream_mutex_);
    const auto active = active_media_streams_.find(frame.stream_id);
    return active != active_media_streams_.end() &&
        active->second.descriptor.device_id == frame.device_id &&
        active->second.descriptor.key.generation == frame.generation;
}

void AxtpAdapter::commit_pending_media_batch()
{
    // Source-state events from the same runtime poll can change the stream
    // generation.  Leave ingress staged until the pump has reconciled those
    // events, then stamp frames from the resulting descriptor and append them
    // after lifecycle entries in one dispatcher FIFO.
    if (has_pending_media_source_state_events()) {
        return;
    }

    std::queue<PendingMediaIngressFrame> ingress;
    {
        std::lock_guard<std::mutex> lock(pending_media_ingress_mutex_);
        ingress.swap(pending_media_ingress_frames_);
    }
    if (ingress.empty()) {
        return;
    }

    std::vector<MediaFrame> frames;
    frames.reserve(ingress.size());
    std::queue<PendingMediaIngressFrame> retained;
    while (!ingress.empty()) {
        auto pending = std::move(ingress.front());
        ingress.pop();
        const bool kind_is_fenced =
            (pending.frame.kind == MediaKind::Video &&
             video_lifecycle_transition_depth_.load(std::memory_order_acquire) != 0) ||
            (pending.frame.kind == MediaKind::Audio &&
             audio_lifecycle_transition_depth_.load(std::memory_order_acquire) != 0) ||
            (pending.frame.kind == MediaKind::Unknown &&
             (video_lifecycle_transition_depth_.load(std::memory_order_acquire) != 0 ||
              audio_lifecycle_transition_depth_.load(std::memory_order_acquire) != 0));
        if (kind_is_fenced) {
            retained.push(std::move(pending));
            continue;
        }
        frames.push_back(std::move(pending.frame));
    }
    if (!retained.empty()) {
        // A different kind may continue through a slow open/reopen.  Put the
        // fenced kind back ahead of ingress that arrived while partitioning
        // so its own FIFO order is unchanged when the lifecycle guard opens.
        std::lock_guard<std::mutex> lock(pending_media_ingress_mutex_);
        while (!pending_media_ingress_frames_.empty()) {
            retained.push(std::move(pending_media_ingress_frames_.front()));
            pending_media_ingress_frames_.pop();
        }
        pending_media_ingress_frames_.swap(retained);
    }
    if (frames.empty()) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(pending_media_dispatch_mutex_);
        for (auto& frame : frames) {
            PendingMediaDispatchItem item;
            item.kind = PendingMediaDispatchItem::Kind::Frame;
            item.frame = std::move(frame);
            pending_media_dispatch_items_.push(std::move(item));
        }
        const auto depth = pending_media_dispatch_items_.size();
        media_dispatch_queue_depth_.store(depth, std::memory_order_relaxed);
        update_high_water(media_dispatch_queue_high_water_, depth);
    }
    notify_media_dispatch();
}

void AxtpAdapter::drain_pending_media_callbacks()
{
    // The AXTP pump is the sole production committer for ingress frames. It
    // reconciles all source-state events from a poll before calling
    // commit_pending_media_batch(), which makes the lifecycle entries precede
    // frames in this FIFO. Letting this independent dispatcher commit ingress
    // would race a poll halfway through an event/frame batch and could invert
    // that order.
    std::lock_guard<std::recursive_mutex> dispatch_lock(
        media_callback_dispatch_mutex_);
    if (draining_media_callbacks_) {
        return;
    }
    draining_media_callbacks_ = true;
    struct DrainStateGuard {
        bool& active;
        ~DrainStateGuard() { active = false; }
    } drain_state_guard{draining_media_callbacks_};

    for (;;) {
        PendingMediaDispatchItem item;
        {
            std::lock_guard<std::mutex> lock(pending_media_dispatch_mutex_);
            if (pending_media_dispatch_items_.empty()) {
                media_dispatch_queue_depth_.store(0, std::memory_order_relaxed);
                return;
            }
            item = std::move(pending_media_dispatch_items_.front());
            pending_media_dispatch_items_.pop();
            media_dispatch_queue_depth_.store(
                pending_media_dispatch_items_.size(), std::memory_order_relaxed);
        }

        if (item.kind == PendingMediaDispatchItem::Kind::StreamEvent) {
            MediaStreamEventCallback event_callback;
            {
                std::lock_guard<std::mutex> lock(media_callback_mutex_);
                event_callback = media_stream_event_callback_;
            }
            if (!event_callback) {
                continue;
            }
            try {
                event_callback(std::move(item.event));
            } catch (...) {
                std::lock_guard<std::mutex> lock(mutex_);
                diagnostics_.last_error = "media stream event callback threw";
            }
            continue;
        }

        MediaFrameCallback callback;
        {
            std::lock_guard<std::mutex> lock(media_callback_mutex_);
            callback = media_frame_callback_;
        }
        if (!callback || !is_current_media_frame(item.frame)) {
            continue;
        }
        const auto device_id = item.frame.device_id;
        if (control_in_flight_.load(std::memory_order_relaxed) != 0) {
            media_frames_dispatched_during_control_call_.fetch_add(
                1, std::memory_order_relaxed);
        }
        try {
            callback(device_id, std::move(item.frame));
        } catch (...) {
            std::lock_guard<std::mutex> lock(mutex_);
            diagnostics_.last_error = "media frame callback threw";
        }
    }
}

void AxtpAdapter::notify_media_dispatch()
{
    {
        std::lock_guard<std::mutex> lock(media_dispatch_mutex_);
        ++media_dispatch_generation_;
    }
    media_dispatch_cv_.notify_one();
}

void AxtpAdapter::run_media_dispatcher()
{
    std::uint64_t observed_generation = 0;
    for (;;) {
        {
            std::unique_lock<std::mutex> lock(media_dispatch_mutex_);
            media_dispatch_cv_.wait(lock, [this, &observed_generation]() {
                return stop_media_dispatcher_ ||
                    media_dispatch_generation_ != observed_generation;
            });
            if (stop_media_dispatcher_) {
                return;
            }
            observed_generation = media_dispatch_generation_;
        }
        drain_pending_media_callbacks();
    }
}

void AxtpAdapter::refresh_diagnostics_locked()
{
    diagnostics_.control_queue_depth =
        control_queue_depth_.load(std::memory_order_relaxed);
    diagnostics_.control_in_flight =
        control_in_flight_.load(std::memory_order_relaxed);
    diagnostics_.control_outstanding_high_water =
        control_outstanding_high_water_.load(std::memory_order_relaxed);
    diagnostics_.media_dispatch_queue_depth =
        media_dispatch_queue_depth_.load(std::memory_order_relaxed);
    diagnostics_.media_dispatch_queue_high_water =
        media_dispatch_queue_high_water_.load(std::memory_order_relaxed);
    diagnostics_.media_frames_dispatched_during_control_call =
        media_frames_dispatched_during_control_call_.load(
            std::memory_order_relaxed);
    diagnostics_.session_health = session_health_;
    diagnostics_.health_probe_failures = health_probe_failures_;
    diagnostics_.session_recoveries = session_recoveries_;
    diagnostics_.last_session_recovery_reason = last_session_recovery_reason_;
    diagnostics_.requested_probe_mode = config_.session_probe_mode;
    diagnostics_.effective_probe_mode = effective_probe_mode_;
    diagnostics_.negotiated_heartbeat_interval_ms = negotiated_heartbeat_interval_ms_;
    diagnostics_.inbound_activity_generation = inbound_activity_generation_;
    diagnostics_.heartbeat_attempts = heartbeat_attempts_;
    diagnostics_.heartbeat_acks = heartbeat_acks_;
    diagnostics_.heartbeat_timeouts = heartbeat_timeouts_;
    diagnostics_.legacy_probe_attempts = legacy_probe_attempts_;
    diagnostics_.legacy_probe_successes = legacy_probe_successes_;
    diagnostics_.legacy_fallbacks = legacy_fallbacks_;
    diagnostics_.legacy_fallback_reason = legacy_fallback_reason_;
    const auto now = std::chrono::steady_clock::now();
    const auto remainingMs = [now](std::chrono::steady_clock::time_point deadline) {
        if (deadline.time_since_epoch().count() == 0 || deadline <= now) {
            return std::uint64_t{0};
        }
        return static_cast<std::uint64_t>(
            std::max<std::int64_t>(1,
                std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count()));
    };
    diagnostics_.video_retry.next_retry_in_ms = diagnostics_.video_retry.terminal
        ? 0 : remainingMs(video_source_recovery_pending_
            ? next_video_source_recovery_attempt_ : next_video_configure_attempt_);
    diagnostics_.audio_retry.next_retry_in_ms = diagnostics_.audio_retry.terminal
        ? 0 : remainingMs(audio_source_recovery_pending_
            ? next_audio_source_recovery_attempt_ : next_audio_configure_attempt_);
}

void testing::AxtpAdapterTestSeam::disconnect_session(AxtpAdapter& adapter)
{
    if (!adapter.device_context_) {
        for (const auto& context : adapter.device_contexts()) {
            if (context && context->adapter) {
                disconnect_session(*context->adapter);
            }
        }
        return;
    }
    std::lock_guard<std::mutex> client_lock(adapter.client_mutex_);
    if (adapter.runtime_->client != nullptr) {
        adapter.runtime_->client->close();
    }
}

void testing::AxtpAdapterTestSeam::bind_media_delivery_session(
    AxtpAdapter& adapter,
    const std::string& device_id,
    const std::string& session_id)
{
    adapter.bind_media_delivery_session(device_id, session_id);
}

void testing::AxtpAdapterTestSeam::release_session(
    AxtpAdapter& adapter,
    const std::string& device_id)
{
    adapter.reset_session_for_device(device_id);
}

void testing::AxtpAdapterTestSeam::stop_session_pump(AxtpAdapter& adapter)
{
    if (!adapter.device_context_) {
        for (const auto& context : adapter.device_contexts()) {
            if (context && context->adapter) {
                stop_session_pump(*context->adapter);
            }
        }
        return;
    }
    std::thread pump;
    {
        std::lock_guard<std::mutex> lock(adapter.mutex_);
        pump = adapter.request_stop_session_pump_locked();
    }
    if (pump.joinable()) {
        pump.join();
    }
}

void testing::AxtpAdapterTestSeam::enqueue_stream_payload(
    AxtpAdapter& adapter,
    const std::string& device_id,
    std::uint32_t stream_id,
    std::uint32_t sequence_id,
    std::uint64_t cursor,
    std::vector<std::uint8_t> data)
{
    adapter.handle_stream_payload(
        device_id, stream_id, sequence_id, cursor, std::move(data));
}

void testing::AxtpAdapterTestSeam::reopen_media_streams(
    AxtpAdapter& adapter,
    const std::string& device_id)
{
    if (!adapter.device_context_) {
        const auto context = adapter.find_device_context(device_id);
        if (context && context->adapter) {
            reopen_media_streams(*context->adapter, device_id);
        }
        return;
    }
    std::lock_guard<std::mutex> session_lock(adapter.session_mutex_);
    std::lock_guard<std::mutex> client_lock(adapter.client_mutex_);
    // Exercise an explicit same-ID receiver-pull replacement. Production
    // configure_media_streams() intentionally leaves healthy active legs
    // alone, so it cannot stand in for this lifecycle transition now that
    // recovery retries are per kind.
    if (adapter.config_.enable_video) {
        adapter.configure_media_stream_kind(
            device_id,
            MediaKind::Video,
            true,
            false,
            MediaStreamEventReason::SourceRecovery);
    }
    if (adapter.config_.enable_audio) {
        adapter.configure_media_stream_kind(
            device_id,
            MediaKind::Audio,
            true,
            false,
            MediaStreamEventReason::SourceRecovery);
    }
}

void testing::AxtpAdapterTestSeam::drain_media_callbacks(AxtpAdapter& adapter)
{
    if (!adapter.device_context_) {
        for (const auto& context : adapter.device_contexts()) {
            if (context && context->adapter) {
                drain_media_callbacks(*context->adapter);
            }
        }
        return;
    }
    // Tests that stop the pump can explicitly advance its normally-owned
    // ingress-to-dispatch boundary before draining callbacks.
    adapter.commit_pending_media_batch();
    adapter.drain_pending_media_callbacks();
}

bool testing::AxtpAdapterTestSeam::is_current_media_frame(
    const AxtpAdapter& adapter,
    const MediaFrame& frame)
{
    return adapter.is_current_media_frame(frame);
}

bool testing::AxtpAdapterTestSeam::hold_device_context_operation(
    AxtpAdapter& adapter,
    const std::string& device_id,
    const std::function<void()>& callback)
{
    if (adapter.device_context_) {
        return false;
    }
    const auto context = adapter.find_device_context(device_id);
    AxtpAdapter::DeviceContextOperation operation(context);
    if (!operation) {
        return false;
    }
    if (callback) {
        callback();
    }
    return true;
}

} // namespace axent
