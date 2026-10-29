#include <chrono>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "axent/adapters/axtp_adapter.hpp"
#include "axent/core/device_manager.hpp"
#include "axtp_adapter_test_seam.hpp"

#include "core/protocol/wire/inbound_processor.hpp"
#include "core/protocol/wire/outbound_processor.hpp"
#include "hidapi/hid_transport.hpp"

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << std::endl;
        throw std::runtime_error(message);
    }
}

void require(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << message << std::endl;
        throw std::runtime_error(message);
    }
}

struct CapturingByteWriter : axtp::IByteWriter {
    axtp::Bytes bytes;

    void writeBytes(const axtp::Byte* data, std::size_t size) override
    {
        bytes.insert(bytes.end(), data, data + size);
    }
};

struct CapturingPayloadSink : axtp::IPayloadSink {
    std::vector<axtp::ControlPayload> controls;
    std::vector<axtp::RpcPayload> rpcs;
    std::vector<axtp::StreamPayload> streams;

    void onControl(axtp::ControlPayload payload) override
    {
        controls.push_back(std::move(payload));
    }

    void onRpc(axtp::RpcPayload payload) override
    {
        rpcs.push_back(std::move(payload));
    }

    void onStream(axtp::StreamPayload payload) override
    {
        streams.push_back(std::move(payload));
    }
};

axtp::Bytes encode_control(axtp::ControlPayload payload)
{
    CapturingByteWriter writer;
    axtp::OutboundProcessor outbound(writer);
    outbound.sendControl(std::move(payload));
    return writer.bytes;
}

axtp::Bytes encode_rpc(axtp::RpcPayload payload)
{
    CapturingByteWriter writer;
    axtp::OutboundProcessor outbound(writer);
    outbound.sendRpc(std::move(payload));
    return writer.bytes;
}

axtp::Bytes encode_stream(axtp::StreamPayload payload)
{
    CapturingByteWriter writer;
    axtp::OutboundProcessor outbound(writer);
    outbound.sendStream(std::move(payload));
    return writer.bytes;
}

class ScriptedAxtpTransport : public axtp::ITransport {
public:
    struct BusinessRequest {
        axtp::EndpointMetadata endpoint;
        std::string body;
    };

    void bind(axtp::IByteSink& sink) override
    {
        sink_ = &sink;
    }

    void open() override
    {
        open_ = true;
    }

    void close() override
    {
        open_ = false;
    }

    void poll() override
    {
        std::queue<axtp::Bytes> pending;
        {
            std::lock_guard<std::mutex> lock(rx_mutex_);
            pending.swap(rx_queue_);
            const auto now = std::chrono::steady_clock::now();
            auto delayed = delayed_rx_.begin();
            while (delayed != delayed_rx_.end()) {
                if (delayed->due > now) {
                    ++delayed;
                    continue;
                }
                pending.push(std::move(delayed->bytes));
                delayed = delayed_rx_.erase(delayed);
            }
        }
        while (!pending.empty()) {
            inject(pending.front());
            pending.pop();
        }
    }

    void injectStream(std::uint32_t stream_id,
                      std::uint32_t sequence_id,
                      std::uint64_t cursor,
                      axtp::Bytes data)
    {
        axtp::StreamPayload stream;
        stream.streamId = stream_id;
        stream.seqId = sequence_id;
        stream.cursor = cursor;
        stream.data = std::move(data);
        std::lock_guard<std::mutex> lock(rx_mutex_);
        rx_queue_.push(encode_stream(std::move(stream)));
    }

    void injectEvent(axtp::EventId event_id, std::string event_name, std::string body)
    {
        axtp::RpcPayload event;
        event.encoding = axtp::RpcEncoding::Json;
        event.op = axtp::RpcOp::Event;
        event.methodOrEventId = static_cast<std::uint32_t>(event_id);
        event.bodyEncoding = axtp::RpcBodyEncoding::None;
        event.meta.sourceProtocol = axtp::SourceProtocol::JsonRpc;
        event.meta.jsonSid = "axent-session-1";
        event.meta.jsonMethodOrEventName = std::move(event_name);
        event.body.assign(body.begin(), body.end());
        queueBytes(encode_rpc(std::move(event)));
    }

    void injectStreamThenEvent(std::uint32_t stream_id,
                               std::uint32_t sequence_id,
                               std::uint64_t cursor,
                               axtp::Bytes data,
                               axtp::EventId event_id,
                               std::string event_name,
                               std::string body)
    {
        axtp::StreamPayload stream;
        stream.streamId = stream_id;
        stream.seqId = sequence_id;
        stream.cursor = cursor;
        stream.data = std::move(data);

        axtp::RpcPayload event;
        event.encoding = axtp::RpcEncoding::Json;
        event.op = axtp::RpcOp::Event;
        event.methodOrEventId = static_cast<std::uint32_t>(event_id);
        event.bodyEncoding = axtp::RpcBodyEncoding::None;
        event.meta.sourceProtocol = axtp::SourceProtocol::JsonRpc;
        event.meta.jsonSid = "axent-session-1";
        event.meta.jsonMethodOrEventName = std::move(event_name);
        event.body.assign(body.begin(), body.end());

        auto stream_bytes = encode_stream(std::move(stream));
        auto event_bytes = encode_rpc(std::move(event));
        std::lock_guard<std::mutex> lock(rx_mutex_);
        rx_queue_.push(std::move(stream_bytes));
        rx_queue_.push(std::move(event_bytes));
    }

    void releaseHeldVideoOpenResponse()
    {
        std::lock_guard<std::mutex> lock(rx_mutex_);
        if (!held_video_open_response_.empty()) {
            rx_queue_.push(std::move(held_video_open_response_));
            held_video_open_response_.clear();
        }
    }

    void sendBytes(const axtp::Byte* data, std::size_t size) override
    {
        CapturingPayloadSink payload_sink;
        axtp::InboundProcessor inbound(payload_sink);
        inbound.onBytes(data, size);

        for (const auto& control : payload_sink.controls) {
            if (control.opcode != axtp::ControlOpcode::Open) {
                continue;
            }
            saw_control_open = true;
            axtp::ControlPayload accept;
            accept.opcode = axtp::ControlOpcode::Accept;
            accept.controlId = control.controlId;
            accept.statusCode = axtp::ErrorCode::Success;
            inject(encode_control(accept));
            inject(encode_rpc(axtp::JsonRpcEncoder::makeHello()));
        }

        for (const auto& rpc : payload_sink.rpcs) {
            if (rpc.op == axtp::RpcOp::Identify) {
                saw_identify = true;
                inject(encode_rpc(axtp::JsonRpcEncoder::makeIdentified("axent-session-1")));
                continue;
            }
            if (rpc.op == axtp::RpcOp::Request) {
                saw_business_request = true;
                last_business_sid = rpc.meta.jsonSid;
                {
                    std::lock_guard<std::mutex> lock(business_request_mutex_);
                    last_business_request_.endpoint = rpc.meta.endpoint;
                    last_business_request_.body.assign(
                        rpc.body.begin(), rpc.body.end());
                }
                const bool capability_request =
                    rpc.methodOrEventId == static_cast<std::uint32_t>(
                        axtp::MethodId::VideoGetStreamCapabilities) ||
                    rpc.methodOrEventId == static_cast<std::uint32_t>(
                        axtp::MethodId::AudioGetStreamCapabilities);
                auto dropped_capability_requests = drop_next_capability_responses.load();
                if (capability_request && dropped_capability_requests > 0 &&
                    drop_next_capability_responses.compare_exchange_strong(
                        dropped_capability_requests, dropped_capability_requests - 1)) {
                    continue;
                }
                if (capability_request && drop_capability_responses &&
                    silent_reset != nullptr && silent_reset->load()) {
                    // Simulate a NA20 reset which leaves the HID path open but
                    // stops answering business requests.  Control open/hello
                    // and identify remain available so a replacement session
                    // can complete its normal handshake.
                    continue;
                }
                axtp::RpcPayload response;
                response.encoding = axtp::RpcEncoding::Json;
                response.op = axtp::RpcOp::RequestResponse;
                response.requestId = rpc.requestId;
                response.methodOrEventId = rpc.methodOrEventId;
                response.statusCode = axtp::ErrorCode::Success;
                response.bodyEncoding = axtp::RpcBodyEncoding::None;
                response.meta.sourceProtocol = axtp::SourceProtocol::JsonRpc;
                response.meta.jsonSid = rpc.meta.jsonSid;
                std::string body = R"({"ok":true})";
                if (rpc.methodOrEventId ==
                    static_cast<std::uint32_t>(axtp::MethodId::VideoGetStreamCapabilities)) {
                    ++video_capability_requests;
                    body = R"({"supported":true,"openModes":["receiver_pull"],"sourceState":{"available":true,"state":"receiving"},"sources":[{"sourceId":"wireless_cast","currentState":"receiving","frameRates":[15,25,30],"supportsReconfigure":true}]})";
                    if (advertise_video_codecs) {
                        body = R"({"supported":true,"openModes":["receiver_pull"],"sourceState":{"available":true,"state":"receiving"},"sources":[{"sourceId":"wireless_cast","currentState":"receiving","frameRates":[15,25,30],"supportsReconfigure":true,"codecs":["h264","h265"]}]})";
                    }
                    if (fail_next_video_capabilities.exchange(false)) {
                        body = R"({"supported":false,"openModes":[],"sourceState":{"available":false,"state":"waiting"},"sources":[{"sourceId":"wireless_cast","currentState":"waiting"}]})";
                    }
                } else if (rpc.methodOrEventId ==
                           static_cast<std::uint32_t>(axtp::MethodId::AudioGetStreamCapabilities)) {
                    body = R"({"supported":true,"openModes":["receiver_pull"],"sourceState":{"available":true,"state":"receiving"},"sources":[{"sourceId":"wireless_cast_audio","currentState":"receiving","channels":[2],"sampleRates":[48000]}]})";
                } else if (rpc.methodOrEventId ==
                           static_cast<std::uint32_t>(axtp::MethodId::VideoOpenStream)) {
                    const auto request_number = ++video_open_requests;
                    const std::string wire_body(rpc.body.begin(), rpc.body.end());
                    const auto params = nlohmann::json::parse(wire_body);
                    {
                        std::lock_guard<std::mutex> lock(requests_mutex);
                        video_open_wire_bodies.push_back(wire_body);
                        video_open_params.push_back(params);
                        media_request_order.push_back("video.open");
                    }
                    auto failures = fail_video_open_count.load();
                    const auto requested_codec = params.value("codec", std::string{"h264"});
                    if (reject_h265_open && requested_codec == "h265") {
                        response.statusCode = axtp::ErrorCode::NotSupported;
                        body = R"({"error":"h265 unsupported"})";
                    } else if (failures > 0 &&
                        fail_video_open_count.compare_exchange_strong(failures, failures - 1)) {
                        response.statusCode = axtp::ErrorCode::MediaFramerateUnsupported;
                        body = R"({"error":"unsupported frame rate"})";
                    } else {
                        nlohmann::json result{
                            {"streamId", unique_video_stream_ids ? 1000 + request_number : 1},
                            {"state", "streaming"},
                            {"source", "wireless_cast"},
                            {"codec", omit_video_open_codec ? "" : requested_codec},
                        };
                        if (params.contains("frameRate")) {
                            result["frameRate"] = params["frameRate"];
                        }
                        body = result.dump();
                    }
                } else if (rpc.methodOrEventId ==
                           static_cast<std::uint32_t>(axtp::MethodId::AudioOpenStream)) {
                    ++audio_open_requests;
                    const auto params = nlohmann::json::parse(
                        std::string(rpc.body.begin(), rpc.body.end()));
                    {
                        std::lock_guard<std::mutex> lock(requests_mutex);
                        audio_open_params.push_back(params);
                        media_request_order.push_back("audio.open");
                    }
                    auto failures = fail_audio_open_count.load();
                    if (failures > 0 &&
                        fail_audio_open_count.compare_exchange_strong(failures, failures - 1)) {
                        response.statusCode = axtp::ErrorCode::MediaStreamStartFailed;
                        body = R"({"error":"audio open failed"})";
                    } else {
                        body = R"({"streamId":2,"state":"streaming","source":"wireless_cast_audio","codec":"aac","transportFormat":"adts"})";
                    }
                } else if (rpc.methodOrEventId ==
                           static_cast<std::uint32_t>(axtp::MethodId::VideoCloseStream)) {
                    ++video_close_requests;
                    const auto params = nlohmann::json::parse(
                        std::string(rpc.body.begin(), rpc.body.end()));
                    {
                        std::lock_guard<std::mutex> lock(requests_mutex);
                        video_close_params.push_back(params);
                        media_request_order.push_back("video.close");
                    }
                    body = nlohmann::json{
                        {"streamId", params.at("streamId")},
                        {"state", close_returns_closing.exchange(false) ? "closing" : "closed"},
                        {"reason", "encodingReconfigure"},
                    }.dump();
                } else if (rpc.methodOrEventId ==
                           static_cast<std::uint32_t>(axtp::MethodId::VideoGetStreamState)) {
                    ++video_state_requests;
                    const auto params = nlohmann::json::parse(
                        std::string(rpc.body.begin(), rpc.body.end()));
                    body = nlohmann::json{
                        {"streamId", params.at("streamId")},
                        {"state", "closed"},
                        {"source", "wireless_cast"},
                        {"codec", "h264"},
                    }.dump();
                } else if (rpc.methodOrEventId ==
                           static_cast<std::uint32_t>(axtp::MethodId::AudioCloseStream)) {
                    ++audio_close_requests;
                    const auto params = nlohmann::json::parse(
                        std::string(rpc.body.begin(), rpc.body.end()));
                    {
                        std::lock_guard<std::mutex> lock(requests_mutex);
                        audio_close_params.push_back(params);
                        media_request_order.push_back("audio.close");
                    }
                    body = nlohmann::json{
                        {"streamId", params.at("streamId")},
                        {"state", audio_close_returns_closing.exchange(false)
                            ? "closing" : "closed"},
                        {"reason", "encodingReconfigure"},
                    }.dump();
                } else if (rpc.methodOrEventId ==
                           static_cast<std::uint32_t>(axtp::MethodId::AudioGetStreamState)) {
                    ++audio_state_requests;
                    const auto params = nlohmann::json::parse(
                        std::string(rpc.body.begin(), rpc.body.end()));
                    body = nlohmann::json{
                        {"streamId", params.at("streamId")},
                        {"state", "closed"},
                        {"source", "wireless_cast_audio"},
                        {"codec", "aac"},
                    }.dump();
                }
                if (capability_request && capability_business_error.load()) {
                    response.statusCode = axtp::ErrorCode::NotSupported;
                    body = R"({"error":"probe business rejection"})";
                }
                response.body = axtp::Bytes(body.begin(), body.end());
                if (queue_terminal_frame_before_next_response.exchange(false)) {
                    axtp::RpcPayload event;
                    event.encoding = axtp::RpcEncoding::Json;
                    event.op = axtp::RpcOp::Event;
                    event.methodOrEventId = static_cast<std::uint32_t>(
                        axtp::EventId::VideoStreamSourceStateChanged);
                    event.bodyEncoding = axtp::RpcBodyEncoding::None;
                    event.meta.sourceProtocol = axtp::SourceProtocol::JsonRpc;
                    event.meta.jsonSid = "axent-session-1";
                    event.meta.jsonMethodOrEventName = "video.streamSourceStateChanged";
                    const std::string event_body =
                        R"({"source":"wireless_cast","state":"stopped","reason":"sender_stopped","activeStreamId":1})";
                    event.body.assign(event_body.begin(), event_body.end());
                    queueBytes(encode_rpc(std::move(event)));

                    axtp::StreamPayload stream;
                    stream.streamId = 1;
                    stream.seqId = 900;
                    stream.cursor = 9000000;
                    stream.data = {0x00, 0x00, 0x01, 0x41};
                    queueBytes(encode_stream(std::move(stream)));
                    queueBytes(encode_rpc(response));
                } else {
                    auto response_bytes = encode_rpc(std::move(response));
                    const bool video_open_response = rpc.methodOrEventId ==
                        static_cast<std::uint32_t>(axtp::MethodId::VideoOpenStream);
                    const auto delay_ms = rpc.methodOrEventId ==
                            static_cast<std::uint32_t>(axtp::MethodId::VideoRequestKeyFrame)
                        ? delay_next_keyframe_response_ms.exchange(0)
                        : 0;
                    if (video_open_response &&
                        hold_next_video_open_response.exchange(false)) {
                        std::lock_guard<std::mutex> lock(rx_mutex_);
                        held_video_open_response_ = std::move(response_bytes);
                        video_open_response_held.store(true);
                    } else if (delay_ms > 0) {
                        queueDelayedBytes(
                            std::move(response_bytes),
                            std::chrono::milliseconds(delay_ms));
                    } else {
                        inject(response_bytes);
                    }
                }
            }
        }
    }

    axtp::TransportProfile profile() const override
    {
        return axtp::TransportProfile{
            axtp::TransportKind::Hid,
            axtp::AxtpWireMode::FramedBinary,
            axtp::jsonBinaryRpcEncoding(),
            false,
            false,
            true,
            4096,
        };
    }

    BusinessRequest lastBusinessRequest() const
    {
        std::lock_guard<std::mutex> lock(business_request_mutex_);
        return last_business_request_;
    }

    bool saw_control_open = false;
    bool saw_identify = false;
    bool saw_business_request = false;
    std::string last_business_sid;
    std::atomic<std::uint32_t> video_open_requests{0};
    std::atomic<std::uint32_t> audio_open_requests{0};
    std::atomic<std::uint32_t> video_capability_requests{0};
    std::atomic<std::uint32_t> video_close_requests{0};
    std::atomic<std::uint32_t> video_state_requests{0};
    std::atomic<std::uint32_t> audio_close_requests{0};
    std::atomic<std::uint32_t> audio_state_requests{0};
    std::atomic<bool> fail_next_video_capabilities{false};
    std::atomic<bool> queue_terminal_frame_before_next_response{false};
    std::atomic<bool> close_returns_closing{false};
    std::atomic<bool> audio_close_returns_closing{false};
    std::atomic<int> fail_video_open_count{0};
    std::atomic<int> fail_audio_open_count{0};
    bool drop_capability_responses = false;
    std::atomic<bool>* silent_reset = nullptr;
    std::atomic<int> drop_next_capability_responses{0};
    std::atomic<bool> capability_business_error{false};
    std::atomic<int> delay_next_keyframe_response_ms{0};
    std::atomic<bool> hold_next_video_open_response{false};
    std::atomic<bool> video_open_response_held{false};
    bool advertise_video_codecs = false;
    bool reject_h265_open = false;
    bool omit_video_open_codec = false;
    bool unique_video_stream_ids = false;
    std::mutex requests_mutex;
    std::vector<nlohmann::json> video_open_params;
    std::vector<std::string> video_open_wire_bodies;
    std::vector<nlohmann::json> video_close_params;
    std::vector<nlohmann::json> audio_open_params;
    std::vector<nlohmann::json> audio_close_params;
    std::vector<std::string> media_request_order;

private:
    struct DelayedBytes {
        std::chrono::steady_clock::time_point due;
        axtp::Bytes bytes;
    };

    void queueBytes(axtp::Bytes bytes)
    {
        std::lock_guard<std::mutex> lock(rx_mutex_);
        rx_queue_.push(std::move(bytes));
    }

    void queueDelayedBytes(axtp::Bytes bytes, std::chrono::milliseconds delay)
    {
        std::lock_guard<std::mutex> lock(rx_mutex_);
        delayed_rx_.push_back({std::chrono::steady_clock::now() + delay, std::move(bytes)});
    }

    void inject(const axtp::Bytes& bytes)
    {
        if (sink_ != nullptr) {
            sink_->onBytes(bytes.data(), bytes.size());
        }
    }

    axtp::IByteSink* sink_ = nullptr;
    mutable std::mutex business_request_mutex_;
    BusinessRequest last_business_request_;
    std::mutex rx_mutex_;
    std::queue<axtp::Bytes> rx_queue_;
    std::vector<DelayedBytes> delayed_rx_;
    axtp::Bytes held_video_open_response_;
    bool open_ = false;
};

axent::TransportSelector na20_selector()
{
    axent::TransportSelector selector;
    selector.kind = axent::TransportKind::Hid;
    selector.vendor_id = 0x0581;
    selector.product_id = 0x2582;
    selector.usage_page = 0x0081;
    selector.report_id = 0x05;
    selector.input_report_size = 0;
    selector.output_report_size = 0;
    return selector;
}

bool wait_for_frames(const std::vector<axent::MediaFrame>& frames,
                     std::mutex& frames_mutex,
                     std::size_t expected_count)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (std::chrono::steady_clock::now() < deadline) {
        {
            std::lock_guard<std::mutex> lock(frames_mutex);
            if (frames.size() >= expected_count) {
                return true;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return false;
}

bool wait_for_stream_events(const std::vector<axent::MediaStreamEvent>& events,
                            std::mutex& events_mutex,
                            std::size_t expected_count,
                            std::chrono::milliseconds timeout = std::chrono::seconds(1))
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        {
            std::lock_guard<std::mutex> lock(events_mutex);
            if (events.size() >= expected_count) {
                return true;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return false;
}

bool wait_until(const std::function<bool()>& predicate)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return false;
}

} // namespace

int main()
{
    const auto defaults = axent::AxtpAdapter::na20_defaults();
    require(defaults.selector.kind == axent::TransportKind::Hid, "NA20 should default to HID transport");
    require(defaults.selector.vendor_id == 0x0581, "NA20 VID mismatch");
    require(defaults.selector.product_id == 0x2582, "NA20 PID mismatch");
    require(defaults.selector.usage_page == 0x0081, "NA20 usage page mismatch");
    require(defaults.selector.report_id == 0x05, "NA20 report id mismatch");
    require(defaults.selector.input_report_size == 0, "NA20 input report size should be auto");
    require(defaults.selector.output_report_size == 0, "NA20 output report size should be auto");
    require(defaults.endpoint_delivery_mode == axent::EndpointDeliveryMode::LocalProjection,
            "NA20 must default to legacy local Endpoint projection");

    axent::transport::HidDeviceInfo hid_device;
    hid_device.path = "hid-path-001";
    hid_device.vendorId = 0x0581;
    hid_device.productId = 0x2582;
    hid_device.serialNumber = "NA20-SERIAL";
    hid_device.manufacturer = "Mostorm";
    hid_device.product = "NA20";
    hid_device.usagePage = 0x0081;
    hid_device.usage = 0x0001;
    hid_device.interfaceNumber = 3;
    hid_device.busType = "usb";

    const auto descriptor = axent::testing::AxtpAdapterTestSeam::descriptor_from_hid_device(hid_device);
    require(descriptor.id == "hid:0581:2582:NA20-SERIAL", "descriptor id should include VID/PID/serial");
    require(descriptor.online, "descriptor should be online");
    require(descriptor.kind == axent::TransportKind::Hid, "descriptor kind mismatch");
    require(descriptor.path == "hid-path-001", "descriptor path mismatch");
    require(descriptor.serial_number == "NA20-SERIAL", "descriptor serial mismatch");

    const auto snapshot = axent::AxtpAdapter::snapshot_from_descriptor(descriptor);
    require(snapshot.id == descriptor.id, "snapshot id mismatch");
    require(snapshot.adapter == "axtp", "snapshot adapter mismatch");
    require(snapshot.identity.vendor == "Mostorm", "snapshot vendor mismatch");
    require(snapshot.identity.model == "NA20", "snapshot model mismatch");
    require(snapshot.identity.serial_number == "NA20-SERIAL", "snapshot serial mismatch");
    require(snapshot.connection.online, "snapshot should be online");
    require(snapshot.connection.transport == "hid", "snapshot transport mismatch");
    require(snapshot.status.health == "ready", "snapshot health mismatch");

    auto canonical_descriptor = descriptor;
    canonical_descriptor.vendor_id = 0x1234;
    canonical_descriptor.product_id = 0x5678;
    canonical_descriptor.serial_number = "SERIAL-1";
    canonical_descriptor.path = "hid-path-a";
    const auto canonical_snapshot =
        axent::AxtpAdapter::snapshot_from_descriptor(canonical_descriptor);
    require(canonical_snapshot.endpoint_id == "ep_3340a334b47934f471968db6b1470da6",
            "serial-backed HID identity must use the canonical Endpoint algorithm");
    require(canonical_snapshot.endpoint_delivery_mode ==
                axent::EndpointDeliveryMode::LocalProjection,
            "legacy HID projection must default to local delivery");

    canonical_descriptor.path = "hid-path-b";
    const auto moved_path_snapshot =
        axent::AxtpAdapter::snapshot_from_descriptor(canonical_descriptor);
    require(moved_path_snapshot.endpoint_id == canonical_snapshot.endpoint_id,
            "HID path churn must not change a serial-backed Endpoint");

    canonical_descriptor.serial_number = "serial-1";
    const auto changed_serial_snapshot =
        axent::AxtpAdapter::snapshot_from_descriptor(canonical_descriptor);
    require(changed_serial_snapshot.endpoint_id != canonical_snapshot.endpoint_id,
            "serial bytes must remain case-sensitive Endpoint evidence");

    canonical_descriptor.serial_number.clear();
    canonical_descriptor.path = "hid-path-without-serial-a";
    const auto path_only_snapshot_a =
        axent::AxtpAdapter::snapshot_from_descriptor(canonical_descriptor);
    canonical_descriptor.path = "hid-path-without-serial-b";
    const auto path_only_snapshot_b =
        axent::AxtpAdapter::snapshot_from_descriptor(canonical_descriptor);
    require(path_only_snapshot_a.endpoint_id.empty() &&
                path_only_snapshot_b.endpoint_id.empty(),
            "path-only HID devices must not receive a synthesized stable Endpoint");

    canonical_descriptor.serial_number = "SERIAL-1";
    const auto native_relay_snapshot = axent::AxtpAdapter::snapshot_from_descriptor(
        canonical_descriptor, axent::EndpointDeliveryMode::NativeRelay);
    require(native_relay_snapshot.endpoint_id == canonical_snapshot.endpoint_id,
            "delivery mode must not change canonical Endpoint identity");
    require(native_relay_snapshot.endpoint_delivery_mode ==
                axent::EndpointDeliveryMode::NativeRelay,
            "explicit NativeRelay projection must be recorded on the snapshot");

    axent::AxtpAdapter adapter(defaults);
    require(axent::testing::AxtpAdapterTestSeam::matches_selector(adapter, hid_device),
            "default adapter should match NA20 device");

    const auto unique_projection =
        axent::testing::AxtpAdapterTestSeam::project_hid_devices(
            defaults.selector, {hid_device});
    require(unique_projection.devices.size() == 1 &&
                unique_projection.routable_device_ids.count(descriptor.id) == 1 &&
                unique_projection.ambiguous_device_ids.empty(),
            "a unique HID identity must remain discoverable and routable");

    auto duplicate_serial_device = hid_device;
    duplicate_serial_device.path = "hid-path-duplicate-serial";
    duplicate_serial_device.interfaceNumber = 4;
    const auto ambiguous_projection =
        axent::testing::AxtpAdapterTestSeam::project_hid_devices(
            defaults.selector, {hid_device, duplicate_serial_device});
    require(ambiguous_projection.devices.empty() &&
                ambiguous_projection.routable_device_ids.empty() &&
                ambiguous_projection.ambiguous_device_ids.count(descriptor.id) == 1,
            "distinct HID providers sharing canonical serial evidence must fail closed");
    axent::DeviceManager ambiguous_devices;
    for (const auto& projected : ambiguous_projection.devices) {
        ambiguous_devices.upsert(projected);
    }
    require(ambiguous_devices.list().empty(),
            "ambiguous HID discovery must not reach DeviceManager as a refresh");

    auto wrong_usage = hid_device;
    wrong_usage.usagePage = 0x1234;
    require(!axent::testing::AxtpAdapterTestSeam::matches_selector(adapter, wrong_usage),
            "usage page filter should reject mismatches");

    auto path_config = defaults;
    path_config.selector = na20_selector();
    path_config.selector.path = "specific-path";
    axent::AxtpAdapter path_adapter(path_config);
    require(!axent::testing::AxtpAdapterTestSeam::matches_selector(path_adapter, hid_device),
            "path filter should reject different path");
    hid_device.path = "specific-path";
    require(axent::testing::AxtpAdapterTestSeam::matches_selector(path_adapter, hid_device),
            "path filter should accept matching path");

    axent::transport::HidTransportOptions options;
    options.inputReportSize = 0;
    options.outputReportSize = 0;
    options.readBufferSize = 4096;
    options.reportId = 0x05;
    auto mapped_options =
        axent::testing::AxtpAdapterTestSeam::hid_options_from_selector(na20_selector());
    require(mapped_options.vendorId == 0x0581, "mapped VID mismatch");
    require(mapped_options.productId == 0x2582, "mapped PID mismatch");
    require(mapped_options.usagePage == 0x0081, "mapped usage page mismatch");
    require(mapped_options.reportId == 0x05, "mapped report id mismatch");
    require(mapped_options.inputReportSize == 0, "mapped input report size should remain auto");
    require(mapped_options.outputReportSize == 0, "mapped output report size should remain auto");
    require(mapped_options.useReadThread, "real HID adapter should use read thread like NearCast");
    bool trace_called = false;
    mapped_options.reportTrace = [&](const axent::transport::HidReportTrace&) {
        trace_called = true;
    };
    axent::transport::HidReportTrace mapped_trace;
    mapped_options.reportTrace(mapped_trace);
    require(trace_called, "mapped HID options should preserve report trace callback storage");

    axent::transport::HidReportTrace timeout;
    timeout.kind = axent::transport::HidReportTraceKind::ReadTimeout;
    timeout.timeoutMs = 1000;
    axent::testing::AxtpAdapterTestSeam::record_hid_trace(adapter, timeout);
    axent::transport::HidReportTrace raw_read;
    raw_read.kind = axent::transport::HidReportTraceKind::ReadReport;
    axent::testing::AxtpAdapterTestSeam::record_hid_trace(adapter, raw_read);
    axent::transport::HidReportTrace accepted;
    accepted.kind = axent::transport::HidReportTraceKind::AcceptedReport;
    axent::testing::AxtpAdapterTestSeam::record_hid_trace(adapter, accepted);
    axent::transport::HidReportTrace read_error;
    read_error.kind = axent::transport::HidReportTraceKind::ReadError;
    read_error.message = "read failed";
    axent::testing::AxtpAdapterTestSeam::record_hid_trace(adapter, read_error);
    axent::transport::HidReportTrace write_error;
    write_error.kind = axent::transport::HidReportTraceKind::WriteError;
    write_error.message = "write failed";
    axent::testing::AxtpAdapterTestSeam::record_hid_trace(adapter, write_error);
    axent::transport::HidReportTrace dropped;
    dropped.kind = axent::transport::HidReportTraceKind::DroppedReportId;
    dropped.reportId = 0x06;
    dropped.expectedReportId = 0x05;
    axent::testing::AxtpAdapterTestSeam::record_hid_trace(adapter, dropped);

    const auto diagnostics = adapter.diagnostics();
    require(diagnostics.read_reports == 1, "accepted report count should not double-count raw reads");
    require(diagnostics.read_errors == 1, "read error count mismatch");
    require(diagnostics.write_errors == 1, "write error count mismatch");
    require(diagnostics.dropped_reports == 1, "dropped report count mismatch");
    require(diagnostics.last_event == "dropped-report-id", "last event should track last trace");
    require(diagnostics.last_error == "write failed", "last HID error should keep the latest error message");

    auto unavailable_adapter = axent::testing::AxtpAdapterTestSeam::make(
        defaults, [](const axent::transport::HidTransportOptions&) {
        return std::unique_ptr<axtp::ITransport>{};
    });
    const auto result = unavailable_adapter->call("hid:0581:2582:NA20-SERIAL", "status.get", {});
    require(result.status == axent::ControlStatus::Unavailable,
            "real adapter without a transport should be unavailable");
    require(result.body.at("error") == "AXTP HID transport target is unavailable",
            "unavailable transport message mismatch");
    const auto firmware_route = unavailable_adapter->start_firmware_update(
        "hid:0581:2582:NA20-SERIAL", "firmware.bin");
    require(firmware_route.status == axent::ControlStatus::Unavailable &&
                firmware_route.body.at("error") ==
                    "AXTP firmware update skeleton only",
            "real adapter firmware route must remain unavailable");

    {
        auto local_config = defaults;
        local_config.enable_media = false;
        local_config.enable_session_health_probe = false;
        ScriptedAxtpTransport* local_transport = nullptr;
        auto local_adapter = axent::testing::AxtpAdapterTestSeam::make(
            local_config,
            [&](const axent::transport::HidTransportOptions&) {
                auto transport = std::make_unique<ScriptedAxtpTransport>();
                local_transport = transport.get();
                return transport;
            });
        axent::AdapterControlRequest local_request;
        local_request.device_id = "hid:0581:2582:LOCAL-PROJECTION";
        local_request.source_endpoint_id = "ep-app-001";
        local_request.destination_endpoint_id = "ep-device-001";
        local_request.endpoint_delivery_mode =
            axent::EndpointDeliveryMode::LocalProjection;
        local_request.method = "audio.getAlgorithmConfig";
        local_request.params = {{"detail", "business"}};
        const auto local_result = local_adapter->call(local_request);
        require(local_result.status == axent::ControlStatus::Ok,
                "LocalProjection routed control should complete");
        require(local_transport != nullptr,
                "LocalProjection control should construct its physical transport");
        const auto local_wire = local_transport->lastBusinessRequest();
        require(!local_wire.endpoint.src.has_value() &&
                    !local_wire.endpoint.dst.has_value(),
                "LocalProjection must omit native Endpoint metadata");
        require(nlohmann::json::parse(local_wire.body) ==
                    nlohmann::json{{"detail", "business"}},
                "LocalProjection params must contain business data only");
    }

    {
        auto native_config = defaults;
        native_config.enable_media = false;
        native_config.enable_session_health_probe = false;
        native_config.endpoint_delivery_mode =
            axent::EndpointDeliveryMode::NativeRelay;
        ScriptedAxtpTransport* native_transport = nullptr;
        auto native_adapter = axent::testing::AxtpAdapterTestSeam::make(
            native_config,
            [&](const axent::transport::HidTransportOptions&) {
                auto transport = std::make_unique<ScriptedAxtpTransport>();
                native_transport = transport.get();
                return transport;
            });
        axent::AdapterControlRequest native_request;
        native_request.device_id = "hid:0581:2582:NATIVE-RELAY";
        native_request.source_endpoint_id = "ep-app-001";
        native_request.destination_endpoint_id = "ep-device-001";
        native_request.endpoint_delivery_mode =
            axent::EndpointDeliveryMode::NativeRelay;
        native_request.method = "audio.getAlgorithmConfig";
        native_request.params = {{"detail", "business"}};
        const auto native_result = native_adapter->call(native_request);
        require(native_result.status == axent::ControlStatus::Ok,
                "NativeRelay control must accept a legacy response without Endpoint metadata");
        const auto native_operation = native_adapter->call_async(native_request);
        const auto native_async_result = native_operation->wait();
        require(native_async_result.status == axent::ControlStatus::Ok,
                "NativeRelay async control must retain routed metadata through the FIFO");
        require(native_transport != nullptr,
                "NativeRelay control should construct its physical transport");
        const auto native_wire = native_transport->lastBusinessRequest();
        require(native_wire.endpoint.src ==
                    std::optional<std::string>{"ep-app-001"} &&
                    native_wire.endpoint.dst ==
                    std::optional<std::string>{"ep-device-001"},
                "NativeRelay must carry source and destination through runtime metadata");
        require(nlohmann::json::parse(native_wire.body) ==
                    nlohmann::json{{"detail", "business"}},
                "NativeRelay params must contain business data only");
        const auto native_firmware = native_adapter->start_firmware_update(
            native_request, "firmware.bin");
        require(native_firmware.status == axent::ControlStatus::Unavailable,
                "routed firmware must retain the existing AXTP skeleton result");
    }

    // Concurrent first use of one canonical physical ID must create and open
    // exactly one leaf runtime. Both callers then share that device context.
    {
        auto concurrent_config = defaults;
        concurrent_config.enable_media = false;
        concurrent_config.enable_session_health_probe = false;
        std::atomic<int> concurrent_factory_calls{0};
        std::atomic<int> concurrent_open_calls{0};
        auto concurrent_adapter = axent::testing::AxtpAdapterTestSeam::make(
            concurrent_config,
            [&](const axent::transport::HidTransportOptions&) {
                ++concurrent_factory_calls;
                class CountingOpenTransport final : public ScriptedAxtpTransport {
                public:
                    explicit CountingOpenTransport(std::atomic<int>& open_calls)
                        : open_calls_(open_calls)
                    {
                    }

                    void open() override
                    {
                        ++open_calls_;
                        ScriptedAxtpTransport::open();
                    }

                private:
                    std::atomic<int>& open_calls_;
                };
                return std::make_unique<CountingOpenTransport>(
                    concurrent_open_calls);
            });
        std::atomic<int> concurrent_ready{0};
        std::atomic<bool> concurrent_start{false};
        axent::ControlResult concurrent_first;
        axent::ControlResult concurrent_second;
        const auto concurrent_call = [&](axent::ControlResult& call_result) {
            ++concurrent_ready;
            while (!concurrent_start.load()) {
                std::this_thread::yield();
            }
            call_result = concurrent_adapter->call(
                "hid:0581:2582:NA20-CONCURRENT",
                "audio.getAlgorithmConfig", {});
        };
        std::thread concurrent_thread_a(
            concurrent_call, std::ref(concurrent_first));
        std::thread concurrent_thread_b(
            concurrent_call, std::ref(concurrent_second));
        while (concurrent_ready.load() != 2) {
            std::this_thread::yield();
        }
        concurrent_start.store(true);
        concurrent_thread_a.join();
        concurrent_thread_b.join();

        require(concurrent_first.status == axent::ControlStatus::Ok &&
                    concurrent_second.status == axent::ControlStatus::Ok,
                "concurrent calls for one physical device should both succeed");
        require(concurrent_factory_calls.load() == 1 &&
                    concurrent_open_calls.load() == 1,
                "concurrent first use must create and open one physical runtime");
    }

    // A broad HID selector is discovery-only. It cannot turn an arbitrary
    // logical name into the first matching physical handle.
    {
        std::atomic<int> broad_selector_factory_calls{0};
        auto broad_selector_adapter =
            axent::testing::AxtpAdapterTestSeam::make(
                defaults,
                [&](const axent::transport::HidTransportOptions&) {
                    ++broad_selector_factory_calls;
                    return std::make_unique<ScriptedAxtpTransport>();
                });
        const auto broad_selector_result = broad_selector_adapter->call(
            "logical-device-without-physical-selector",
            "audio.getAlgorithmConfig", {});
        require(broad_selector_result.status == axent::ControlStatus::NotFound,
                "broad selector must reject a non-canonical device id");
        for (const auto& malformed_id : std::vector<std::string>{
                 "hid:581:2582:SHORT-VID",
                 "hid:+581:2582:SIGNED-VID",
                 "hid:0581:258:SHORT-PID",
                 "hid:0581:25g2:NON-HEX-PID",
                 "hid:0581:25A2:UPPERCASE-PID",
                 "hid:0581:2582:"}) {
            const auto malformed_result = broad_selector_adapter->call(
                malformed_id, "audio.getAlgorithmConfig", {});
            require(malformed_result.status == axent::ControlStatus::NotFound,
                    "broad selector must reject malformed canonical HID ids");
        }
        const auto mismatched_pid_result = broad_selector_adapter->call(
            "hid:0581:2581:UNCONFIGURED-PID",
            "audio.getAlgorithmConfig", {});
        require(mismatched_pid_result.status == axent::ControlStatus::NotFound,
                "canonical HID id must not bypass the configured PID allowlist");
        require(broad_selector_factory_calls.load() == 0,
                "rejected broad-selector routing must not construct a runtime");
    }

    // Retiring a context must wait for an operation that already captured its
    // shared_ptr, while new callers fail fast until the old leaf is closed.
    // This models a WS lazy-open racing with the Host's final-lease reset.
    {
        auto lifecycle_config = defaults;
        lifecycle_config.enable_media = false;
        lifecycle_config.enable_session_health_probe = false;
        std::atomic<int> lifecycle_factory_calls{0};
        auto lifecycle_adapter = axent::testing::AxtpAdapterTestSeam::make(
            lifecycle_config,
            [&](const axent::transport::HidTransportOptions&) {
                ++lifecycle_factory_calls;
                return std::make_unique<ScriptedAxtpTransport>();
            });
        const std::string lifecycle_device = "hid:0581:2582:LIFECYCLE-RACE";
        require(lifecycle_adapter->call(
                    lifecycle_device, "audio.getAlgorithmConfig", {})
                    .status == axent::ControlStatus::Ok,
                "lifecycle race fixture should open its first context");

        std::atomic<bool> holder_entered{false};
        std::atomic<bool> holder_release{false};
        std::atomic<bool> holder_ok{false};
        std::thread holder([&]() {
            holder_ok.store(
                axent::testing::AxtpAdapterTestSeam::hold_device_context_operation(
                    *lifecycle_adapter,
                    lifecycle_device,
                    [&]() {
                        holder_entered.store(true);
                        while (!holder_release.load()) {
                            std::this_thread::yield();
                        }
                    }));
        });
        require(wait_until([&]() { return holder_entered.load(); }),
                "lifecycle holder should enter the context gate");

        std::atomic<bool> reset_done{false};
        std::thread reset([&]() {
            axent::testing::AxtpAdapterTestSeam::release_session(
                *lifecycle_adapter, lifecycle_device);
            reset_done.store(true);
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        require(!reset_done.load(),
                "context reset must wait for an in-flight manager operation");

        bool saw_retiring = false;
        const auto retiring_deadline =
            std::chrono::steady_clock::now() + std::chrono::seconds(1);
        while (std::chrono::steady_clock::now() < retiring_deadline &&
               !saw_retiring) {
            const auto result = lifecycle_adapter->call(
                lifecycle_device, "audio.getAlgorithmConfig", {});
            saw_retiring = result.status == axent::ControlStatus::NotFound;
            if (!saw_retiring) {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        }
        holder_release.store(true);
        holder.join();
        reset.join();
        require(holder_ok.load(), "lifecycle holder should complete normally");
        require(saw_retiring,
                "new calls must not create a replacement while a context retires");
        require(reset_done.load(), "context reset should complete after the holder exits");

        const auto reopened = lifecycle_adapter->call(
            lifecycle_device, "audio.getAlgorithmConfig", {});
        require(reopened.status == axent::ControlStatus::Ok,
                "a device should reopen after its retired context is fully closed");
        require(lifecycle_factory_calls.load() == 2,
                "retirement must prevent duplicate leaf creation during reset");
    }

    auto session_config = defaults;
    session_config.enable_media = false;
    session_config.enable_session_health_probe = false;
    std::vector<ScriptedAxtpTransport*> session_transports;
    std::vector<std::string> session_transport_serials;
    int session_transport_factory_calls = 0;
    auto session_adapter = axent::testing::AxtpAdapterTestSeam::make(
        session_config, [&](const axent::transport::HidTransportOptions& options) {
        ++session_transport_factory_calls;
        auto transport = std::make_unique<ScriptedAxtpTransport>();
        session_transports.push_back(transport.get());
        session_transport_serials.push_back(options.serialNumber);
        return transport;
    });
    const std::string first_session_device = "hid:0581:2582:NA20-FIRST";
    const std::string second_session_device = "hid:0581:2582:NA20-SECOND";
    const auto call_result = session_adapter->call(
        first_session_device, "audio.getAlgorithmConfig", {});
    require(call_result.status == axent::ControlStatus::Ok,
            std::string("scripted AXTP call should succeed: ") + call_result.body.dump());
    require(call_result.body.at("ok") == true, "scripted AXTP response should be parsed");
    require(session_transports.size() == 1 && session_transports[0] != nullptr,
            "first scripted transport should be constructed");
    require(session_transports[0]->saw_control_open,
            "first AXTP session should send control open");
    require(session_transports[0]->saw_identify,
            "first AXTP session should send identify");
    require(session_transports[0]->saw_business_request,
            "first AXTP call should send a business request");
    require(session_transports[0]->last_business_sid == "axent-session-1",
            "first business request should use app-ready sid");

    std::string second_session_error;
    const auto second_session_status = session_adapter->open_session_status(
        second_session_device, second_session_error, false);
    require(second_session_status == axent::ControlStatus::Ok,
            std::string("a second AXTP device should open independently: ") +
                second_session_error);
    const auto second_call_result = session_adapter->call(
        second_session_device, "audio.getAlgorithmConfig", {});
    require(second_call_result.status == axent::ControlStatus::Ok &&
                second_call_result.body.at("ok") == true,
            "control on the second AXTP device should succeed while the first is open");
    require(session_transport_factory_calls == 2 && session_transports.size() == 2,
            "two devices must own two physical AXTP transports");
    require(session_transport_serials.size() == 2 &&
                session_transport_serials[0] == "NA20-FIRST" &&
                session_transport_serials[1] == "NA20-SECOND",
            "each device context must open its own serial-number selector");
    require(session_adapter->diagnostics(first_session_device).open &&
                session_adapter->diagnostics(second_session_device).open,
            "both AXTP device diagnostics should remain open concurrently");

    axent::testing::AxtpAdapterTestSeam::release_session(
        *session_adapter, first_session_device);
    require(!session_adapter->diagnostics(first_session_device).open &&
                session_adapter->diagnostics(second_session_device).open,
            "releasing the first AXTP session must not close the second device");
    const auto second_after_release = session_adapter->call(
        second_session_device, "audio.getAlgorithmConfig", {});
    require(second_after_release.status == axent::ControlStatus::Ok &&
                session_transport_factory_calls == 2,
            "control on the second device must reuse its transport after first-device release");
    const auto reopened_first = session_adapter->call(
        first_session_device, "audio.getAlgorithmConfig", {});
    require(reopened_first.status == axent::ControlStatus::Ok &&
                session_transport_factory_calls == 3 &&
                session_transport_serials.back() == "NA20-FIRST",
            "a released device should reopen only its own physical transport");

    // Both physical devices intentionally advertise the same numeric AXTP
    // stream IDs. Device-local contexts must keep their descriptors,
    // generations, bindings, and frame callbacks separate.
    {
        auto multi_media_config = defaults;
        multi_media_config.enable_session_health_probe = false;
        int multi_media_factory_calls = 0;
        std::mutex multi_frames_mutex;
        std::vector<axent::MediaFrame> multi_frames;
        auto multi_media_adapter = axent::testing::AxtpAdapterTestSeam::make(
            multi_media_config,
            [&](const axent::transport::HidTransportOptions&) {
                ++multi_media_factory_calls;
                return std::make_unique<ScriptedAxtpTransport>();
            });
        multi_media_adapter->set_media_frame_callback(
            [&](std::string device_id, axent::MediaFrame frame) {
                frame.device_id = std::move(device_id);
                std::lock_guard<std::mutex> lock(multi_frames_mutex);
                multi_frames.push_back(std::move(frame));
            });

        const std::string media_device_a = "hid:0581:2582:NA20-MEDIA-A";
        const std::string media_device_b = "hid:0581:2582:NA20-MEDIA-B";
        std::string media_error_a;
        std::string media_error_b;
        require(multi_media_adapter->open_session(media_device_a, media_error_a),
                std::string("first multi-device media session should open: ") +
                    media_error_a);
        require(multi_media_adapter->open_session(media_device_b, media_error_b),
                std::string("second multi-device media session should open: ") +
                    media_error_b);
        require(multi_media_factory_calls == 2,
                "multi-device media sessions must use independent transports");

        const auto media_descriptors_a =
            multi_media_adapter->active_media_stream_descriptors(media_device_a);
        const auto media_descriptors_b =
            multi_media_adapter->active_media_stream_descriptors(media_device_b);
        const auto has_stream = [](const std::vector<axent::MediaStreamDescriptor>& descriptors,
                                   const std::string& device_id,
                                   axent::MediaKind kind,
                                   std::uint32_t stream_id) {
            return std::any_of(
                descriptors.begin(), descriptors.end(),
                [&](const axent::MediaStreamDescriptor& descriptor) {
                    return descriptor.device_id == device_id &&
                        descriptor.kind == kind &&
                        descriptor.key.stream_id == stream_id;
                });
        };
        require(media_descriptors_a.size() == 2 &&
                    has_stream(media_descriptors_a, media_device_a,
                               axent::MediaKind::Video, 1) &&
                    has_stream(media_descriptors_a, media_device_a,
                               axent::MediaKind::Audio, 2),
                "first device should own its video/audio stream IDs");
        require(media_descriptors_b.size() == 2 &&
                    has_stream(media_descriptors_b, media_device_b,
                               axent::MediaKind::Video, 1) &&
                    has_stream(media_descriptors_b, media_device_b,
                               axent::MediaKind::Audio, 2),
                "second device should independently reuse the same numeric stream IDs");
        require(multi_media_adapter->active_media_stream_descriptors().size() == 4,
                "aggregate media descriptors should retain both device namespaces");
        const auto aggregate_media_diagnostics = multi_media_adapter->diagnostics();
        require(aggregate_media_diagnostics.active_media_streams == 4 &&
                    aggregate_media_diagnostics.active_video_stream_id == 0 &&
                    aggregate_media_diagnostics.active_audio_stream_id == 0,
                "aggregate diagnostics must not assign a device-ambiguous stream ID");

        // Direct seam injection has no runtime ingress token. Leave the
        // logical lease unbound here and exercise the legacy frame path;
        // device and stream provenance remain fully physical-device scoped.
        axent::testing::AxtpAdapterTestSeam::enqueue_stream_payload(
            *multi_media_adapter, media_device_a, 1, 101, 1001,
            {0x00, 0x00, 0x01, 0x65});
        axent::testing::AxtpAdapterTestSeam::enqueue_stream_payload(
            *multi_media_adapter, media_device_b, 1, 201, 2001,
            {0x00, 0x00, 0x01, 0x41});
        axent::testing::AxtpAdapterTestSeam::drain_media_callbacks(
            *multi_media_adapter);
        require(wait_for_frames(multi_frames, multi_frames_mutex, 2),
                "same-ID media frames from both devices should be delivered");
        {
            std::lock_guard<std::mutex> lock(multi_frames_mutex);
            const auto has_frame = [&](const std::string& device_id,
                                       std::uint64_t sequence_id) {
                return std::any_of(
                    multi_frames.begin(), multi_frames.end(),
                    [&](const axent::MediaFrame& frame) {
                        return frame.device_id == device_id &&
                            frame.session_id.empty() &&
                            frame.stream_id == 1 &&
                            frame.sequence_id == sequence_id &&
                            frame.generation == 1;
                    });
            };
            require(has_frame(media_device_a, 101),
                    "first same-ID frame should retain first-device provenance");
            require(has_frame(media_device_b, 201),
                    "second same-ID frame should retain second-device provenance");
        }

        axent::testing::AxtpAdapterTestSeam::release_session(
            *multi_media_adapter, media_device_a);
        require(multi_media_adapter->active_media_stream_descriptors(
                    media_device_a).empty() &&
                    multi_media_adapter->active_media_stream_descriptors(
                        media_device_b).size() == 2 &&
                    !multi_media_adapter->diagnostics(media_device_a).open &&
                    multi_media_adapter->diagnostics(media_device_b).open,
                "first-device media reset must preserve second-device streams and session");
        axent::testing::AxtpAdapterTestSeam::enqueue_stream_payload(
            *multi_media_adapter, media_device_b, 1, 202, 2002,
            {0x00, 0x00, 0x01, 0x41});
        axent::testing::AxtpAdapterTestSeam::drain_media_callbacks(
            *multi_media_adapter);
        require(wait_for_frames(multi_frames, multi_frames_mutex, 3),
                "second-device media should continue after first-device reset");
        {
            std::lock_guard<std::mutex> lock(multi_frames_mutex);
            require(std::any_of(
                        multi_frames.begin(), multi_frames.end(),
                        [&](const axent::MediaFrame& frame) {
                            return frame.device_id == media_device_b &&
                                frame.session_id.empty() &&
                                frame.sequence_id == 202;
                        }),
                    "post-reset media should retain the surviving device binding");
        }
    }

    // A silent reset and physical transport rebuild is device-local. A
    // healthy sibling context must neither reconnect nor inherit recovery
    // counters from the failing device.
    {
        auto isolation_recovery_config = defaults;
        isolation_recovery_config.enable_media = false;
        isolation_recovery_config.session_probe_mode =
            axent::SessionProbeMode::LegacyRpc;
        isolation_recovery_config.session_health_probe_interval_ms = 20;
        isolation_recovery_config.session_health_probe_timeout_ms = 15;
        isolation_recovery_config.session_health_failure_threshold = 1;
        isolation_recovery_config.session_recovery_backoff_initial_ms = 20;
        isolation_recovery_config.session_recovery_backoff_max_ms = 40;
        std::atomic<bool> reset_recovery_a{false};
        std::mutex isolation_factory_mutex;
        std::map<std::string, int> isolation_factory_calls;
        auto isolation_recovery_adapter =
            axent::testing::AxtpAdapterTestSeam::make(
                isolation_recovery_config,
                [&](const axent::transport::HidTransportOptions& options) {
                    int serial_call = 0;
                    {
                        std::lock_guard<std::mutex> lock(isolation_factory_mutex);
                        serial_call = ++isolation_factory_calls[options.serialNumber];
                    }
                    auto transport = std::make_unique<ScriptedAxtpTransport>();
                    if (options.serialNumber == "NA20-RECOVERY-A") {
                        transport->drop_capability_responses = serial_call == 1;
                        transport->silent_reset = &reset_recovery_a;
                    }
                    return transport;
                });
        const auto factory_calls_for = [&](const std::string& serial) {
            std::lock_guard<std::mutex> lock(isolation_factory_mutex);
            const auto found = isolation_factory_calls.find(serial);
            return found == isolation_factory_calls.end() ? 0 : found->second;
        };
        const std::string recovery_device_a =
            "hid:0581:2582:NA20-RECOVERY-A";
        const std::string recovery_device_b =
            "hid:0581:2582:NA20-RECOVERY-B";
        std::string recovery_error_a;
        std::string recovery_error_b;
        require(isolation_recovery_adapter->open_session_status(
                    recovery_device_a, recovery_error_a, false) ==
                    axent::ControlStatus::Ok,
                std::string("first recovery-isolation session should open: ") +
                    recovery_error_a);
        require(isolation_recovery_adapter->open_session_status(
                    recovery_device_b, recovery_error_b, false) ==
                    axent::ControlStatus::Ok,
                std::string("second recovery-isolation session should open: ") +
                    recovery_error_b);
        axent::testing::AxtpAdapterTestSeam::bind_media_delivery_session(
            *isolation_recovery_adapter, recovery_device_a, "lease-recovery-a");
        axent::testing::AxtpAdapterTestSeam::bind_media_delivery_session(
            *isolation_recovery_adapter, recovery_device_b, "lease-recovery-b");
        reset_recovery_a.store(true);
        require(wait_until([&]() {
            const auto diagnostics =
                isolation_recovery_adapter->diagnostics(recovery_device_a);
            return factory_calls_for("NA20-RECOVERY-A") >= 2 &&
                diagnostics.session_recoveries >= 1 &&
                diagnostics.session_health == axent::SessionHealthState::Healthy;
        }), "silent reset should rebuild only the failing device context");
        require(factory_calls_for("NA20-RECOVERY-B") == 1 &&
                    isolation_recovery_adapter->diagnostics(
                        recovery_device_b).session_recoveries == 0 &&
                    isolation_recovery_adapter->diagnostics(
                        recovery_device_b).open,
                "healthy sibling device must remain on its original transport");
        const auto healthy_sibling_call = isolation_recovery_adapter->call(
            recovery_device_b, "audio.getAlgorithmConfig", {});
        require(healthy_sibling_call.status == axent::ControlStatus::Ok &&
                    factory_calls_for("NA20-RECOVERY-B") == 1,
                "healthy sibling control should continue without reconnection");
    }

    std::mutex frames_mutex;
    std::vector<axent::MediaFrame> frames;
    std::mutex stream_events_mutex;
    std::condition_variable stream_events_cv;
    std::vector<axent::MediaStreamEvent> stream_events;
    std::mutex wire_callback_order_mutex;
    std::vector<std::string> wire_callback_order;
    std::atomic<bool> capture_wire_callback_order{false};
    bool block_next_stream_event = false;
    bool stream_event_blocked = false;
    bool unblock_stream_event = false;
    axent::AxtpAdapterConfig media_config = axent::AxtpAdapter::na20_defaults();
    ScriptedAxtpTransport* media_scripted = nullptr;
    auto media_adapter = axent::testing::AxtpAdapterTestSeam::make(
        media_config, [&](const axent::transport::HidTransportOptions&) {
        auto transport = std::make_unique<ScriptedAxtpTransport>();
        media_scripted = transport.get();
        return transport;
    });

    media_adapter->set_media_frame_callback([&](std::string device_id, axent::MediaFrame frame) {
        frame.device_id = std::move(device_id);
        const auto sequence_id = frame.sequence_id;
        {
            std::lock_guard<std::mutex> lock(frames_mutex);
            frames.push_back(std::move(frame));
        }
        if (capture_wire_callback_order.load()) {
            std::lock_guard<std::mutex> lock(wire_callback_order_mutex);
            wire_callback_order.push_back("frame:" + std::to_string(sequence_id));
        }
    });
    media_adapter->set_media_stream_event_callback(
        [&](axent::MediaStreamEvent event) {
            std::unique_lock<std::mutex> lock(stream_events_mutex);
            const auto kind = event.kind;
            const auto media_kind = event.descriptor.kind;
            stream_events.push_back(std::move(event));
            stream_events_cv.notify_all();
            if (block_next_stream_event) {
                block_next_stream_event = false;
                stream_event_blocked = true;
                stream_events_cv.notify_all();
                stream_events_cv.wait(lock, [&]() { return unblock_stream_event; });
            }
            lock.unlock();
            if (capture_wire_callback_order.load()) {
                std::lock_guard<std::mutex> order_lock(wire_callback_order_mutex);
                wire_callback_order.push_back(
                    kind == axent::MediaStreamEventKind::Closed
                        ? (media_kind == axent::MediaKind::Video
                            ? "closed:video"
                            : "closed:audio")
                        : (media_kind == axent::MediaKind::Video
                            ? "opened:video"
                            : "opened:audio"));
            }
        });

    std::string error;
    require(media_adapter->open_session("hid:0581:2582:NA20-SERIAL", error),
            "scripted adapter session should open");
    require(media_scripted != nullptr, "scripted media transport should be constructed");
    const auto media_diagnostics = media_adapter->diagnostics();
    require(media_diagnostics.active_video_stream_id == 1, "video stream id should be registered from openStream");
    require(media_diagnostics.active_audio_stream_id == 2, "audio stream id should be registered from openStream");
    require(media_diagnostics.active_media_streams == 2, "active media stream count mismatch");
    require(wait_for_stream_events(stream_events, stream_events_mutex, 2),
            "openStream should publish video and audio Opened events");
    std::mutex media_video_params_mutex;
    std::vector<axent::VideoStreamParamsState> media_video_params_updates;
    auto media_video_params_subscription = media_adapter->subscribe_video_stream_params(
        "hid:0581:2582:NA20-SERIAL",
        [&](const axent::VideoStreamParamsState& update) {
            std::lock_guard<std::mutex> lock(media_video_params_mutex);
            media_video_params_updates.push_back(update);
        });
    require(media_video_params_subscription != nullptr,
            "active media session should expose video parameter state updates");

    const auto descriptors = media_adapter->active_media_stream_descriptors();
    require(descriptors.size() == 2, "active descriptor snapshot should contain video and audio");
    require(descriptors[0].key.session_id.empty() &&
                descriptors[0].key.stream_id == 1 &&
                descriptors[0].key.generation == 1 &&
                descriptors[0].device_id == "hid:0581:2582:NA20-SERIAL" &&
                descriptors[0].kind == axent::MediaKind::Video &&
                descriptors[0].codec == axent::MediaCodec::H264 &&
                descriptors[0].source == "wireless_cast" &&
                descriptors[0].stream_profile == "media.video" &&
                descriptors[0].cursor_unit == "timestampUs",
            "video open result descriptor mismatch");
    require(descriptors[1].key.session_id.empty() &&
                descriptors[1].key.stream_id == 2 &&
                descriptors[1].key.generation == 1 &&
                descriptors[1].kind == axent::MediaKind::Audio &&
                descriptors[1].codec == axent::MediaCodec::Aac &&
                descriptors[1].transport_format == "adts" &&
                descriptors[1].sample_rate == 48000 &&
                descriptors[1].channels == 2 &&
                descriptors[1].stream_profile == "media.audio" &&
                descriptors[1].cursor_unit == "timestampUs",
            "audio open result descriptor mismatch");

    media_scripted->injectStream(1, 3, 777000, {0x00, 0x00, 0x01, 0x65});
    require(wait_for_frames(frames, frames_mutex, 1), "video stream should publish a frame");

    axent::MediaFrame received_frame;
    {
        std::lock_guard<std::mutex> lock(frames_mutex);
        require(frames.size() == 1, "stream payload should publish one media frame");
        received_frame = frames.front();
    }
    require(received_frame.device_id == "hid:0581:2582:NA20-SERIAL", "device id mismatch");
    require(received_frame.stream_id == 1, "stream id mismatch");
    require(received_frame.kind == axent::MediaKind::Video, "stream kind mismatch");
    require(received_frame.codec == axent::MediaCodec::H264, "codec mismatch");
    require(received_frame.sequence_id == 3, "sequence mismatch");
    require(received_frame.cursor == 777000, "cursor mismatch");
    require(received_frame.timestamp_us == 777000, "timestamp mismatch");
    require(received_frame.generation == 1, "video frame generation mismatch");
    require(received_frame.session_id.empty(),
            "standalone adapter frames must remain unbound without a Host media lease");
    require(axent::has_flag(received_frame.flags, axent::MediaFrameFlag::EndOfFrame),
            "end-of-frame flag missing");

    media_scripted->injectStream(2, 4, 888000, {0x11, 0x22, 0x33, 0x44});
    require(wait_for_frames(frames, frames_mutex, 2), "audio stream should publish a frame");

    axent::MediaFrame received_audio_frame;
    {
        std::lock_guard<std::mutex> lock(frames_mutex);
        require(frames.size() == 2, "audio stream should append one media frame");
        received_audio_frame = frames.back();
    }
    require(received_audio_frame.device_id == "hid:0581:2582:NA20-SERIAL", "audio device id mismatch");
    require(received_audio_frame.stream_id == 2, "audio stream id mismatch");
    require(received_audio_frame.kind == axent::MediaKind::Audio, "audio stream kind mismatch");
    require(received_audio_frame.codec == axent::MediaCodec::Aac, "audio codec mismatch");
    require(received_audio_frame.sequence_id == 4, "audio sequence mismatch");
    require(received_audio_frame.cursor == 888000, "audio cursor mismatch");
    require(received_audio_frame.timestamp_us == 888000, "audio timestamp mismatch");
    require(received_audio_frame.generation == 1, "audio frame generation mismatch");
    require(axent::has_flag(received_audio_frame.flags, axent::MediaFrameFlag::EndOfFrame),
            "audio end-of-frame flag missing");

    // A slow control response must not serialize media delivery behind the
    // RPC. The session pump continues polling, callRaw invokes progress after
    // each poll, and the independent dispatcher publishes those staged frames
    // while the control operation remains in flight.
    constexpr std::uint32_t kProgressFramePairs = 60;
    media_scripted->delay_next_keyframe_response_ms.store(2000);
    axent::ControlCallOptions slow_call_options;
    slow_call_options.timeout = std::chrono::seconds(4);
    auto slow_operation = media_adapter->call_async(
        "hid:0581:2582:NA20-SERIAL",
        "video.requestKeyFrame",
        {{"streamId", 1}, {"reason", "progress-test"}},
        slow_call_options);
    require(slow_operation != nullptr && !slow_operation->ready(),
            "delayed control request should return an in-flight operation");
    require(wait_until([&]() {
        return media_adapter->diagnostics().control_in_flight == 1;
    }), "delayed control request should enter the session-pump FIFO");
    // The submitting thread must never wait for the pump's active client RPC
    // (or its session state lock) before it receives an operation handle.
    const auto queued_submit_started = std::chrono::steady_clock::now();
    auto queued_behind_slow_call = media_adapter->call_async(
        "hid:0581:2582:NA20-SERIAL",
        "audio.getAlgorithmConfig",
        {},
        slow_call_options);
    const auto queued_submit_elapsed =
        std::chrono::steady_clock::now() - queued_submit_started;
    require(queued_behind_slow_call != nullptr && !queued_behind_slow_call->ready() &&
                queued_submit_elapsed < std::chrono::milliseconds(100),
            "call_async must enqueue promptly while a two-second RPC is in flight");
    queued_behind_slow_call->cancel();
    require(queued_behind_slow_call->ready(),
            "queued control cancellation must complete immediately");

    std::thread media_during_control([&]() {
        for (std::uint32_t index = 0; index < kProgressFramePairs; ++index) {
            const auto cursor = 1'000'000ULL + static_cast<std::uint64_t>(index) * 33'333ULL;
            if (index == 10) {
                // A streamable source-state event requires deferred pump work,
                // but must not turn progress into a global media gate.
                media_scripted->injectEvent(
                    axtp::EventId::VideoStreamSourceStateChanged,
                    "video.streamSourceStateChanged",
                    R"({"source":"wireless_cast","state":"receiving","reason":"progress-test"})");
            }
            media_scripted->injectStream(
                1, 1000U + index, cursor, {0x00, 0x00, 0x01, 0x41});
            media_scripted->injectStream(
                2, 2000U + index, cursor, {0x11, 0x22, 0x33, 0x44});
            std::this_thread::sleep_for(std::chrono::milliseconds(33));
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(900));
    {
        std::lock_guard<std::mutex> lock(frames_mutex);
        require(frames.size() >= 32,
                "media callbacks must keep growing during a delayed control RPC");
    }
    {
        const auto progress_diagnostics = media_adapter->diagnostics();
        require(!slow_operation->ready() &&
                    progress_diagnostics.last_media_source_event_state == "receiving" &&
                    progress_diagnostics.last_media_source_event_reason == "progress-test",
                "streamable source events must reconcile before the slow RPC completes");
    }
    const auto slow_result = slow_operation->wait_for(std::chrono::seconds(4));
    media_during_control.join();
    require(slow_result.has_value() && slow_result->status == axent::ControlStatus::Ok,
            "delayed control request should complete successfully");
    require(wait_for_frames(
                frames,
                frames_mutex,
                2U + static_cast<std::size_t>(kProgressFramePairs) * 2U),
            "all A/V frames should be dispatched during the delayed control RPC");
    {
        std::lock_guard<std::mutex> lock(frames_mutex);
        std::uint32_t expected_video = 1000;
        std::uint32_t expected_audio = 2000;
        for (std::size_t index = 2; index < frames.size(); ++index) {
            const auto& frame = frames[index];
            if (frame.kind == axent::MediaKind::Video) {
                require(frame.sequence_id == expected_video++,
                        "video media dispatch must remain FIFO during control RPC");
            } else if (frame.kind == axent::MediaKind::Audio) {
                require(frame.sequence_id == expected_audio++,
                        "audio media dispatch must remain FIFO during control RPC");
            }
        }
        require(expected_video == 1000U + kProgressFramePairs &&
                    expected_audio == 2000U + kProgressFramePairs,
                "delayed control RPC should dispatch every injected A/V frame");
        // The remainder of this characterization test uses exact historical
        // frame counts. Retain its two baseline samples after the isolated
        // progress assertion.
        frames.erase(frames.begin() + 2, frames.end());
    }
    require(media_adapter->diagnostics().media_frames_dispatched_during_control_call >=
                static_cast<std::uint64_t>(kProgressFramePairs),
            "diagnostics should record media dispatch while control is in flight");

    media_scripted->injectEvent(
        axtp::EventId::VideoStreamSourceStateChanged,
        "video.streamSourceStateChanged",
        R"({"source":"wireless_cast","state":"paused","reason":"diagnostic_only","activeStreamId":1})");
    require(wait_until([&]() {
        const auto current = media_adapter->diagnostics();
        return current.last_media_source_event_state == "paused" &&
            current.last_media_source_event_reason == "diagnostic_only";
    }), "unknown video source state should be parsed for diagnostics");
    {
        const auto unknown_state_diagnostics = media_adapter->diagnostics();
        require(unknown_state_diagnostics.last_media_source_event_id == 0x0807,
                "video source event id diagnostic mismatch");
        require(unknown_state_diagnostics.last_media_source_event_name ==
                    "video.streamSourceStateChanged",
                "video source event name diagnostic mismatch");
        require(unknown_state_diagnostics.last_media_source_event_source == "wireless_cast" &&
                    unknown_state_diagnostics.last_media_source_event_reason == "diagnostic_only" &&
                    unknown_state_diagnostics.last_media_source_event_has_active_stream_id &&
                    unknown_state_diagnostics.last_media_source_event_active_stream_id == 1,
                "video source event payload diagnostics mismatch");
        require(unknown_state_diagnostics.active_media_streams == 2,
                "unknown source state must not change active descriptors");
    }

    media_scripted->injectEvent(
        axtp::EventId::VideoStreamSourceStateChanged,
        "video.streamSourceStateChanged",
        R"({"source":"wireless_cast","state":"stopped","activeStreamId":99})");
    require(wait_until([&]() {
        const auto current = media_adapter->diagnostics();
        return current.last_media_source_event_active_stream_id == 99;
    }), "stale activeStreamId event should be visible in diagnostics");
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    {
        std::lock_guard<std::mutex> lock(stream_events_mutex);
        require(stream_events.size() == 2,
                "stale activeStreamId terminal event must not close a newer active descriptor");
    }

    media_scripted->injectStreamThenEvent(
        1,
        5,
        999000,
        {0x00, 0x00, 0x01, 0x41},
        axtp::EventId::VideoStreamSourceStateChanged,
        "video.streamSourceStateChanged",
        R"({"source":"wireless_cast","state":"stopped","reason":"sender_stopped","activeStreamId":1})");
    require(wait_for_stream_events(stream_events, stream_events_mutex, 3),
            "video stopped source event should publish Closed");
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    {
        std::lock_guard<std::mutex> lock(frames_mutex);
        require(frames.size() == 2,
                "queued configured-stream frame must be dropped after terminal source event");
    }
    {
        std::lock_guard<std::mutex> lock(stream_events_mutex);
        require(stream_events.size() == 3 &&
                    stream_events.back().kind == axent::MediaStreamEventKind::Closed &&
                    stream_events.back().descriptor.kind == axent::MediaKind::Video &&
                    stream_events.back().descriptor.key.stream_id == 1 &&
                    stream_events.back().descriptor.key.generation == 1,
                "video terminal event must close only generation 1 video descriptor");
    }
    {
        const auto after_video_stop = media_adapter->active_media_stream_descriptors();
        require(after_video_stop.size() == 1 &&
                    after_video_stop.front().kind == axent::MediaKind::Audio &&
                    after_video_stop.front().key.generation == 1,
                "video terminal event must leave audio descriptor active");
    }
    require(wait_until([&]() {
        const auto state = media_adapter->video_stream_params_state(
            "hid:0581:2582:NA20-SERIAL");
        std::lock_guard<std::mutex> lock(media_video_params_mutex);
        return state.state == axent::VideoStreamParamsStateKind::Idle &&
            state.phase == axent::VideoStreamParamsPhase::Idle &&
            !state.active_stream_id.has_value() &&
            !state.effective_frame_rate.has_value() &&
            !media_video_params_updates.empty() &&
            media_video_params_updates.back().phase == axent::VideoStreamParamsPhase::Idle;
    }), "video terminal event should publish an idle source-video state");
    require(media_adapter->diagnostics().last_event ==
                "media-source-event name=video.streamSourceStateChanged id=0x0807 "
                "source=wireless_cast state=stopped reason=sender_stopped activeStreamId=1",
            "source event should remain observable as a complete stable diagnostic summary");

    media_scripted->injectEvent(
        axtp::EventId::VideoStreamSourceStateChanged,
        "video.streamSourceStateChanged",
        R"({"state":"stopped","activeStreamId":1})");
    require(wait_until([&]() {
        return media_adapter->diagnostics().last_media_source_event_reason.empty();
    }), "duplicate terminal event should still update diagnostics");
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    {
        std::lock_guard<std::mutex> lock(stream_events_mutex);
        require(stream_events.size() == 3,
                "duplicate terminal event must not publish a second Closed");
    }

    media_scripted->injectEvent(
        axtp::EventId::VideoStreamSourceStateChanged,
        "video.streamSourceStateChanged",
        R"({"source":"wireless_cast","state":"receiving"})");
    require(wait_for_stream_events(stream_events, stream_events_mutex, 6),
            "video receiving source event should reopen paired media streams");
    require(media_adapter->diagnostics().last_event ==
                "media-source-event name=video.streamSourceStateChanged id=0x0807 "
                "source=wireless_cast state=receiving reason=<absent> activeStreamId=<absent>",
            "event-driven reopen must not overwrite the receiving event diagnostic summary");
    axent::transport::HidReportTrace source_event_read_timeout;
    source_event_read_timeout.kind = axent::transport::HidReportTraceKind::ReadTimeout;
    axent::testing::AxtpAdapterTestSeam::record_hid_trace(
        *media_adapter, source_event_read_timeout);
    {
        const auto after_timeout = media_adapter->diagnostics();
        require(after_timeout.last_event == "read-timeout",
                "generic last_event should preserve last-writer-wins trace ordering");
        require(after_timeout.last_media_source_event_name ==
                    "video.streamSourceStateChanged" &&
                    after_timeout.last_media_source_event_state == "receiving",
                "generic traces must not erase structured source-event diagnostics");
    }
    {
        const auto after_video_recovery = media_adapter->active_media_stream_descriptors();
        require(after_video_recovery.size() == 2 &&
                    after_video_recovery[0].kind == axent::MediaKind::Video &&
                    after_video_recovery[0].key.stream_id == 1 &&
                    after_video_recovery[0].key.generation == 2 &&
                    after_video_recovery[1].kind == axent::MediaKind::Audio &&
                    after_video_recovery[1].key.generation == 2,
                "video recovery must increment both paired same-ID generations");
        require(media_scripted->video_open_requests.load() == 2 &&
                    media_scripted->audio_open_requests.load() == 2,
                "video recovery must reopen both paired media legs");
        require(media_scripted->video_close_requests.load() == 1 &&
                    media_scripted->audio_close_requests.load() == 1,
                "source recovery must close both receiver-pull legs before reopening video");
        std::lock_guard<std::mutex> requests_lock(media_scripted->requests_mutex);
        require(media_scripted->video_close_params.front().at("reason") == "sourceRecovery" &&
                    media_scripted->audio_close_params.front().at("reason") == "sourceRecovery" &&
                    media_scripted->media_request_order.size() >= 5 &&
                    media_scripted->media_request_order[2] == "video.close" &&
                    media_scripted->media_request_order[3] == "audio.close" &&
                    media_scripted->media_request_order[4] == "video.open",
                "source recovery close/open ordering mismatch");
    }
    require(wait_until([&]() {
        const auto state = media_adapter->video_stream_params_state(
            "hid:0581:2582:NA20-SERIAL");
        std::lock_guard<std::mutex> lock(media_video_params_mutex);
        return state.phase == axent::VideoStreamParamsPhase::Streaming &&
            state.active_stream_id == 1U &&
            !media_video_params_updates.empty() &&
            media_video_params_updates.back().phase ==
                axent::VideoStreamParamsPhase::Streaming;
    }), "same-ID video recovery should publish a streaming source-video state");
    media_scripted->injectStream(1, 6, 1000000, {0x00, 0x00, 0x01, 0x65});
    require(wait_for_frames(frames, frames_mutex, 3),
            "reopened video generation should deliver new frames");
    {
        std::lock_guard<std::mutex> lock(frames_mutex);
        require(frames.back().generation == 2 && frames.back().sequence_id == 6,
                "reopened video frame must bind to generation 2");
    }

    media_scripted->injectEvent(
        axtp::EventId::VideoStreamSourceStateChanged,
        "",
        R"({"source":"wireless_cast","state":"paused","reason":"empty_name"})");
    require(wait_until([&]() {
        const auto current = media_adapter->diagnostics();
        return current.last_media_source_event_reason == "empty_name";
    }), "empty source event name should be observed");
    require(media_adapter->diagnostics().last_media_source_event_name ==
                "video.streamSourceStateChanged",
            "empty source event name should use the generated canonical name");

    const auto require_ignored_video_terminal =
        [&](const std::string& reason,
            const std::string& body,
            bool expected_has_active_stream_id,
            std::uint32_t expected_active_stream_id) {
            std::size_t event_count_before = 0;
            {
                std::lock_guard<std::mutex> lock(stream_events_mutex);
                event_count_before = stream_events.size();
            }
            media_scripted->injectEvent(
                axtp::EventId::VideoStreamSourceStateChanged,
                "video.streamSourceStateChanged",
                body);
            require(wait_until([&]() {
                return media_adapter->diagnostics().last_media_source_event_reason == reason;
            }), std::string("source event was not dispatched: ") + reason);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            const auto current = media_adapter->diagnostics();
            require(current.last_media_source_event_has_active_stream_id ==
                        expected_has_active_stream_id &&
                        current.last_media_source_event_active_stream_id ==
                            expected_active_stream_id,
                    std::string("activeStreamId parsing mismatch: ") + reason);
            require(media_adapter->active_media_stream_descriptors().size() == 2,
                    std::string("ignored source event changed active descriptors: ") +
                        reason);
            std::lock_guard<std::mutex> lock(stream_events_mutex);
            require(stream_events.size() == event_count_before,
                    std::string("ignored source event published lifecycle: ") + reason);
        };

    require_ignored_video_terminal(
        "foreignAudioId",
        R"({"source":"wireless_cast","state":"stopped","reason":"foreignAudioId","activeStreamId":2})",
        true,
        2);
    require_ignored_video_terminal(
        "uint32MaxId",
        R"({"source":"wireless_cast","state":"stopped","reason":"uint32MaxId","activeStreamId":4294967295})",
        true,
        std::numeric_limits<std::uint32_t>::max());
    require_ignored_video_terminal(
        "negativeId",
        R"({"source":"wireless_cast","state":"stopped","reason":"negativeId","activeStreamId":-1})",
        false,
        0);
    require_ignored_video_terminal(
        "overflowId",
        R"({"source":"wireless_cast","state":"stopped","reason":"overflowId","activeStreamId":4294967296})",
        false,
        0);
    require_ignored_video_terminal(
        "stringId",
        R"({"source":"wireless_cast","state":"stopped","reason":"stringId","activeStreamId":"1"})",
        false,
        0);
    require_ignored_video_terminal(
        "floatingId",
        R"({"source":"wireless_cast","state":"stopped","reason":"floatingId","activeStreamId":1.0})",
        false,
        0);
    {
        std::size_t event_count_before = 0;
        {
            std::lock_guard<std::mutex> lock(stream_events_mutex);
            event_count_before = stream_events.size();
        }
        media_scripted->injectEvent(
            axtp::EventId::VideoStreamSourceStateChanged,
            "video.streamSourceStateChanged",
            "{");
        require(wait_until([&]() {
            const auto current = media_adapter->diagnostics();
            return current.last_media_source_event_state.empty() &&
                current.last_media_source_event_reason.empty();
        }), "malformed source event was not dispatched");
        require(media_adapter->active_media_stream_descriptors().size() == 2,
                "malformed source event must not change active descriptors");
        std::lock_guard<std::mutex> lock(stream_events_mutex);
        require(stream_events.size() == event_count_before,
                "malformed source event must not publish lifecycle");
    }

    media_scripted->injectEvent(
        axtp::EventId::VideoStreamSourceStateChanged,
        "video.streamSourceStateChanged",
        R"({"state":"stopped","activeStreamId":0})");
    require(wait_for_stream_events(stream_events, stream_events_mutex, 7),
            "zero activeStreamId should use the valid missing-source fallback");
    {
        const auto after_missing_source_stop =
            media_adapter->active_media_stream_descriptors();
        require(after_missing_source_stop.size() == 1 &&
                    after_missing_source_stop.front().kind == axent::MediaKind::Audio,
                "missing-source terminal event should close the configured video kind");
        const auto zero_id_diagnostics = media_adapter->diagnostics();
        require(zero_id_diagnostics.last_media_source_event_has_active_stream_id &&
                    zero_id_diagnostics.last_media_source_event_active_stream_id == 0,
                "activeStreamId zero should remain a valid structured value");
    }

    // AXTP receiver-pull requires a successful replacement openStream before
    // the source can emit frames for the new generation. A frame between stop
    // and receiving is therefore stale; a post-open frame belongs to the new
    // generation.
    std::size_t frames_before_stale_video = 0;
    {
        std::lock_guard<std::mutex> lock(frames_mutex);
        frames_before_stale_video = frames.size();
    }
    media_scripted->injectStream(1, 7, 1100000, {0x00, 0x00, 0x01, 0x41});
    const auto stale_frame_fence = media_adapter->call(
        "hid:0581:2582:NA20-SERIAL", "audio.getAlgorithmConfig", {});
    require(stale_frame_fence.status == axent::ControlStatus::Ok,
            "public call should fence stale-frame dispatch");
    {
        std::lock_guard<std::mutex> lock(frames_mutex);
        require(frames.size() == frames_before_stale_video,
                "frame after stop and before recovery must be dropped");
    }

    // Capabilities are cached for the physical session.  Exercise the
    // recovery retry at the openStream boundary instead of forcing a second
    // capabilities RPC on every source event.
    const auto video_capabilities_before_recovery =
        media_scripted->video_capability_requests.load();
    media_scripted->fail_video_open_count.store(1);
    const auto video_recovery_started = std::chrono::steady_clock::now();
    media_scripted->injectEvent(
        axtp::EventId::VideoStreamSourceStateChanged,
        "video.streamSourceStateChanged",
        R"({"source":"wireless_cast","state":"receiving"})");
    require(wait_until([&]() {
        return media_scripted->video_open_requests.load() >= 3 &&
            media_scripted->audio_open_requests.load() >= 3;
    }), "event-driven video recovery should make its first open attempt");
    require(media_scripted->video_capability_requests.load() ==
                video_capabilities_before_recovery &&
                media_scripted->video_open_requests.load() == 3 &&
                media_scripted->audio_open_requests.load() == 3,
            "failed video recovery must still reopen the healthy paired audio leg");
    const auto video_capabilities_after_first_failure =
        media_scripted->video_capability_requests.load();
    media_scripted->injectEvent(
        axtp::EventId::VideoStreamSourceStateChanged,
        "video.streamSourceStateChanged",
        R"({"source":"wireless_cast","state":"receiving","reason":"duplicate"})");
    require(wait_until([&]() {
        return media_adapter->diagnostics().last_media_source_event_reason == "duplicate";
    }), "duplicate receiving event should remain observable during recovery backoff");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    require(media_scripted->video_capability_requests.load() ==
                video_capabilities_after_first_failure,
            "duplicate source event must not bypass the per-kind retry interval");
    require(wait_for_stream_events(
                stream_events, stream_events_mutex, 10, std::chrono::seconds(3)),
            "failed video recovery should retry and publish paired Opened events");
    const auto video_recovery_elapsed =
        std::chrono::steady_clock::now() - video_recovery_started;
    require(video_recovery_elapsed >= std::chrono::milliseconds(900),
            "per-kind source recovery retry must respect the one-second interval");
    require(media_scripted->video_open_requests.load() == 4 &&
                media_scripted->audio_open_requests.load() == 3,
            "video recovery retry must not reopen audio a second time");
    const auto video_capabilities_after_recovery =
        media_scripted->video_capability_requests.load();
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    require(media_scripted->video_capability_requests.load() ==
                 video_capabilities_after_recovery &&
                 media_scripted->video_open_requests.load() == 4,
            "successful per-kind recovery must clear its pending retry");
    {
        const auto after_video_retry = media_adapter->active_media_stream_descriptors();
        require(after_video_retry.size() == 2 &&
                    after_video_retry[0].kind == axent::MediaKind::Video &&
                    after_video_retry[0].key.generation == 3 &&
                    after_video_retry[1].kind == axent::MediaKind::Audio &&
                    after_video_retry[1].key.generation == 3,
                "paired recovery retry should preserve the recovered audio generation");
    }
    media_scripted->injectStream(1, 8, 1200000, {0x00, 0x00, 0x01, 0x65});
    require(wait_for_frames(
                frames, frames_mutex, frames_before_stale_video + 1),
            "new frame after receiving and reopen should be delivered");
    {
        std::lock_guard<std::mutex> lock(frames_mutex);
        require(frames.back().sequence_id == 8 && frames.back().generation == 3,
                "post-recovery frame must bind to video generation 3");
    }

    media_scripted->injectEvent(
        axtp::EventId::AudioStreamSourceStateChanged,
        "audio.streamSourceStateChanged",
        R"({"source":"wireless_cast_audio","state":"receiving","reason":"source_disconnected","activeStreamId":2})");
    require(wait_for_stream_events(stream_events, stream_events_mutex, 11),
            "source_disconnected reason should close audio even with receiving state");
    {
        const auto after_audio_stop = media_adapter->active_media_stream_descriptors();
        require(after_audio_stop.size() == 1 &&
                    after_audio_stop.front().kind == axent::MediaKind::Video &&
                    after_audio_stop.front().key.generation == 3,
                "audio terminal event must leave reopened video descriptor unchanged");
    }
    media_scripted->injectEvent(
        axtp::EventId::AudioStreamSourceStateChanged,
        "audio.streamSourceStateChanged",
        R"({"source":"wireless_cast_audio","state":"available"})");
    require(wait_for_stream_events(stream_events, stream_events_mutex, 12),
            "audio available source event should independently reopen audio");
    {
        const auto after_audio_recovery = media_adapter->active_media_stream_descriptors();
        require(after_audio_recovery.size() == 2 &&
                    after_audio_recovery[0].key.generation == 3 &&
                    after_audio_recovery[1].kind == axent::MediaKind::Audio &&
                    after_audio_recovery[1].key.stream_id == 2 &&
                    after_audio_recovery[1].key.generation == 4,
                "audio recovery must increment only the same-ID audio generation");
        require(media_scripted->video_open_requests.load() == 4 &&
                media_scripted->audio_open_requests.load() == 4,
            "audio recovery must not reopen video");
    }

    axent::testing::AxtpAdapterTestSeam::enqueue_stream_payload(
        *media_adapter,
        "hid:0581:2582:NA20-SERIAL",
        1,
        5,
        999000,
        {0x00, 0x00, 0x01, 0x41});
    {
        std::lock_guard<std::mutex> lock(stream_events_mutex);
        block_next_stream_event = true;
        stream_event_blocked = false;
        unblock_stream_event = false;
    }
    // reopen_media_streams() wakes the dispatcher itself. Arm the callback
    // gate first so the test does not race that notification and accidentally
    // observe a frame only after the lifecycle event was already delivered.
    axent::testing::AxtpAdapterTestSeam::reopen_media_streams(
        *media_adapter, "hid:0581:2582:NA20-SERIAL");
    std::thread first_drain([&]() {
        axent::testing::AxtpAdapterTestSeam::drain_media_callbacks(*media_adapter);
    });
    bool first_event_blocked = false;
    {
        std::unique_lock<std::mutex> lock(stream_events_mutex);
        first_event_blocked = stream_events_cv.wait_for(
            lock, std::chrono::seconds(1), [&]() { return stream_event_blocked; });
    }
    axent::testing::AxtpAdapterTestSeam::enqueue_stream_payload(
        *media_adapter,
        "hid:0581:2582:NA20-SERIAL",
        1,
        6,
        1000000,
        {0x00, 0x00, 0x01, 0x65});
    std::atomic<bool> second_drain_finished{false};
    std::thread second_drain([&]() {
        axent::testing::AxtpAdapterTestSeam::drain_media_callbacks(*media_adapter);
        second_drain_finished.store(true);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    const bool second_drain_waited_for_lifecycle = !second_drain_finished.load();
    std::size_t frames_before_opened_unblocked = 0;
    {
        std::lock_guard<std::mutex> lock(frames_mutex);
        frames_before_opened_unblocked = frames.size();
    }
    {
        std::lock_guard<std::mutex> lock(stream_events_mutex);
        unblock_stream_event = true;
    }
    stream_events_cv.notify_all();
    first_drain.join();
    second_drain.join();
    require(first_event_blocked && second_drain_waited_for_lifecycle &&
                frames_before_opened_unblocked == 4,
            "concurrent media drains must not deliver a frame ahead of lifecycle events "
            "(firstBlocked=" + std::to_string(first_event_blocked) +
            ", secondWaited=" + std::to_string(second_drain_waited_for_lifecycle) +
            ", framesBeforeUnblock=" +
            std::to_string(frames_before_opened_unblocked) + ")");

    require(wait_for_stream_events(stream_events, stream_events_mutex, 16),
            "same-ID reopen should publish Closed/Open pairs for video and audio");
    const auto reopened_descriptors = media_adapter->active_media_stream_descriptors();
    require(reopened_descriptors.size() == 2 &&
                reopened_descriptors[0].key.generation == 4 &&
                reopened_descriptors[1].key.generation == 5,
            "same-ID reopen should increment each physical generation");
    require(!axent::testing::AxtpAdapterTestSeam::is_current_media_frame(
                *media_adapter, received_frame),
            "old generation frame must become stale after same-ID reopen");

    {
        std::lock_guard<std::mutex> lock(frames_mutex);
        require(frames.size() == 5,
                "queued old generation must be dropped while the new generation frame is delivered");
        require(frames.back().sequence_id == 6 && frames.back().generation == 4,
                "reopened stream frame should carry the incremented generation");
        require(axent::testing::AxtpAdapterTestSeam::is_current_media_frame(
                    *media_adapter, frames.back()),
                "new generation frame should remain current");
    }

    std::size_t frames_before_public_call = 0;
    {
        std::lock_guard<std::mutex> lock(frames_mutex);
        frames_before_public_call = frames.size();
    }
    {
        std::lock_guard<std::mutex> lock(wire_callback_order_mutex);
        wire_callback_order.clear();
    }
    capture_wire_callback_order.store(true);
    media_scripted->queue_terminal_frame_before_next_response.store(true);
    const auto terminal_call = media_adapter->call(
        "hid:0581:2582:NA20-SERIAL", "audio.getAlgorithmConfig", {});
    {
        std::lock_guard<std::mutex> lock(wire_callback_order_mutex);
        wire_callback_order.push_back("response");
    }
    capture_wire_callback_order.store(false);
    require(terminal_call.status == axent::ControlStatus::Ok &&
                terminal_call.body.value("ok", false),
            "public call should receive the scripted response after terminal/frame dispatch");
    {
        std::lock_guard<std::mutex> lock(wire_callback_order_mutex);
        require(wire_callback_order.size() == 2 &&
                    wire_callback_order[0] == "closed:video" &&
                    wire_callback_order[1] == "response",
                "wire terminal must publish Closed before public call returns its response");
    }
    {
        std::lock_guard<std::mutex> lock(frames_mutex);
        require(frames.size() == frames_before_public_call,
                "wire frame queued after terminal must be dropped before public call returns");
    }
    {
        const auto after_terminal_call = media_adapter->active_media_stream_descriptors();
        require(after_terminal_call.size() == 1 &&
                    after_terminal_call.front().kind == axent::MediaKind::Audio &&
                    after_terminal_call.front().key.generation == 5,
                "public call terminal dispatch must close video without disturbing audio");
    }

    {
        auto open_terminal_config = media_config;
        open_terminal_config.enable_audio = false;
        open_terminal_config.session_health_probe_interval_ms = 60'000;
        ScriptedAxtpTransport* open_terminal_transport = nullptr;
        auto open_terminal_adapter = axent::testing::AxtpAdapterTestSeam::make(
            open_terminal_config,
            [&](const axent::transport::HidTransportOptions&) {
                auto transport = std::make_unique<ScriptedAxtpTransport>();
                transport->hold_next_video_open_response.store(true);
                open_terminal_transport = transport.get();
                return transport;
            });
        std::mutex open_terminal_events_mutex;
        std::vector<axent::MediaStreamEvent> open_terminal_events;
        open_terminal_adapter->set_media_stream_event_callback(
            [&](axent::MediaStreamEvent event) {
                std::lock_guard<std::mutex> lock(open_terminal_events_mutex);
                open_terminal_events.push_back(std::move(event));
            });

        std::string open_terminal_error;
        require(open_terminal_adapter->open_session_status(
                    "hid:0581:2582:OPEN-TERMINAL",
                    open_terminal_error,
                    false) == axent::ControlStatus::Ok,
                "open-terminal fixture should establish a physical session");
        require(open_terminal_transport != nullptr && wait_until([&]() {
                    return open_terminal_transport->video_open_response_held.load();
                }),
                "video open response should be held before descriptor publication");

        open_terminal_transport->injectEvent(
            axtp::EventId::VideoStreamSourceStateChanged,
            "video.streamSourceStateChanged",
            R"({"source":"wireless_cast","state":"receiving","reason":"older-receiving"})");
        require(wait_until([&]() {
                    return open_terminal_adapter->diagnostics().
                        last_media_source_event_reason == "older-receiving";
                }),
                "receiving should be deferred while openStream owns the client");
        open_terminal_transport->injectEvent(
            axtp::EventId::VideoStreamSourceStateChanged,
            "video.streamSourceStateChanged",
            R"({"source":"wireless_cast","state":"receiving","reason":"source_disconnected","activeStreamId":1})");
        require(wait_until([&]() {
                    return open_terminal_adapter->diagnostics().
                        last_media_source_event_reason == "source_disconnected";
                }),
                "reason-terminal event should be observed while openStream is pending");
        open_terminal_transport->releaseHeldVideoOpenResponse();
        require(wait_until([&]() {
                    return open_terminal_transport->video_close_requests.load() == 1;
                }),
                "a stale successful open response should schedule one orphan close");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        {
            std::lock_guard<std::mutex> lock(open_terminal_events_mutex);
            require(open_terminal_events.empty(),
                    "terminal during open must not publish a stale or deferred Opened lifecycle");
        }
        const auto open_terminal_diagnostics = open_terminal_adapter->diagnostics();
        require(open_terminal_adapter->active_media_stream_descriptors().empty() &&
                    open_terminal_diagnostics.active_video_stream_id == 0 &&
                    open_terminal_transport->video_open_requests.load() == 1 &&
                    open_terminal_transport->video_close_requests.load() == 1,
                "matched terminal order must suppress older receiving and close the orphan once");
    }

    {
        auto per_kind_fence_config = media_config;
        per_kind_fence_config.session_health_probe_interval_ms = 60'000;
        ScriptedAxtpTransport* per_kind_fence_transport = nullptr;
        auto per_kind_fence_adapter = axent::testing::AxtpAdapterTestSeam::make(
            per_kind_fence_config,
            [&](const axent::transport::HidTransportOptions&) {
                auto transport = std::make_unique<ScriptedAxtpTransport>();
                per_kind_fence_transport = transport.get();
                return transport;
            });
        std::mutex per_kind_frames_mutex;
        std::vector<axent::MediaFrame> per_kind_frames;
        per_kind_fence_adapter->set_media_frame_callback(
            [&](const std::string&, axent::MediaFrame frame) {
                std::lock_guard<std::mutex> lock(per_kind_frames_mutex);
                per_kind_frames.push_back(std::move(frame));
            });
        std::string per_kind_fence_error;
        require(per_kind_fence_adapter->open_session(
                    "hid:0581:2582:PER-KIND-FENCE", per_kind_fence_error) &&
                    per_kind_fence_transport != nullptr &&
                    wait_until([&]() {
                        return per_kind_fence_adapter->
                            active_media_stream_descriptors().size() == 2;
                    }),
                "per-kind fence fixture should open both media legs");

        per_kind_fence_transport->hold_next_video_open_response.store(true);
        std::atomic<bool> per_kind_reopen_finished{false};
        std::thread per_kind_reopen([&]() {
            axent::testing::AxtpAdapterTestSeam::reopen_media_streams(
                *per_kind_fence_adapter,
                "hid:0581:2582:PER-KIND-FENCE");
            per_kind_reopen_finished.store(true);
        });
        const bool per_kind_video_open_held = wait_until([&]() {
            return per_kind_fence_transport->video_open_response_held.load();
        });
        if (per_kind_video_open_held) {
            for (std::uint32_t index = 0; index < 3; ++index) {
                per_kind_fence_transport->injectStream(
                    2,
                    5000U + index,
                    5'000'000ULL + static_cast<std::uint64_t>(index) * 21'333ULL,
                    {0x11, 0x22, 0x33, 0x44});
            }
        }
        const bool audio_delivered_before_video_open =
            per_kind_video_open_held &&
            wait_for_frames(per_kind_frames, per_kind_frames_mutex, 3) &&
            !per_kind_reopen_finished.load();
        per_kind_fence_transport->releaseHeldVideoOpenResponse();
        per_kind_reopen.join();
        require(per_kind_video_open_held,
                "replacement video open response should be held by the fixture");
        require(audio_delivered_before_video_open,
                "active audio must dispatch while only video lifecycle is fenced");
        {
            std::lock_guard<std::mutex> lock(per_kind_frames_mutex);
            require(per_kind_frames.size() == 3,
                    "per-kind fence should dispatch each held-open audio frame once");
            for (std::size_t index = 0; index < per_kind_frames.size(); ++index) {
                require(per_kind_frames[index].kind == axent::MediaKind::Audio &&
                            per_kind_frames[index].sequence_id == 5000U + index,
                        "audio dispatch must remain FIFO during slow video reopen");
            }
        }
    }

    {
        auto terminal_progress_config = media_config;
        terminal_progress_config.session_health_probe_interval_ms = 60'000;
        ScriptedAxtpTransport* terminal_progress_transport = nullptr;
        auto terminal_progress_adapter = axent::testing::AxtpAdapterTestSeam::make(
            terminal_progress_config,
            [&](const axent::transport::HidTransportOptions&) {
                auto transport = std::make_unique<ScriptedAxtpTransport>();
                terminal_progress_transport = transport.get();
                return transport;
            });
        std::mutex terminal_progress_events_mutex;
        std::vector<axent::MediaStreamEvent> terminal_progress_events;
        std::mutex terminal_progress_frames_mutex;
        std::vector<axent::MediaFrame> terminal_progress_frames;
        terminal_progress_adapter->set_media_stream_event_callback(
            [&](axent::MediaStreamEvent event) {
                std::lock_guard<std::mutex> lock(terminal_progress_events_mutex);
                terminal_progress_events.push_back(std::move(event));
            });
        terminal_progress_adapter->set_media_frame_callback(
            [&](const std::string&, axent::MediaFrame frame) {
                std::lock_guard<std::mutex> lock(terminal_progress_frames_mutex);
                terminal_progress_frames.push_back(std::move(frame));
            });
        std::string terminal_progress_error;
        require(terminal_progress_adapter->open_session(
                    "hid:0581:2582:TERMINAL-PROGRESS", terminal_progress_error) &&
                    terminal_progress_transport != nullptr &&
                    wait_for_stream_events(
                        terminal_progress_events,
                        terminal_progress_events_mutex,
                        2),
                "terminal progress fixture should open both media legs");
        {
            std::lock_guard<std::mutex> lock(terminal_progress_events_mutex);
            terminal_progress_events.clear();
        }

        terminal_progress_transport->delay_next_keyframe_response_ms.store(1500);
        axent::ControlCallOptions terminal_progress_options;
        terminal_progress_options.timeout = std::chrono::seconds(3);
        auto terminal_progress_operation = terminal_progress_adapter->call_async(
            "hid:0581:2582:TERMINAL-PROGRESS",
            "video.requestKeyFrame",
            {{"streamId", 1}, {"reason", "terminal-progress"}},
            terminal_progress_options);
        require(terminal_progress_operation != nullptr && wait_until([&]() {
                    return terminal_progress_adapter->diagnostics().control_in_flight == 1;
                }),
                "terminal progress fixture should enter a slow RPC");
        terminal_progress_transport->injectEvent(
            axtp::EventId::VideoStreamSourceStateChanged,
            "video.streamSourceStateChanged",
            R"({"source":"wireless_cast","state":"receiving","reason":"source_disconnected","activeStreamId":1})");
        const bool video_closed_during_rpc = wait_for_stream_events(
            terminal_progress_events,
            terminal_progress_events_mutex,
            1) && !terminal_progress_operation->ready();
        for (std::uint32_t index = 0; index < 3; ++index) {
            terminal_progress_transport->injectStream(
                2,
                6000U + index,
                6'000'000ULL + static_cast<std::uint64_t>(index) * 21'333ULL,
                {0x55, 0x66, 0x77, 0x88});
        }
        const bool audio_delivered_during_rpc = wait_for_frames(
            terminal_progress_frames,
            terminal_progress_frames_mutex,
            3) && !terminal_progress_operation->ready();
        const auto terminal_progress_result =
            terminal_progress_operation->wait_for(std::chrono::seconds(3));
        require(video_closed_during_rpc,
                "source_disconnected must publish video Closed before slow RPC completion");
        require(audio_delivered_during_rpc,
                "video source_disconnected must not stall the active audio leg");
        require(terminal_progress_result.has_value() &&
                    terminal_progress_result->status == axent::ControlStatus::Ok,
                "terminal progress slow RPC should still complete");
        {
            std::lock_guard<std::mutex> lock(terminal_progress_events_mutex);
            require(terminal_progress_events.size() == 1 &&
                        terminal_progress_events.front().kind ==
                            axent::MediaStreamEventKind::Closed &&
                        terminal_progress_events.front().descriptor.kind ==
                            axent::MediaKind::Video,
                    "reason-terminal progress should close only video");
        }
        {
            std::lock_guard<std::mutex> lock(terminal_progress_frames_mutex);
            require(terminal_progress_frames.size() == 3,
                    "terminal progress should dispatch each audio frame once");
            for (std::size_t index = 0;
                 index < terminal_progress_frames.size();
                 ++index) {
                require(terminal_progress_frames[index].kind == axent::MediaKind::Audio &&
                            terminal_progress_frames[index].sequence_id == 6000U + index,
                        "terminal progress audio dispatch must remain FIFO");
            }
        }
        const auto terminal_progress_descriptors =
            terminal_progress_adapter->active_media_stream_descriptors();
        require(terminal_progress_descriptors.size() == 1 &&
                    terminal_progress_descriptors.front().kind == axent::MediaKind::Audio,
                "reason-terminal progress must leave the audio descriptor active");
    }

    {
        auto ordered_source_config = media_config;
        ordered_source_config.enable_audio = false;
        ordered_source_config.session_health_probe_interval_ms = 60'000;
        ScriptedAxtpTransport* ordered_source_transport = nullptr;
        auto ordered_source_adapter = axent::testing::AxtpAdapterTestSeam::make(
            ordered_source_config,
            [&](const axent::transport::HidTransportOptions&) {
                auto transport = std::make_unique<ScriptedAxtpTransport>();
                ordered_source_transport = transport.get();
                return transport;
            });
        std::mutex ordered_source_events_mutex;
        std::vector<axent::MediaStreamEvent> ordered_source_events;
        ordered_source_adapter->set_media_stream_event_callback(
            [&](axent::MediaStreamEvent event) {
                std::lock_guard<std::mutex> lock(ordered_source_events_mutex);
                ordered_source_events.push_back(std::move(event));
            });
        std::string ordered_source_error;
        require(ordered_source_adapter->open_session(
                    "hid:0581:2582:ORDERED-SOURCE", ordered_source_error) &&
                    ordered_source_transport != nullptr &&
                    wait_for_stream_events(
                        ordered_source_events, ordered_source_events_mutex, 1),
                "ordered source-state fixture should open video generation one");
        ordered_source_transport->injectEvent(
            axtp::EventId::VideoStreamSourceStateChanged,
            "video.streamSourceStateChanged",
            R"({"source":"wireless_cast","state":"stopped","reason":"initial-stop","activeStreamId":1})");
        require(wait_for_stream_events(
                    ordered_source_events, ordered_source_events_mutex, 2),
                "ordered source-state fixture should close its initial stream");

        ordered_source_transport->delay_next_keyframe_response_ms.store(1000);
        axent::ControlCallOptions ordered_call_options;
        ordered_call_options.timeout = std::chrono::seconds(2);
        auto ordered_operation = ordered_source_adapter->call_async(
            "hid:0581:2582:ORDERED-SOURCE",
            "video.requestKeyFrame",
            {{"streamId", 1}, {"reason", "source-order"}},
            ordered_call_options);
        require(ordered_operation != nullptr && wait_until([&]() {
                    return ordered_source_adapter->diagnostics().control_in_flight == 1;
                }),
                "ordered source-state fixture should enter a slow RPC");
        ordered_source_transport->injectEvent(
            axtp::EventId::VideoStreamSourceStateChanged,
            "video.streamSourceStateChanged",
            R"({"source":"wireless_cast","state":"receiving","reason":"older-receiving"})");
        require(wait_until([&]() {
                    return ordered_source_adapter->diagnostics().
                        last_media_source_event_reason == "older-receiving";
                }),
                "streamable event should be observed during the slow RPC");
        ordered_source_transport->injectEvent(
            axtp::EventId::VideoStreamSourceStateChanged,
            "video.streamSourceStateChanged",
            R"({"source":"wireless_cast","state":"stopped","reason":"newer-terminal"})");
        require(wait_until([&]() {
                    return ordered_source_adapter->diagnostics().
                        last_media_source_event_reason == "newer-terminal";
                }),
                "newer terminal event should supersede deferred receiving");
        const auto ordered_result = ordered_operation->wait_for(std::chrono::seconds(2));
        require(ordered_result.has_value() &&
                    ordered_result->status == axent::ControlStatus::Ok,
                "ordered source-state slow RPC should complete");
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        {
            std::lock_guard<std::mutex> lock(ordered_source_events_mutex);
            require(ordered_source_events.size() == 2,
                    "receiving followed by terminal must not publish a delayed Opened");
        }
        require(ordered_source_transport->video_open_requests.load() == 1 &&
                    ordered_source_adapter->active_media_stream_descriptors().empty(),
                "stale deferred receiving must not reopen after the slow RPC");
    }

    auto kind_fallback_config = media_config;
    kind_fallback_config.video_source.clear();
    kind_fallback_config.enable_audio = false;
    ScriptedAxtpTransport* kind_fallback_scripted = nullptr;
    auto kind_fallback_adapter = axent::testing::AxtpAdapterTestSeam::make(
        kind_fallback_config, [&](const axent::transport::HidTransportOptions&) {
            auto transport = std::make_unique<ScriptedAxtpTransport>();
            kind_fallback_scripted = transport.get();
            return transport;
        });
    std::mutex kind_fallback_events_mutex;
    std::vector<axent::MediaStreamEvent> kind_fallback_events;
    kind_fallback_adapter->set_media_stream_event_callback(
        [&](axent::MediaStreamEvent event) {
            std::lock_guard<std::mutex> lock(kind_fallback_events_mutex);
            kind_fallback_events.push_back(std::move(event));
        });
    require(kind_fallback_adapter->open_session(
                "hid:0581:2582:NA20-SERIAL", error),
            "kind-fallback adapter session should open");
    require(kind_fallback_scripted != nullptr &&
                wait_for_stream_events(
                    kind_fallback_events, kind_fallback_events_mutex, 1),
            "kind-fallback adapter should publish video Opened");
    kind_fallback_scripted->injectEvent(
        axtp::EventId::VideoStreamSourceStateChanged,
        "video.streamSourceStateChanged",
        R"({"source":"foreign_source","state":"stopped"})");
    require(wait_until([&]() {
        return kind_fallback_adapter->diagnostics().last_media_source_event_source ==
            "foreign_source";
    }), "mismatched source event should be observable");
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    require(kind_fallback_adapter->active_media_stream_descriptors().size() == 1,
            "explicit mismatched source must not close the video descriptor");
    kind_fallback_scripted->injectEvent(
        axtp::EventId::VideoStreamSourceStateChanged,
        "",
        R"({"state":"stopped"})");
    require(wait_for_stream_events(
                kind_fallback_events, kind_fallback_events_mutex, 2),
            "source-less event should fall back to kind when config is also source-less");
    require(kind_fallback_adapter->active_media_stream_descriptors().empty(),
            "source-less kind fallback should close the active video descriptor");
    require(kind_fallback_adapter->diagnostics().last_media_source_event_name ==
                "video.streamSourceStateChanged",
            "source-less event with empty name should retain the generated canonical name");
    // Capabilities are cached for the physical session, so exercise the
    // retry at the openStream boundary instead.
    kind_fallback_scripted->fail_video_open_count.store(1);
    const auto video_only_recovery_started = std::chrono::steady_clock::now();
    kind_fallback_scripted->injectEvent(
        axtp::EventId::VideoStreamSourceStateChanged,
        "video.streamSourceStateChanged",
        R"({"state":"receiving"})");
    require(wait_until([&]() {
        return kind_fallback_scripted->video_open_requests.load() >= 2;
    }), "video-only recovery should make its first open attempt");
    const auto video_only_capabilities_after_failure =
        kind_fallback_scripted->video_capability_requests.load();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    require(kind_fallback_scripted->video_capability_requests.load() ==
                video_only_capabilities_after_failure &&
                kind_fallback_scripted->video_open_requests.load() == 2,
            "global media retry must not bypass a video-only per-kind deadline");
    require(wait_for_stream_events(
                kind_fallback_events,
                kind_fallback_events_mutex,
                3,
                std::chrono::seconds(3)),
            "video-only per-kind retry should eventually publish Opened");
    require(std::chrono::steady_clock::now() - video_only_recovery_started >=
                std::chrono::milliseconds(900) &&
                kind_fallback_scripted->video_open_requests.load() == 3,
            "video-only per-kind retry must respect the one-second interval");
    {
        const auto video_only_recovered =
            kind_fallback_adapter->active_media_stream_descriptors();
        require(video_only_recovered.size() == 1 &&
                    video_only_recovered.front().kind == axent::MediaKind::Video &&
                    video_only_recovered.front().key.generation == 2,
                "video-only retry should restore generation 2");
    }

    std::mutex reentrant_frames_mutex;
    std::vector<axent::MediaFrame> reentrant_frames;
    ScriptedAxtpTransport* reentrant_scripted = nullptr;
    auto reentrant_adapter = axent::testing::AxtpAdapterTestSeam::make(
        media_config, [&](const axent::transport::HidTransportOptions&) {
        auto transport = std::make_unique<ScriptedAxtpTransport>();
        reentrant_scripted = transport.get();
        return transport;
    });
    std::atomic<bool> reentrant_call_succeeded{false};
    reentrant_adapter->set_media_frame_callback(
        [&reentrant_adapter,
         &reentrant_call_succeeded,
         &reentrant_frames,
         &reentrant_frames_mutex](std::string device_id, axent::MediaFrame frame) {
            frame.device_id = std::move(device_id);
            {
                std::lock_guard<std::mutex> lock(reentrant_frames_mutex);
                reentrant_frames.push_back(std::move(frame));
            }
            const auto result =
                reentrant_adapter->call("hid:0581:2582:NA20-SERIAL", "audio.getAlgorithmConfig", {});
            reentrant_call_succeeded.store(result.status == axent::ControlStatus::Ok
                && result.body.value("ok", false));
        });

    require(reentrant_adapter->open_session("hid:0581:2582:NA20-SERIAL", error),
            "reentrant scripted adapter session should open");
    require(reentrant_scripted != nullptr, "reentrant scripted transport should be constructed");
    reentrant_scripted->injectStream(0x1001, 5, 999000, {0x00, 0x00, 0x01, 0x65});
    const auto reentrant_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (!reentrant_call_succeeded.load() && std::chrono::steady_clock::now() < reentrant_deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    {
        std::lock_guard<std::mutex> lock(reentrant_frames_mutex);
        require(reentrant_frames.size() == 1, "reentrant callback should receive one media frame");
    }
    require(reentrant_call_succeeded.load(), "media callback should be able to re-enter adapter call");

    axent::AxtpAdapterConfig frame_rate_config = axent::AxtpAdapter::na20_defaults();
    frame_rate_config.video_frame_rate = 25;
    ScriptedAxtpTransport* frame_rate_scripted = nullptr;
    auto frame_rate_adapter = axent::testing::AxtpAdapterTestSeam::make(
        frame_rate_config, [&](const axent::transport::HidTransportOptions&) {
            auto transport = std::make_unique<ScriptedAxtpTransport>();
            // NA20 may reuse the numeric video stream id after terminal close;
            // generation, not the id alone, separates the replacement.
            transport->unique_video_stream_ids = false;
            transport->close_returns_closing.store(true);
            transport->audio_close_returns_closing.store(true);
            frame_rate_scripted = transport.get();
            return transport;
        });
    std::mutex frame_rate_events_mutex;
    std::vector<axent::MediaStreamEvent> frame_rate_events;
    frame_rate_adapter->set_media_stream_event_callback(
        [&](axent::MediaStreamEvent event) {
            std::lock_guard<std::mutex> lock(frame_rate_events_mutex);
            frame_rate_events.push_back(std::move(event));
        });
    require(frame_rate_adapter->open_session(
                "hid:0581:2582:NA20-SERIAL", error),
            "frame-rate adapter session should open");
    require(frame_rate_scripted != nullptr &&
                wait_for_stream_events(frame_rate_events, frame_rate_events_mutex, 2),
            "frame-rate adapter should open initial audio and video streams");
    {
        std::lock_guard<std::mutex> lock(frame_rate_scripted->requests_mutex);
        require(frame_rate_scripted->video_open_params.size() == 1 &&
                    frame_rate_scripted->video_open_params.front().at("frameRate") == 25,
                "startup frame rate must be forwarded to the first video.openStream");
    }
    const auto initial_frame_rate_descriptors =
        frame_rate_adapter->active_media_stream_descriptors();
    const auto initial_video = std::find_if(
        initial_frame_rate_descriptors.begin(), initial_frame_rate_descriptors.end(),
        [](const auto& descriptor) { return descriptor.kind == axent::MediaKind::Video; });
    const auto initial_audio = std::find_if(
        initial_frame_rate_descriptors.begin(), initial_frame_rate_descriptors.end(),
        [](const auto& descriptor) { return descriptor.kind == axent::MediaKind::Audio; });
    require(initial_video != initial_frame_rate_descriptors.end() &&
                initial_video->frame_rate == 25 &&
                initial_audio != initial_frame_rate_descriptors.end(),
            "negotiated frame rate and initial audio stream must be exposed");
    std::size_t request_order_before_reconfigure = 0;
    {
        std::lock_guard<std::mutex> lock(frame_rate_scripted->requests_mutex);
        request_order_before_reconfigure = frame_rate_scripted->media_request_order.size();
    }

    std::mutex params_updates_mutex;
    std::vector<axent::VideoStreamParamsState> params_updates;
    auto params_subscription = frame_rate_adapter->subscribe_video_stream_params(
        "hid:0581:2582:NA20-SERIAL",
        [&](const axent::VideoStreamParamsState& update) {
            std::lock_guard<std::mutex> lock(params_updates_mutex);
            params_updates.push_back(update);
        });
    require(params_subscription != nullptr,
            "frame-rate state subscription should be available for the active session");

    axent::VideoStreamParamsRequest set_fifteen;
    set_fifteen.frame_rate = 15;
    const auto pending_frame_rate = frame_rate_adapter->set_video_stream_params(
        "hid:0581:2582:NA20-SERIAL", set_fifteen);
    require(pending_frame_rate.status_code == 0 && pending_frame_rate.accepted &&
                pending_frame_rate.state.state == axent::VideoStreamParamsStateKind::Pending &&
                pending_frame_rate.state.phase == axent::VideoStreamParamsPhase::Closing,
            "active frame-rate update should be accepted as pending/closing");
    const auto concurrent_frame_rate = frame_rate_adapter->set_video_stream_params(
        "hid:0581:2582:NA20-SERIAL", set_fifteen);
    require(concurrent_frame_rate.status_code == 0x0005 && !concurrent_frame_rate.accepted,
            "concurrent frame-rate update should fail fast with BUSY");
    require(wait_until([&]() {
        return frame_rate_adapter->video_stream_params_state(
            "hid:0581:2582:NA20-SERIAL").state ==
                axent::VideoStreamParamsStateKind::Applied;
    }), "frame-rate close/open transaction should reach applied");
    require(frame_rate_scripted->video_close_requests.load() == 1 &&
                frame_rate_scripted->video_state_requests.load() >= 1 &&
                frame_rate_scripted->audio_close_requests.load() == 1 &&
                frame_rate_scripted->audio_state_requests.load() >= 1 &&
                frame_rate_scripted->video_open_requests.load() == 2 &&
                frame_rate_scripted->audio_open_requests.load() == 2,
            "active frame-rate update must close, await, and reopen audio and video");
    {
        std::lock_guard<std::mutex> lock(frame_rate_scripted->requests_mutex);
        require(frame_rate_scripted->video_close_params.front().at("reason") ==
                    "encodingReconfigure" &&
                    frame_rate_scripted->audio_close_params.front().at("reason") ==
                        "encodingReconfigure" &&
                    frame_rate_scripted->video_open_params.back().at("frameRate") == 15 &&
                    nlohmann::json::parse(
                        frame_rate_scripted->video_open_wire_bodies.back()).at("frameRate") == 15,
                "reconfiguration wire parameters mismatch");
        const std::vector<std::string> expected_order{
            "video.close", "audio.close", "video.open", "audio.open"};
        require(frame_rate_scripted->media_request_order.size() >=
                    request_order_before_reconfigure + expected_order.size() &&
                    std::equal(
                        expected_order.begin(), expected_order.end(),
                        frame_rate_scripted->media_request_order.begin() +
                            static_cast<std::ptrdiff_t>(request_order_before_reconfigure)),
                "both close requests must precede either replacement open request");
    }
    const auto applied_state = frame_rate_adapter->video_stream_params_state(
        "hid:0581:2582:NA20-SERIAL");
    require(applied_state.desired_frame_rate == 15U &&
                applied_state.effective_frame_rate == 15U &&
                applied_state.active_stream_id.has_value() &&
                applied_state.previous_stream_id.has_value() &&
                applied_state.active_stream_id == applied_state.previous_stream_id,
            "same-id replacement should preserve the numeric id while advancing generation");
    const auto applied_descriptors = frame_rate_adapter->active_media_stream_descriptors();
    const auto applied_video = std::find_if(
        applied_descriptors.begin(), applied_descriptors.end(),
        [](const auto& descriptor) { return descriptor.kind == axent::MediaKind::Video; });
    const auto applied_audio = std::find_if(
        applied_descriptors.begin(), applied_descriptors.end(),
        [](const auto& descriptor) { return descriptor.kind == axent::MediaKind::Audio; });
    require(applied_video != applied_descriptors.end() &&
                applied_audio != applied_descriptors.end() &&
                applied_video->key.generation > initial_video->key.generation &&
                applied_audio->key.generation > initial_audio->key.generation,
            "frame-rate reconfiguration must advance both stream generations");

    const auto unchanged_frame_rate = frame_rate_adapter->set_video_stream_params(
        "hid:0581:2582:NA20-SERIAL", set_fifteen);
    require(unchanged_frame_rate.status_code == 0 && unchanged_frame_rate.accepted &&
                unchanged_frame_rate.state.state == axent::VideoStreamParamsStateKind::Unchanged &&
                frame_rate_scripted->video_close_requests.load() == 1,
            "same frame rate should return unchanged without closing the stream");

    axent::VideoStreamParamsRequest reset_frame_rate;
    reset_frame_rate.reset_frame_rate = true;
    const auto reset_pending = frame_rate_adapter->set_video_stream_params(
        "hid:0581:2582:NA20-SERIAL", reset_frame_rate);
    require(reset_pending.status_code == 0 && reset_pending.accepted,
            "frame-rate reset should be accepted");
    require(wait_until([&]() {
        const auto state = frame_rate_adapter->video_stream_params_state(
            "hid:0581:2582:NA20-SERIAL");
        return state.state == axent::VideoStreamParamsStateKind::Applied &&
            !state.desired_frame_rate.has_value();
    }), "frame-rate reset should reopen using the source default");
    {
        std::lock_guard<std::mutex> lock(frame_rate_scripted->requests_mutex);
        require(frame_rate_scripted->video_open_params.size() == 3 &&
                    !frame_rate_scripted->video_open_params.back().contains("frameRate"),
                "reset open must omit frameRate to restore the source/profile default");
    }
    require(frame_rate_scripted->audio_close_requests.load() == 2 &&
                frame_rate_scripted->audio_open_requests.load() == 3,
            "frame-rate reset must restart audio together with video");

    axent::VideoStreamParamsRequest invalid_frame_rate;
    invalid_frame_rate.frame_rate = 0;
    const auto invalid_result = frame_rate_adapter->set_video_stream_params(
        "hid:0581:2582:NA20-SERIAL", invalid_frame_rate);
    require(invalid_result.status_code == 0x000A && !invalid_result.accepted,
            "zero encoder frame rate should be rejected as INVALID_ARGUMENT");
    axent::VideoStreamParamsRequest unsupported_frame_rate;
    unsupported_frame_rate.frame_rate = 17;
    const auto unsupported_result = frame_rate_adapter->set_video_stream_params(
        "hid:0581:2582:NA20-SERIAL", unsupported_frame_rate);
    require(unsupported_result.status_code == 0x0805 && !unsupported_result.accepted,
            "frame rate outside the advertised source profile should be rejected");

    // A silent NA20 reset must be recovered without a HID hotplug event.  The
    // scripted transport drops only the read-only capability probe on the old
    // physical session; a replacement transport answers normally.
    axent::AxtpAdapterConfig recovery_config = axent::AxtpAdapter::na20_defaults();
    recovery_config.session_probe_mode = axent::SessionProbeMode::LegacyRpc;
    recovery_config.session_health_probe_interval_ms = 20;
    recovery_config.session_health_probe_timeout_ms = 15;
    recovery_config.session_health_failure_threshold = 2;
    recovery_config.session_recovery_backoff_initial_ms = 20;
    recovery_config.session_recovery_backoff_max_ms = 40;
    std::atomic<bool> silent_reset{false};
    std::atomic<int> recovery_factory_calls{0};
    std::atomic<ScriptedAxtpTransport*> current_recovery_transport{nullptr};
    auto recovery_adapter = axent::testing::AxtpAdapterTestSeam::make(
        recovery_config, [&](const axent::transport::HidTransportOptions&) {
            const auto call = recovery_factory_calls.fetch_add(1) + 1;
            auto transport = std::make_unique<ScriptedAxtpTransport>();
            transport->drop_capability_responses = call == 1;
            transport->silent_reset = &silent_reset;
            current_recovery_transport.store(transport.get());
            return transport;
        });
    std::mutex recovery_events_mutex;
    std::vector<axent::MediaStreamEvent> recovery_events;
    recovery_adapter->set_media_stream_event_callback(
        [&](axent::MediaStreamEvent event) {
            std::lock_guard<std::mutex> lock(recovery_events_mutex);
            recovery_events.push_back(std::move(event));
        });
    const std::string recovery_device_id = "hid:0581:2582:NA20-RECOVERY";
    require(recovery_adapter->open_session(recovery_device_id, error),
            "silent-session recovery adapter should open");
    require(wait_for_stream_events(
                recovery_events, recovery_events_mutex, 2),
            "silent-session recovery adapter should open both media legs");
    axent::testing::AxtpAdapterTestSeam::bind_media_delivery_session(
        *recovery_adapter, recovery_device_id, "logical-media-session");
    require(recovery_adapter->video_stream_params_state(recovery_device_id).session_id ==
                "logical-media-session",
            "media binding should expose the logical session id");

    auto* initial_recovery_transport = current_recovery_transport.load();
    require(initial_recovery_transport != nullptr,
            "silent-session recovery should retain the initial transport");
    initial_recovery_transport->drop_next_capability_responses.store(1);
    require(wait_until([&]() {
        return recovery_adapter->diagnostics().health_probe_failures >= 1;
    }), "one dropped capability probe should be observable");
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    require(recovery_factory_calls.load() == 1 &&
                recovery_adapter->diagnostics().session_recoveries == 0,
            "a single probe timeout must not rebuild a healthy session");

    initial_recovery_transport->capability_business_error.store(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    require(recovery_factory_calls.load() == 1 &&
                recovery_adapter->diagnostics().health_probe_failures == 0,
            "a typed capability error must prove the session is online");
    initial_recovery_transport->capability_business_error.store(false);
    silent_reset.store(true);
    require(wait_until([&]() {
        const auto diagnostics = recovery_adapter->diagnostics();
        return recovery_factory_calls.load() >= 2 &&
            diagnostics.session_recoveries >= 1 &&
            diagnostics.session_health == axent::SessionHealthState::Healthy;
    }), "two silent capability probe failures should rebuild the AXTP session");
    require(wait_for_stream_events(recovery_events, recovery_events_mutex, 6),
            "session recovery should publish closed and opened lifecycle events");
    {
        std::lock_guard<std::mutex> lock(recovery_events_mutex);
        require(recovery_events[2].kind == axent::MediaStreamEventKind::Closed &&
                    recovery_events[3].kind == axent::MediaStreamEventKind::Closed &&
                    recovery_events[2].reason == axent::MediaStreamEventReason::SessionRecovery &&
                    recovery_events[3].reason == axent::MediaStreamEventReason::SessionRecovery &&
                    recovery_events[4].kind == axent::MediaStreamEventKind::Opened &&
                    recovery_events[5].kind == axent::MediaStreamEventKind::Opened &&
                    recovery_events[4].reason == axent::MediaStreamEventReason::SessionRecovery &&
                    recovery_events[5].reason == axent::MediaStreamEventReason::SessionRecovery,
                "session recovery lifecycle ordering/reason mismatch");
    }
    const auto recovered_descriptors = recovery_adapter->active_media_stream_descriptors();
    require(recovered_descriptors.size() == 2 &&
                recovered_descriptors[0].key.generation == 2 &&
                recovered_descriptors[1].key.generation == 2,
            "session recovery should reopen both streams with a new generation");
    require(recovery_adapter->video_stream_params_state(recovery_device_id).session_id ==
                "logical-media-session",
            "session recovery must retain the logical media lease/session id");

    auto* recovered_transport = current_recovery_transport.load();
    require(recovered_transport != nullptr &&
                recovery_factory_calls.load() == 2,
            "session recovery should replace exactly one physical transport");
    // Trigger another probe failure and release while recovery is pending.  A
    // release must cancel the worker and prevent a late replacement session.
    recovered_transport->drop_capability_responses = true;
    silent_reset.store(true);
    require(wait_until([&]() {
        return recovery_adapter->diagnostics().health_probe_failures >= 1;
    }), "second silent reset should enter probe-failure state");
    axent::testing::AxtpAdapterTestSeam::release_session(
        *recovery_adapter, recovery_device_id);
    const auto factory_calls_after_release = recovery_factory_calls.load();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    require(recovery_factory_calls.load() == factory_calls_after_release,
            "release must cancel pending session recovery attempts");
    require(recovery_adapter->active_media_stream_descriptors().empty() &&
                !recovery_adapter->diagnostics().open,
            "released silent session must have no active streams or open transport");

    frame_rate_scripted->fail_video_open_count.store(1);
    const auto rollback_pending = frame_rate_adapter->set_video_stream_params(
        "hid:0581:2582:NA20-SERIAL", set_fifteen);
    require(rollback_pending.status_code == 0 && rollback_pending.accepted,
            "rollback scenario should begin as an accepted reconfiguration");
    require(wait_until([&]() {
        return frame_rate_adapter->video_stream_params_state(
            "hid:0581:2582:NA20-SERIAL").state ==
                axent::VideoStreamParamsStateKind::RolledBack;
    }), "failed replacement open should restore the previous stream parameters");
    const auto rolled_back_state = frame_rate_adapter->video_stream_params_state(
        "hid:0581:2582:NA20-SERIAL");
    require(rolled_back_state.rollback_applied &&
                !rolled_back_state.desired_frame_rate.has_value() &&
                rolled_back_state.active_stream_id.has_value(),
            "rollback success should restore source-default selection and an active stream");
    {
        std::lock_guard<std::mutex> lock(frame_rate_scripted->requests_mutex);
        require(frame_rate_scripted->video_open_params.size() == 5 &&
                    frame_rate_scripted->video_open_params[3].at("frameRate") == 15 &&
                    !frame_rate_scripted->video_open_params[4].contains("frameRate"),
                "rollback should retry open with the previous source-default parameters");
    }

    frame_rate_scripted->fail_audio_open_count.store(1);
    const auto audio_rollback_pending = frame_rate_adapter->set_video_stream_params(
        "hid:0581:2582:NA20-SERIAL", set_fifteen);
    require(audio_rollback_pending.status_code == 0 && audio_rollback_pending.accepted,
            "audio-open rollback scenario should begin as pending");
    require(wait_until([&]() {
        return frame_rate_adapter->video_stream_params_state(
            "hid:0581:2582:NA20-SERIAL").state ==
                axent::VideoStreamParamsStateKind::RolledBack;
    }), "failed replacement audio open should close replacement video and restore both streams");
    const auto after_audio_rollback = frame_rate_adapter->active_media_stream_descriptors();
    require(after_audio_rollback.size() == 2 &&
                std::count_if(
                    after_audio_rollback.begin(), after_audio_rollback.end(),
                    [](const auto& descriptor) {
                        return descriptor.kind == axent::MediaKind::Video;
                    }) == 1 &&
                std::count_if(
                    after_audio_rollback.begin(), after_audio_rollback.end(),
                    [](const auto& descriptor) {
                        return descriptor.kind == axent::MediaKind::Audio;
                    }) == 1,
            "audio-open rollback must restore one active audio and one active video stream");

    frame_rate_scripted->fail_video_open_count.store(2);
    const auto rollback_failure_pending = frame_rate_adapter->set_video_stream_params(
        "hid:0581:2582:NA20-SERIAL", set_fifteen);
    require(rollback_failure_pending.status_code == 0 && rollback_failure_pending.accepted,
            "rollback-failure scenario should begin as pending");
    require(wait_until([&]() {
        return frame_rate_adapter->video_stream_params_state(
            "hid:0581:2582:NA20-SERIAL").state ==
                axent::VideoStreamParamsStateKind::Failed;
    }), "replacement and rollback open failures should reach failed");
    const auto rollback_failed_state = frame_rate_adapter->video_stream_params_state(
        "hid:0581:2582:NA20-SERIAL");
    require(!rollback_failed_state.rollback_applied &&
                !rollback_failed_state.active_stream_id.has_value() &&
                rollback_failed_state.last_error.has_value(),
            "rollback failure should leave no active video stream and preserve typed error");
    const auto after_rollback_failure = frame_rate_adapter->active_media_stream_descriptors();
    require(after_rollback_failure.empty(),
            "rollback failure after a paired close must not expose a stale audio stream");
    std::this_thread::sleep_for(std::chrono::milliseconds(750));
    require(frame_rate_adapter->active_media_stream_descriptors().empty(),
            "failed paired rollback must not auto-reopen an audio stream without its video");

    // Codec negotiation is session-local: auto prefers H.265 when both the
    // peer and the runtime advertise it, but a rejected H.265 open retries
    // H.264 in the same configure attempt without mutating the preference
    // vector used by the next session.
    {
        auto codec_config = axent::AxtpAdapter::na20_defaults();
        codec_config.video_codec_preferences = {
            axent::MediaCodec::H265, axent::MediaCodec::H264};
        ScriptedAxtpTransport* codec_scripted = nullptr;
        auto codec_adapter = axent::testing::AxtpAdapterTestSeam::make(
            codec_config, [&](const axent::transport::HidTransportOptions&) {
            auto transport = std::make_unique<ScriptedAxtpTransport>();
            transport->advertise_video_codecs = true;
            codec_scripted = transport.get();
            return transport;
        });
        std::string codec_error;
        require(codec_adapter->open_session(
                    "hid:0581:2582:NA20-SERIAL", codec_error),
                "H.265 codec session should open: " + codec_error);
        require(codec_scripted != nullptr &&
                    codec_scripted->video_open_params.size() == 1 &&
                    codec_scripted->video_open_params.front().at("codec") == "h265",
                "auto codec selection should request H.265 first");
        const auto negotiated = codec_adapter->active_media_stream_descriptors();
        require(!negotiated.empty() && negotiated.front().codec == axent::MediaCodec::H265,
                "H.265 lifecycle descriptor should remain authoritative");
        const auto codec_diagnostics = codec_adapter->diagnostics();
        require(codec_diagnostics.device_video_codecs.size() == 2 &&
                    codec_diagnostics.requested_video_codec == "h265" &&
                    codec_diagnostics.negotiated_video_codec == "h265" &&
                    codec_diagnostics.video_codec_fallback_reason.empty(),
                "H.265 negotiation diagnostics should expose capability and result");
        axent::testing::AxtpAdapterTestSeam::disconnect_session(*codec_adapter);
    }

    {
        auto fallback_config = axent::AxtpAdapter::na20_defaults();
        fallback_config.video_codec_preferences = {
            axent::MediaCodec::H265, axent::MediaCodec::H264};
        ScriptedAxtpTransport* fallback_scripted = nullptr;
        auto fallback_adapter = axent::testing::AxtpAdapterTestSeam::make(
            fallback_config, [&](const axent::transport::HidTransportOptions&) {
            auto transport = std::make_unique<ScriptedAxtpTransport>();
            transport->advertise_video_codecs = true;
            transport->reject_h265_open = true;
            fallback_scripted = transport.get();
            return transport;
        });
        std::string fallback_error;
        require(fallback_adapter->open_session(
                    "hid:0581:2582:NA20-SERIAL", fallback_error),
                "H.265 rejection should fall back to H.264");
        require(fallback_scripted != nullptr &&
                    fallback_scripted->video_open_params.size() == 2 &&
                    fallback_scripted->video_open_params[0].at("codec") == "h265" &&
                    fallback_scripted->video_open_params[1].at("codec") == "h264",
                "codec fallback should retry H.264 immediately");
        const auto negotiated = fallback_adapter->active_media_stream_descriptors();
        require(!negotiated.empty() && negotiated.front().codec == axent::MediaCodec::H264,
                "fallback descriptor should confirm H.264");
        const auto fallback_diagnostics = fallback_adapter->diagnostics();
        require(fallback_diagnostics.requested_video_codec == "h264" &&
                    fallback_diagnostics.negotiated_video_codec == "h264" &&
                    !fallback_diagnostics.video_codec_fallback_reason.empty(),
                "codec fallback diagnostics should retain the H.265 failure reason");
        axent::testing::AxtpAdapterTestSeam::disconnect_session(*fallback_adapter);
    }

    {
        auto legacy_config = axent::AxtpAdapter::na20_defaults();
        legacy_config.video_codec_preferences = {axent::MediaCodec::H264};
        ScriptedAxtpTransport* legacy_scripted = nullptr;
        auto legacy_adapter = axent::testing::AxtpAdapterTestSeam::make(
            legacy_config, [&](const axent::transport::HidTransportOptions&) {
            auto transport = std::make_unique<ScriptedAxtpTransport>();
            transport->advertise_video_codecs = true;
            transport->omit_video_open_codec = true;
            legacy_scripted = transport.get();
            return transport;
        });
        std::string legacy_error;
        require(legacy_adapter->open_session(
                    "hid:0581:2582:NA20-SERIAL", legacy_error),
                "legacy H.264 response without a codec should remain compatible");
        require(legacy_scripted != nullptr &&
                    !legacy_adapter->active_media_stream_descriptors().empty(),
                "legacy H.264 response should publish a descriptor");
        axent::testing::AxtpAdapterTestSeam::disconnect_session(*legacy_adapter);
    }

    {
        auto explicit_config = axent::AxtpAdapter::na20_defaults();
        explicit_config.video_codec_preferences = {axent::MediaCodec::H265};
        ScriptedAxtpTransport* explicit_scripted = nullptr;
        auto explicit_adapter = axent::testing::AxtpAdapterTestSeam::make(
            explicit_config, [&](const axent::transport::HidTransportOptions&) {
            auto transport = std::make_unique<ScriptedAxtpTransport>();
            transport->advertise_video_codecs = true;
            transport->omit_video_open_codec = true;
            explicit_scripted = transport.get();
            return transport;
        });
        std::string explicit_error;
        require(explicit_adapter->open_session(
                    "hid:0581:2582:NA20-SERIAL", explicit_error),
                "transport session should remain open while H.265 negotiation fails");
        require(explicit_scripted != nullptr &&
                    wait_until([&]() {
                        return explicit_adapter->diagnostics().active_video_stream_id == 0;
                    }),
                "H.265 response without an explicit codec must not publish a video stream");
        require(explicit_scripted->video_open_params.size() >= 1,
                "explicit codec test should construct transport");
    }

    // The H.265 bring-up path must still issue video.openStream when the
    // preceding capabilities response says the source is waiting.  The
    // openStream response remains authoritative for the negotiated contract.
    {
        auto bypass_config = axent::AxtpAdapter::na20_defaults();
        bypass_config.video_codec_preferences = {axent::MediaCodec::H265};
        bypass_config.video_open_codec_override = axent::MediaCodec::H264;
        bypass_config.video_decode_codec_override = axent::MediaCodec::H265;
        ScriptedAxtpTransport* bypass_scripted = nullptr;
        auto bypass_adapter = axent::testing::AxtpAdapterTestSeam::make(
            bypass_config, [&](const axent::transport::HidTransportOptions&) {
            auto transport = std::make_unique<ScriptedAxtpTransport>();
            transport->fail_next_video_capabilities.store(true);
            bypass_scripted = transport.get();
            return transport;
        });
        std::string bypass_error;
        require(bypass_adapter->open_session(
                    "hid:0581:2582:NA20-SERIAL", bypass_error),
                "capability waiting must not prevent the H.265 session from opening");
        require(bypass_scripted != nullptr &&
                    bypass_scripted->video_open_params.size() >= 1 &&
                    bypass_scripted->video_open_params.front().at("codec") == "h264",
                "H.265 decode bypass must issue video.openStream(codec=h264)");
        const auto bypass_diagnostics = bypass_adapter->diagnostics();
        require(bypass_diagnostics.video_codec_capabilities_bypassed &&
                    bypass_diagnostics.video_open_stream_attempts >= 1 &&
                    bypass_diagnostics.video_open_last_status == "accepted" &&
                    bypass_diagnostics.requested_video_codec == "h265" &&
                    bypass_diagnostics.video_open_request_codec == "h264" &&
                    bypass_diagnostics.negotiated_video_codec == "h264" &&
                    bypass_diagnostics.video_decode_codec == "h265" &&
                    bypass_diagnostics.video_codec_decode_bypassed &&
                    !bypass_adapter->active_media_stream_descriptors().empty() &&
                    bypass_adapter->active_media_stream_descriptors().front().codec ==
                        axent::MediaCodec::H265,
                "forced H.265 decode bypass diagnostics should expose both contracts");
        axent::testing::AxtpAdapterTestSeam::disconnect_session(*bypass_adapter);
    }

    return 0;
}
