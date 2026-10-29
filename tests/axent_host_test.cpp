#include <algorithm>
#include <chrono>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "axent/host/axent_host.hpp"
#include "axtp_adapter_test_seam.hpp"

#include "../src/core/control_operation_internal.hpp"

#include "core/protocol/wire/inbound_processor.hpp"
#include "core/protocol/wire/outbound_processor.hpp"
#include "hidapi/hid_transport.hpp"

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::fprintf(stderr, "axent_host_test assertion failed: %s\n", message);
        std::fflush(stderr);
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

    void injectStreamOnNextRequest(std::uint32_t stream_id,
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
        stream_on_next_request_ = encode_stream(std::move(stream));
    }

    void blockAfterNextStream()
    {
        std::lock_guard<std::mutex> lock(block_mutex_);
        block_after_next_stream_ = true;
        stream_blocked_ = false;
        unblock_stream_ = false;
    }

    bool waitForStreamBlocked()
    {
        std::unique_lock<std::mutex> lock(block_mutex_);
        return block_cv_.wait_for(lock, std::chrono::seconds(1), [this]() {
            return stream_blocked_;
        });
    }

    void unblockStream()
    {
        {
            std::lock_guard<std::mutex> lock(block_mutex_);
            unblock_stream_ = true;
        }
        block_cv_.notify_all();
    }

    void blockNextBusinessRequest()
    {
        std::lock_guard<std::mutex> lock(block_mutex_);
        block_next_business_request_ = true;
        business_request_blocked_ = false;
        unblock_business_request_ = false;
    }

    bool waitForBusinessRequestBlocked()
    {
        std::unique_lock<std::mutex> lock(block_mutex_);
        return block_cv_.wait_for(lock, std::chrono::seconds(1), [this]() {
            return business_request_blocked_;
        });
    }

    bool waitForBusinessRequests(std::size_t count)
    {
        std::unique_lock<std::mutex> lock(block_mutex_);
        return block_cv_.wait_for(lock, std::chrono::seconds(1), [this, count]() {
            return business_request_count_ >= count;
        });
    }

    std::size_t businessRequestCount() const
    {
        std::lock_guard<std::mutex> lock(block_mutex_);
        return business_request_count_;
    }

    void unblockBusinessRequest()
    {
        {
            std::lock_guard<std::mutex> lock(block_mutex_);
            unblock_business_request_ = true;
        }
        block_cv_.notify_all();
    }

    void sendBytes(const axtp::Byte* data, std::size_t size) override
    {
        CapturingPayloadSink payload_sink;
        axtp::InboundProcessor inbound(payload_sink);
        inbound.onBytes(data, size);

        for (const auto& control : payload_sink.controls) {
            if (control.opcode == axtp::ControlOpcode::Open) {
                axtp::ControlPayload accept;
                accept.opcode = axtp::ControlOpcode::Accept;
                accept.controlId = control.controlId;
                accept.statusCode = axtp::ErrorCode::Success;
                inject(encode_control(accept));
                inject(encode_rpc(axtp::JsonRpcEncoder::makeHello()));
            }
        }

        for (const auto& rpc : payload_sink.rpcs) {
            if (rpc.op == axtp::RpcOp::Identify) {
                inject(encode_rpc(axtp::JsonRpcEncoder::makeIdentified("axent-host-session")));
                continue;
            }
            if (rpc.op == axtp::RpcOp::Request) {
                saw_business_request = true;
                {
                    std::unique_lock<std::mutex> lock(block_mutex_);
                    ++business_request_count_;
                    if (block_next_business_request_) {
                        block_next_business_request_ = false;
                        business_request_blocked_ = true;
                        block_cv_.notify_all();
                        block_cv_.wait(lock, [this]() {
                            return unblock_business_request_;
                        });
                    }
                }
                std::optional<axtp::Bytes> request_stream;
                {
                    std::lock_guard<std::mutex> lock(rx_mutex_);
                    request_stream.swap(stream_on_next_request_);
                }
                if (request_stream.has_value()) {
                    inject(*request_stream);
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
                std::string body = R"({"via":"axtp"})";
                if (rpc.methodOrEventId ==
                    static_cast<std::uint32_t>(axtp::MethodId::VideoGetStreamCapabilities)) {
                    body = R"({"supported":true,"openModes":["receiver_pull"],"sourceState":{"available":true,"state":"receiving"},"sources":[{"sourceId":"wireless_cast","currentState":"receiving"}]})";
                } else if (rpc.methodOrEventId ==
                           static_cast<std::uint32_t>(axtp::MethodId::AudioGetStreamCapabilities)) {
                    body = R"({"supported":true,"openModes":["receiver_pull"],"sourceState":{"available":true,"state":"receiving"},"sources":[{"sourceId":"wireless_cast_audio","currentState":"receiving","channels":[2],"sampleRates":[48000]}]})";
                } else if (rpc.methodOrEventId ==
                           static_cast<std::uint32_t>(axtp::MethodId::VideoOpenStream)) {
                    body = R"({"streamId":4097,"state":"streaming","source":"wireless_cast","codec":"h264","width":1920,"height":1080,"streamProfile":"media.video","cursorUnit":"timestampUs"})";
                } else if (rpc.methodOrEventId ==
                           static_cast<std::uint32_t>(axtp::MethodId::AudioOpenStream)) {
                    body = R"({"streamId":8193,"state":"streaming","source":"wireless_cast_audio","codec":"aac","transportFormat":"adts","sampleRate":48000,"channels":2,"streamProfile":"media.audio","cursorUnit":"timestampUs"})";
                }
                response.body = axtp::Bytes(body.begin(), body.end());
                inject(encode_rpc(response));
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

    bool saw_business_request = false;

private:
    void inject(const axtp::Bytes& bytes)
    {
        if (sink_ != nullptr) {
            sink_->onBytes(bytes.data(), bytes.size());
        }
        maybeBlockAfterStreamInject();
    }

    void maybeBlockAfterStreamInject()
    {
        std::unique_lock<std::mutex> lock(block_mutex_);
        if (!block_after_next_stream_) {
            return;
        }
        block_after_next_stream_ = false;
        stream_blocked_ = true;
        block_cv_.notify_all();
        block_cv_.wait(lock, [this]() {
            return unblock_stream_;
        });
    }

    axtp::IByteSink* sink_ = nullptr;
    std::mutex rx_mutex_;
    std::queue<axtp::Bytes> rx_queue_;
    std::optional<axtp::Bytes> stream_on_next_request_;
    mutable std::mutex block_mutex_;
    std::condition_variable block_cv_;
    bool block_after_next_stream_ = false;
    bool stream_blocked_ = false;
    bool unblock_stream_ = false;
    bool block_next_business_request_ = false;
    bool business_request_blocked_ = false;
    bool unblock_business_request_ = false;
    std::size_t business_request_count_ = 0;
    bool open_ = false;
};

axent::MediaFrame make_media_frame(std::uint64_t sequence_id)
{
    axent::MediaFrame frame;
    frame.session_id = "filled-by-test";
    frame.stream_id = 0x2001;
    frame.kind = axent::MediaKind::Video;
    frame.codec = axent::MediaCodec::H264;
    frame.sequence_id = sequence_id;
    frame.cursor = sequence_id * 1000U;
    frame.flags = sequence_id == 1 ? axent::MediaFrameFlag::KeyFrame : axent::MediaFrameFlag::EndOfFrame;
    frame.payload = {0, 0, 1, static_cast<std::uint8_t>(sequence_id)};
    return frame;
}

axent::DeviceSnapshot make_second_mock_device()
{
    axent::DeviceSnapshot device;
    device.id = "mock-device-002";
    device.adapter = "mock";
    device.identity.vendor = "Mostorm";
    device.identity.model = "MockCam";
    device.identity.serial_number = "MOCK002";
    device.identity.firmware_version = "mock-fw-1.0.0";
    device.identity.hardware_version = "mock-hw-revA";
    device.connection.online = true;
    device.connection.transport = "mock";
    device.connection.last_change_reason = "test-upserted";
    device.status.health = "ok";
    return device;
}

class StatefulInventoryAdapter final : public axent::Adapter {
public:
    explicit StatefulInventoryAdapter(std::shared_ptr<int> discovery_count)
        : discovery_count_(std::move(discovery_count))
    {
    }

    axent::AdapterMetadata metadata() const override
    {
        return {"inventory", "Stateful inventory adapter", true, ""};
    }

    std::vector<axent::Capability> capabilities() const override
    {
        return {};
    }

    std::vector<axent::DeviceSnapshot> discover() override
    {
        switch (++*discovery_count_) {
        case 1:
            return {device("device-a"), device("device-b")};
        case 2:
            return {device("device-b"), device("device-c")};
        default:
            return {device("device-a"), device("device-b"), device("device-c")};
        }
    }

    axent::ControlResult call(const std::string&,
                              const std::string&,
                              const nlohmann::json&) override
    {
        return {axent::ControlStatus::Unavailable, nlohmann::json::object()};
    }

    axent::ControlResult start_firmware_update(const std::string&,
                                               const std::string&) override
    {
        return {axent::ControlStatus::Unavailable, nlohmann::json::object()};
    }

private:
    static axent::DeviceSnapshot device(std::string id)
    {
        axent::DeviceSnapshot snapshot;
        snapshot.id = std::move(id);
        snapshot.adapter = "inventory";
        snapshot.endpoint_id = "ep-" + snapshot.id;
        snapshot.connection.online = true;
        snapshot.connection.transport = "test";
        return snapshot;
    }

    std::shared_ptr<int> discovery_count_;
};

class BatchInventoryAdapter final : public axent::Adapter {
public:
    explicit BatchInventoryAdapter(std::shared_ptr<int> discovery_count)
        : discovery_count_(std::move(discovery_count))
    {
    }

    axent::AdapterMetadata metadata() const override
    {
        return {"batch-inventory", "Batch inventory adapter", true, ""};
    }

    std::vector<axent::Capability> capabilities() const override
    {
        return {};
    }

    std::vector<axent::DeviceSnapshot> discover() override
    {
        switch (++*discovery_count_) {
        case 1:
            return {
                device("stable-path-old", "endpoint/stable"),
                device("conflict-a", "endpoint/conflict"),
                device("unrelated", "endpoint/unrelated"),
                device("legacy-old", ""),
                device("endpoint-change", "endpoint/original"),
            };
        case 2:
            return {
                device("stable-path-new", "endpoint/stable"),
                device("legacy-new", ""),
                device("endpoint-change", "endpoint/changed"),
            };
        case 3:
            return {
                device("stable-path-new", "endpoint/stable"),
                device("conflict-a", "endpoint/conflict"),
                device("conflict-b", "endpoint/conflict"),
                device("endpoint-change", "endpoint/original"),
            };
        case 4:
            return {
                device("stable-path-new", "endpoint/stable"),
                device("conflict-b", ""),
                device("endpoint-change", "endpoint/original"),
            };
        case 5:
            return {
                device("stable-path-new", "endpoint/stable"),
                device("conflict-b", ""),
                device("endpoint-change", "endpoint/original"),
            };
        case 6:
            return {
                device("stable-path-new", "endpoint/stable"),
                device("conflict-b", ""),
                device("duplicate-row", "endpoint/duplicate-row"),
                device("duplicate-row", "endpoint/duplicate-row"),
                device("invalid-stale", "endpoint/invalid-a"),
                device("endpoint-change", "endpoint/original"),
            };
        case 7:
            return {
                device("stable-path-new", "endpoint/stable"),
                device("conflict-b", ""),
                device("invalid-key", "endpoint/invalid-a"),
                device("invalid-key", "endpoint/invalid-b"),
                device("endpoint-change", "endpoint/original"),
            };
        case 8:
            return {
                device("stable-path-new", "endpoint/stable"),
                device("conflict-b", ""),
                device("invalid-key", "endpoint/invalid-b"),
                device("invalid-key", "endpoint/invalid-a"),
                device("endpoint-change", "endpoint/original"),
            };
        case 9:
            return {
                device("stable-path-new", "endpoint/stable"),
                device("conflict-b", ""),
                device("rejected-change-a", "endpoint/rejected-original"),
                device("rejected-change-b", "endpoint/rejected-original"),
                device("rejected-target-owner", "endpoint/rejected-target"),
                device("endpoint-change", "endpoint/original"),
            };
        case 10:
        case 11:
            return {
                device("stable-path-new", "endpoint/stable"),
                device("conflict-b", ""),
                device("rejected-change-b", "endpoint/rejected-target"),
                device("endpoint-change", "endpoint/original"),
            };
        default:
            return {
                device("stable-path-new", "endpoint/stable"),
                device("conflict-b", ""),
                device("rejected-change-b", "endpoint/rejected-target"),
                device("rejected-change-c", "endpoint/rejected-original"),
                device("endpoint-change", "endpoint/original"),
            };
        }
    }

    axent::ControlResult call(const std::string& device_id,
                              const std::string&,
                              const nlohmann::json&) override
    {
        return {axent::ControlStatus::Ok, {{"deviceId", device_id}}};
    }

    axent::ControlResult start_firmware_update(const std::string&,
                                               const std::string&) override
    {
        return {axent::ControlStatus::Unavailable, nlohmann::json::object()};
    }

private:
    static axent::DeviceSnapshot device(std::string id, std::string endpoint_id)
    {
        axent::DeviceSnapshot snapshot;
        snapshot.id = std::move(id);
        snapshot.adapter = "batch-inventory";
        snapshot.endpoint_id = std::move(endpoint_id);
        snapshot.connection.online = true;
        snapshot.connection.transport = "test";
        return snapshot;
    }

    std::shared_ptr<int> discovery_count_;
};

struct EndpointCancellationState {
    bool wait_for_cancellations(std::size_t count)
    {
        std::unique_lock<std::mutex> lock(mutex);
        return cv.wait_for(lock, std::chrono::seconds(1), [&]() {
            return cancellations >= count;
        });
    }

    std::mutex mutex;
    std::condition_variable cv;
    std::size_t cancellations = 0;
};

class EndpointRecordingAdapter final : public axent::Adapter {
public:
    explicit EndpointRecordingAdapter(std::shared_ptr<EndpointCancellationState> cancellation_state)
        : cancellation_state_(std::move(cancellation_state))
    {
    }

    axent::AdapterMetadata metadata() const override
    {
        return {"endpoint-recording", "Endpoint host bridge test adapter", true, ""};
    }

    std::vector<axent::Capability> capabilities() const override
    {
        return {};
    }

    std::vector<axent::DeviceSnapshot> discover() override
    {
        return {
            device("physical-primary", "endpoint/mock-primary", true),
            device("physical-offline", "endpoint/mock-offline", false),
        };
    }

    axent::ControlResult call(const std::string&,
                              const std::string&,
                              const nlohmann::json&) override
    {
        return {axent::ControlStatus::InternalError, {{"error", "legacy call used"}}};
    }

    axent::ControlOperationPtr call_async(
        const axent::AdapterControlRequest& request,
        axent::ControlCallOptions) override
    {
        if (request.method != "hold") {
            std::lock_guard<std::mutex> lock(state_->mutex);
            state_->requests.push_back(request);
            return axent::make_completed_control_operation(
                {axent::ControlStatus::Ok, {{"path", "endpoint-routed"}}});
        }

        auto pending = std::make_shared<PendingCall>();
        pending->request = request;
        const auto cancellation_state = cancellation_state_;
        pending->source.set_cancel_handler([cancellation_state]() {
            std::lock_guard<std::mutex> lock(cancellation_state->mutex);
            ++cancellation_state->cancellations;
            cancellation_state->cv.notify_all();
        });
        {
            std::lock_guard<std::mutex> lock(state_->mutex);
            state_->requests.push_back(request);
            state_->held.push_back(pending);
        }
        return pending->source.operation();
    }

    axent::ControlResult start_firmware_update(
        const std::string&, const std::string&) override
    {
        return {axent::ControlStatus::Unavailable, nlohmann::json::object()};
    }

    std::vector<axent::AdapterControlRequest> requests() const
    {
        std::lock_guard<std::mutex> lock(state_->mutex);
        return state_->requests;
    }

    void complete_held()
    {
        std::vector<std::shared_ptr<PendingCall>> held;
        {
            std::lock_guard<std::mutex> lock(state_->mutex);
            held.swap(state_->held);
        }
        for (const auto& pending : held) {
            pending->source.complete(
                {axent::ControlStatus::Ok, {{"path", "endpoint-held"}}});
        }
    }

private:
    struct PendingCall {
        axent::AdapterControlRequest request;
        axent::ControlOperationSource source;
    };

    struct State {
        std::mutex mutex;
        std::vector<axent::AdapterControlRequest> requests;
        std::vector<std::shared_ptr<PendingCall>> held;
    };

    static axent::DeviceSnapshot device(
        std::string id, std::string endpoint_id, bool online)
    {
        axent::DeviceSnapshot snapshot;
        snapshot.id = std::move(id);
        snapshot.adapter = "endpoint-recording";
        snapshot.endpoint_id = std::move(endpoint_id);
        snapshot.connection.online = online;
        snapshot.connection.transport = "test";
        return snapshot;
    }

    std::shared_ptr<State> state_ = std::make_shared<State>();
    std::shared_ptr<EndpointCancellationState> cancellation_state_;
};

std::optional<axent::MediaFrame> wait_for_media_frame(axent::MediaConsumer& consumer)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (std::chrono::steady_clock::now() < deadline) {
        auto frame = consumer.read();
        if (frame.has_value()) {
            return frame;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return std::nullopt;
}

class RecordingMediaSink final : public axent::IMediaFrameSink {
public:
    void on_media_frame(axent::MediaFrame frame) override
    {
        std::unique_lock<std::mutex> lock(mutex_);
        frames_.push_back(std::move(frame));
        if (block_next_frame_) {
            block_next_frame_ = false;
            frame_blocked_ = true;
            cv_.notify_all();
            cv_.wait(lock, [this]() {
                return unblock_frames_;
            });
        }
        cv_.notify_all();
    }

    void on_media_event(axent::MediaEvent event) override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        events_.push_back(event);
        cv_.notify_all();
    }

    void block_next_frame()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        block_next_frame_ = true;
        frame_blocked_ = false;
        unblock_frames_ = false;
    }

    bool wait_for_frames(std::size_t minimum)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, std::chrono::seconds(1), [this, minimum]() {
            return frames_.size() >= minimum;
        });
    }

    bool wait_for_frame_blocked()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, std::chrono::seconds(1), [this]() {
            return frame_blocked_;
        });
    }

    void unblock_frames()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            unblock_frames_ = true;
        }
        cv_.notify_all();
    }

    bool wait_for_closed()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, std::chrono::seconds(1), [this]() {
            for (const auto& event : events_) {
                if (event.kind == axent::MediaEventKind::Closed) {
                    return true;
                }
            }
            return false;
        });
    }

    bool has_drop_event() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& event : events_) {
            if (event.kind == axent::MediaEventKind::Dropped) {
                return true;
            }
        }
        return false;
    }

    std::vector<axent::MediaFrame> frames() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return frames_;
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<axent::MediaFrame> frames_;
    std::vector<axent::MediaEvent> events_;
    bool block_next_frame_ = false;
    bool frame_blocked_ = false;
    bool unblock_frames_ = false;
};

enum class StreamCallbackKind {
    Opened,
    Frame,
    Closed,
    DeliveryDropped,
    SubscriptionClosed,
};

struct StreamCallbackRecord {
    StreamCallbackKind kind = StreamCallbackKind::Frame;
    axent::StreamKey key;
    axent::MediaStreamDescriptor descriptor;
    axent::MediaFrame frame;
    axent::MediaDeliveryEvent delivery;
    std::thread::id thread_id;
};

class RecordingMediaStreamSink final : public axent::IMediaStreamSink {
public:
    void on_media_stream_event(axent::MediaStreamEvent event) override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        StreamCallbackRecord record;
        record.kind = event.kind == axent::MediaStreamEventKind::Opened
            ? StreamCallbackKind::Opened
            : StreamCallbackKind::Closed;
        record.key = event.descriptor.key;
        record.descriptor = std::move(event.descriptor);
        record.thread_id = std::this_thread::get_id();
        records_.push_back(std::move(record));
        cv_.notify_all();
    }

    void on_media_stream_frame(axent::MediaFrame frame) override
    {
        std::function<void()> hook;
        std::unique_lock<std::mutex> lock(mutex_);
        StreamCallbackRecord record;
        record.kind = StreamCallbackKind::Frame;
        record.key = axent::stream_key(frame);
        record.frame = std::move(frame);
        record.thread_id = std::this_thread::get_id();
        records_.push_back(std::move(record));
        hook = frame_hook_;
        if (block_next_frame_) {
            block_next_frame_ = false;
            frame_blocked_ = true;
            cv_.notify_all();
            cv_.wait(lock, [this]() { return unblock_frames_; });
        }
        cv_.notify_all();
        lock.unlock();
        if (hook) {
            hook();
        }
    }

    void on_media_delivery_event(axent::MediaDeliveryEvent event) override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        StreamCallbackRecord record;
        record.kind = event.kind == axent::MediaDeliveryEventKind::DeliveryDropped
            ? StreamCallbackKind::DeliveryDropped
            : StreamCallbackKind::SubscriptionClosed;
        record.delivery = event;
        record.thread_id = std::this_thread::get_id();
        records_.push_back(std::move(record));
        cv_.notify_all();
    }

    void set_frame_hook(std::function<void()> hook)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        frame_hook_ = std::move(hook);
    }

    void block_next_frame()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        block_next_frame_ = true;
        frame_blocked_ = false;
        unblock_frames_ = false;
    }

    bool wait_for_frame_blocked()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, std::chrono::seconds(1), [this]() {
            return frame_blocked_;
        });
    }

    void unblock_frames()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            unblock_frames_ = true;
        }
        cv_.notify_all();
    }

    bool wait_for_records(std::size_t minimum)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, std::chrono::seconds(1), [this, minimum]() {
            return records_.size() >= minimum;
        });
    }

    std::vector<StreamCallbackRecord> records() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return records_;
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<StreamCallbackRecord> records_;
    std::function<void()> frame_hook_;
    bool block_next_frame_ = false;
    bool frame_blocked_ = false;
    bool unblock_frames_ = false;
};

class CallbackExitGate {
public:
    void enter_and_wait()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        entered_ = true;
        cv_.notify_all();
        cv_.wait(lock, [this]() { return released_; });
    }

    bool wait_until_entered()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, std::chrono::seconds(1), [this]() {
            return entered_;
        });
    }

    void release()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            released_ = true;
        }
        cv_.notify_all();
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    bool entered_ = false;
    bool released_ = false;
};

} // namespace

int main()
{
    axent::AxentHost host;
    axent::SessionAcquireRequest stopped_request;
    stopped_request.client_id = "stopped-client";
    stopped_request.device_id = "mock-device-001";
    stopped_request.media = true;
    const auto stopped_lease = host.acquire_session(stopped_request);
    require(!stopped_lease.acquired, "stopped host should not acquire");
    require(stopped_lease.reason == "host not running", "stopped acquire reason mismatch");

    require(host.create_media_consumer("missing-session", {}) == nullptr,
            "missing session should not create consumer");
    auto missing_frame = make_media_frame(99);
    require(!host.publish_media_frame("missing-session", missing_frame),
            "missing session publish should fail");
    const auto missing_call = host.call("missing-session", "status.get", {});
    require(missing_call.status == axent::ControlStatus::Unavailable,
            "call before start should return unavailable");

    bool broker_threw = false;
    try {
        (void)host.broker();
    } catch (const std::logic_error&) {
        broker_threw = true;
    }
    require(broker_threw, "broker should throw before start");

    auto inventory_discovery_count = std::make_shared<int>(0);
    axent::AxentHost inventory_host;
    axent::AxentHostOptions inventory_options;
    inventory_options.enable_mock_adapter = false;
    inventory_options.enable_axtp_adapter = true;
    inventory_options.axtp_adapter_factory = [inventory_discovery_count](
                                              axent::AxtpAdapterConfig) {
        return std::make_unique<StatefulInventoryAdapter>(inventory_discovery_count);
    };
    require(inventory_host.start(std::move(inventory_options)),
            "inventory host should start");

    const auto find_inventory_device = [](const std::vector<axent::DeviceSnapshot>& devices,
                                          const char* id) -> const axent::DeviceSnapshot& {
        const auto device = std::find_if(
            devices.begin(), devices.end(), [id](const axent::DeviceSnapshot& candidate) {
                return candidate.id == id;
            });
        require(device != devices.end(), "refreshed inventory must contain expected device");
        return *device;
    };

    const auto refreshed = inventory_host.refresh_devices();
    require(find_inventory_device(refreshed, "device-a").connection.online == false,
            "missing A must be retained offline");
    require(find_inventory_device(refreshed, "device-b").connection.online,
            "B must remain online");
    require(find_inventory_device(refreshed, "device-c").connection.online,
            "new C must be inserted");

    const auto restored = inventory_host.refresh_devices();
    require(find_inventory_device(restored, "device-a").endpoint_id == "ep-device-a",
            "reappearance must preserve the stable Endpoint");

    inventory_host.stop();
    const auto discovery_count_before_stopped_refresh = *inventory_discovery_count;
    require(inventory_host.refresh_devices().empty(),
            "stopped Host refresh should return an empty list");
    require(*inventory_discovery_count == discovery_count_before_stopped_refresh,
            "stopped Host refresh must not call discovery");

    auto batch_discovery_count = std::make_shared<int>(0);
    axent::AxentHost batch_inventory_host;
    axent::AxentHostOptions batch_inventory_options;
    batch_inventory_options.enable_mock_adapter = false;
    batch_inventory_options.enable_axtp_adapter = true;
    batch_inventory_options.axtp_adapter_factory = [batch_discovery_count](
                                                    axent::AxtpAdapterConfig) {
        return std::make_unique<BatchInventoryAdapter>(batch_discovery_count);
    };
    require(batch_inventory_host.start(std::move(batch_inventory_options)),
            "batch inventory host should start");

    const auto migrated_inventory = batch_inventory_host.refresh_devices();
    require(std::none_of(
                migrated_inventory.begin(),
                migrated_inventory.end(),
                [](const axent::DeviceSnapshot& device) {
                    return device.id == "stable-path-old";
                }),
            "stable Endpoint migration must remove the stale same-adapter owner");
    require(find_inventory_device(migrated_inventory, "stable-path-new").connection.online,
            "stable Endpoint migration must retain the current provider-local ID");
    require(!find_inventory_device(migrated_inventory, "unrelated").connection.online,
            "missing unrelated devices must remain retained offline");
    require(!find_inventory_device(migrated_inventory, "legacy-old").connection.online &&
                find_inventory_device(migrated_inventory, "legacy-new").connection.online,
            "legacy empty-Endpoint devices must reconcile strictly by adapter and local ID");
    require(find_inventory_device(migrated_inventory, "endpoint-change").endpoint_id ==
                "endpoint/original",
            "same physical owner must not change between non-empty Endpoints");

    const auto conflicted_inventory = batch_inventory_host.refresh_devices();
    require(find_inventory_device(conflicted_inventory, "conflict-a").connection.online &&
                find_inventory_device(conflicted_inventory, "conflict-b").connection.online,
            "simultaneous current Endpoint claims must all remain visible");
    const auto conflicted_call = batch_inventory_host.call_endpoint({
        "endpoint/controller",
        "endpoint/conflict",
        "status.get",
        nlohmann::json::object(),
    });
    require(conflicted_call != nullptr &&
                conflicted_call->wait().status == axent::ControlStatus::Unavailable,
            "simultaneous Endpoint claims must make Host routing fail closed");

    const auto recovered_inventory = batch_inventory_host.refresh_devices();
    require(std::none_of(
                recovered_inventory.begin(),
                recovered_inventory.end(),
                [](const axent::DeviceSnapshot& device) {
                    return device.id == "conflict-a";
                }) &&
                find_inventory_device(recovered_inventory, "conflict-b").connection.online,
            "collapsed same-adapter conflict must remove the absent stale claim");
    const auto recovered_call = batch_inventory_host.call_endpoint({
        "endpoint/controller",
        "endpoint/conflict",
        "status.get",
        nlohmann::json::object(),
    });
    const auto recovered_result = recovered_call != nullptr
        ? recovered_call->wait()
        : axent::ControlResult{axent::ControlStatus::InternalError,
                               nlohmann::json::object()};
    require(recovered_call != nullptr &&
                recovered_result.status == axent::ControlStatus::Ok &&
                recovered_result.body.at("deviceId") == "conflict-b",
            "collapsed Endpoint conflict must restore the unique Host route");

    const auto repeated_recovery_inventory = batch_inventory_host.refresh_devices();
    require(repeated_recovery_inventory.size() == recovered_inventory.size() &&
                std::none_of(
                    repeated_recovery_inventory.begin(),
                    repeated_recovery_inventory.end(),
                    [](const axent::DeviceSnapshot& device) {
                        return device.id == "conflict-a";
                    }) &&
                find_inventory_device(repeated_recovery_inventory, "conflict-b")
                        .endpoint_id == "endpoint/conflict",
            "repeated effective empty-Endpoint Host refresh must be idempotent");

    const auto duplicate_row_inventory = batch_inventory_host.refresh_devices();
    require(std::count_if(
                duplicate_row_inventory.begin(),
                duplicate_row_inventory.end(),
                [](const axent::DeviceSnapshot& device) {
                    return device.id == "duplicate-row";
                }) == 1,
            "Host refresh must deduplicate identical rows for one physical key");
    const auto duplicate_row_call = batch_inventory_host.call_endpoint({
        "endpoint/controller",
        "endpoint/duplicate-row",
        "status.get",
        nlohmann::json::object(),
    });
    require(duplicate_row_call != nullptr &&
                duplicate_row_call->wait().status == axent::ControlStatus::Ok,
            "deduplicated Host discovery row must retain a unique Endpoint route");

    const auto invalid_order_inventory = batch_inventory_host.refresh_devices();
    require(std::none_of(
                invalid_order_inventory.begin(),
                invalid_order_inventory.end(),
                [](const axent::DeviceSnapshot& device) {
                    return device.id == "invalid-key";
                }) &&
                !find_inventory_device(invalid_order_inventory, "invalid-stale")
                     .connection.online,
            "invalid same-key Endpoint rows must not choose a winner or erase stale owners");
    const auto reverse_invalid_order_inventory = batch_inventory_host.refresh_devices();
    require(std::none_of(
                reverse_invalid_order_inventory.begin(),
                reverse_invalid_order_inventory.end(),
                [](const axent::DeviceSnapshot& device) {
                    return device.id == "invalid-key";
                }) &&
                !find_inventory_device(reverse_invalid_order_inventory, "invalid-stale")
                     .connection.online,
            "invalid same-key Endpoint rejection must be independent of row order");

    const auto rejected_change_conflict_inventory =
        batch_inventory_host.refresh_devices();
    require(find_inventory_device(
                rejected_change_conflict_inventory, "rejected-change-a")
                    .connection.online &&
                find_inventory_device(
                    rejected_change_conflict_inventory, "rejected-change-b")
                    .connection.online,
            "Host rejected-change fixture must begin with two current Endpoint owners");

    const auto rejected_change_recovered_inventory =
        batch_inventory_host.refresh_devices();
    require(std::none_of(
                rejected_change_recovered_inventory.begin(),
                rejected_change_recovered_inventory.end(),
                [](const axent::DeviceSnapshot& device) {
                    return device.id == "rejected-change-a";
                }) &&
                find_inventory_device(
                    rejected_change_recovered_inventory, "rejected-change-b")
                        .endpoint_id == "endpoint/rejected-original" &&
                !find_inventory_device(
                    rejected_change_recovered_inventory, "rejected-target-owner")
                     .connection.online,
            "rejected Host change must recover the old Endpoint without cleaning its target");
    const auto rejected_change_recovered_call = batch_inventory_host.call_endpoint({
        "endpoint/controller",
        "endpoint/rejected-original",
        "status.get",
        nlohmann::json::object(),
    });
    require(rejected_change_recovered_call != nullptr &&
                rejected_change_recovered_call->wait().status ==
                    axent::ControlStatus::Ok,
            "rejected Host change must restore the unique authoritative route");
    const auto rejected_target_call = batch_inventory_host.call_endpoint({
        "endpoint/controller",
        "endpoint/rejected-target",
        "status.get",
        nlohmann::json::object(),
    });
    require(rejected_target_call != nullptr &&
                rejected_target_call->wait().status ==
                    axent::ControlStatus::Unavailable,
            "rejected attempted Endpoint must not erase its retained offline owner");

    const auto repeated_rejected_change_inventory =
        batch_inventory_host.refresh_devices();
    require(repeated_rejected_change_inventory.size() ==
                rejected_change_recovered_inventory.size() &&
                find_inventory_device(
                    repeated_rejected_change_inventory, "rejected-change-b")
                        .endpoint_id == "endpoint/rejected-original",
            "repeated rejected Host change recovery must be idempotent");

    const auto simultaneous_rejected_change_inventory =
        batch_inventory_host.refresh_devices();
    require(find_inventory_device(
                simultaneous_rejected_change_inventory, "rejected-change-b")
                    .connection.online &&
                find_inventory_device(
                    simultaneous_rejected_change_inventory, "rejected-change-c")
                    .connection.online,
            "simultaneous authoritative Endpoint owners must remain visible");
    const auto simultaneous_rejected_change_call = batch_inventory_host.call_endpoint({
        "endpoint/controller",
        "endpoint/rejected-original",
        "status.get",
        nlohmann::json::object(),
    });
    require(simultaneous_rejected_change_call != nullptr &&
                simultaneous_rejected_change_call->wait().status ==
                    axent::ControlStatus::Unavailable,
            "simultaneous authoritative Endpoint owners must keep Host routing conflicted");
    batch_inventory_host.stop();

    EndpointRecordingAdapter* endpoint_adapter = nullptr;
    auto endpoint_cancellations = std::make_shared<EndpointCancellationState>();
    axent::AxentHost endpoint_host;
    axent::AxentHostOptions endpoint_options;
    endpoint_options.enable_mock_adapter = false;
    endpoint_options.enable_axtp_adapter = true;
    endpoint_options.axtp_adapter_factory = [&endpoint_adapter, endpoint_cancellations](
                                              axent::AxtpAdapterConfig) {
        auto adapter = std::make_unique<EndpointRecordingAdapter>(endpoint_cancellations);
        endpoint_adapter = adapter.get();
        return adapter;
    };
    require(endpoint_host.start(std::move(endpoint_options)),
            "endpoint host should start");
    require(endpoint_adapter != nullptr, "endpoint adapter should be installed");

    const auto endpoint_operation = endpoint_host.call_endpoint({
        "ep-nearcast-source",
        "endpoint/mock-primary",
        "status.get",
        nlohmann::json::object(),
    });
    require(endpoint_operation != nullptr &&
                endpoint_operation->wait().status == axent::ControlStatus::Ok,
            "endpoint call should succeed without a Host session");
    const auto endpoint_requests = endpoint_adapter->requests();
    require(endpoint_requests.size() == 1 &&
                endpoint_requests.front().device_id == "physical-primary" &&
                endpoint_requests.front().source_endpoint_id == "ep-nearcast-source" &&
                endpoint_requests.front().destination_endpoint_id == "endpoint/mock-primary" &&
                endpoint_requests.front().method == "status.get" &&
                endpoint_requests.front().params == nlohmann::json::object() &&
                !endpoint_requests.front().params.contains("deviceId") &&
                !endpoint_requests.front().params.contains("serialNumber"),
            "endpoint call must preserve Endpoint IDs without injecting device selectors");

    const auto unknown_endpoint = endpoint_host.call_endpoint({
        "ep-nearcast-source",
        "endpoint/missing",
        "status.get",
        nlohmann::json::object(),
    });
    require(unknown_endpoint != nullptr &&
                unknown_endpoint->wait().status == axent::ControlStatus::NotFound,
            "unknown endpoint should fail closed as NotFound");
    const auto offline_endpoint = endpoint_host.call_endpoint({
        "ep-nearcast-source",
        "endpoint/mock-offline",
        "status.get",
        nlohmann::json::object(),
    });
    require(offline_endpoint != nullptr &&
                offline_endpoint->wait().status == axent::ControlStatus::Unavailable,
            "offline endpoint should fail closed as Unavailable");
    for (const auto& invalid_request : std::vector<axent::EndpointControlRequest>{
             {"", "endpoint/mock-primary", "status.get", nlohmann::json::object()},
             {"ep-nearcast-source", "", "status.get", nlohmann::json::object()},
             {"ep-nearcast-source", "endpoint/mock-primary", "", nlohmann::json::object()},
         }) {
        const auto invalid_operation = endpoint_host.call_endpoint(invalid_request);
        require(invalid_operation != nullptr &&
                    invalid_operation->wait().status == axent::ControlStatus::InvalidArgument,
                "endpoint calls require source, destination, and method");
    }

    axent::SessionAcquireRequest endpoint_collision_session_request;
    endpoint_collision_session_request.client_id = "endpoint-collision-client";
    endpoint_collision_session_request.device_id = "physical-primary";
    const auto endpoint_collision_session =
        endpoint_host.acquire_session(endpoint_collision_session_request);
    require(endpoint_collision_session.acquired,
            "collision test should acquire a Host session");
    axent::DeviceSnapshot endpoint_collision_device;
    endpoint_collision_device.id = "physical-session-collision";
    endpoint_collision_device.adapter = "endpoint-recording";
    endpoint_collision_device.endpoint_id = endpoint_collision_session.session_id;
    endpoint_collision_device.connection.online = true;
    endpoint_host.upsert_device(endpoint_collision_device);
    const auto endpoint_collision_operation = endpoint_host.call_endpoint({
        "ep-nearcast-source",
        endpoint_collision_session.session_id,
        "hold",
        nlohmann::json::object(),
    });
    require(endpoint_collision_operation != nullptr && !endpoint_collision_operation->ready(),
            "endpoint operation should remain pending before unrelated session release");
    endpoint_host.release_session(
        endpoint_collision_session.session_id, "session and endpoint key collision");
    require(!endpoint_collision_operation->ready(),
            "releasing a matching session ID must not cancel an endpoint operation");
    endpoint_adapter->complete_held();
    require(endpoint_collision_operation->wait().status == axent::ControlStatus::Ok,
            "endpoint operation should complete after its own adapter releases it");

    {
        const auto discarded_operation = endpoint_host.call_endpoint({
            "ep-nearcast-source", "endpoint/mock-primary", "hold", nlohmann::json::object()});
        require(discarded_operation != nullptr && !discarded_operation->ready(),
                "Adapter should retain the pending endpoint operation");
    }
    endpoint_host.stop();
    require(endpoint_cancellations->wait_for_cancellations(1),
            "Host stop should cancel a producer-owned endpoint operation after caller release");
    const auto stopped_endpoint = endpoint_host.call_endpoint({
        "ep-nearcast-source", "endpoint/mock-primary", "status.get", nlohmann::json::object()});
    require(stopped_endpoint != nullptr &&
                stopped_endpoint->wait().status == axent::ControlStatus::Unavailable,
            "stopped Host endpoint calls should be unavailable");

    axent::AxentHostOptions options;
    options.enable_mock_adapter = true;
    options.enable_axtp_adapter = false;
    require(host.start(options), "host should start");

    const auto devices = host.discover_devices();
    require(devices.size() >= 1, "host should discover at least the mock device");
    require(devices[0].id == "mock-device-001", "mock device id mismatch");
    host.upsert_device(make_second_mock_device());
    const auto devices_after_upsert = host.discover_devices();
    require(devices_after_upsert.size() == 2, "host should allow upserting a second device");

    auto conflicting_device = make_second_mock_device();
    conflicting_device.id = "mock-device-conflict";
    conflicting_device.endpoint_id = "endpoint/mock-primary";
    host.upsert_device(conflicting_device);
    const auto devices_after_conflict = host.discover_devices();
    require(devices_after_conflict.size() == 2,
            "host must not store a conflicting endpoint binding");
    require(devices_after_conflict.front().endpoint_id == "endpoint/mock-primary",
            "host conflict must preserve the original endpoint owner");

    axent::DeviceSnapshot host_serial_alias;
    host_serial_alias.id = "host-serial-alias-owner";
    host_serial_alias.adapter = "external";
    host_serial_alias.identity.serial_number = "mock-device-001";
    host_serial_alias.endpoint_id = "endpoint/host-serial-alias";
    host_serial_alias.connection.online = true;
    host.upsert_device(host_serial_alias);
    axent::SessionAcquireRequest host_namespace_request;
    host_namespace_request.client_id = "host-namespace-test";
    host_namespace_request.device_id = "mock-device-001";
    const auto host_namespace_lease =
        host.acquire_session(host_namespace_request);
    require(host_namespace_lease.acquired,
            "Host lease should select the unique provider-local device ID");
    const auto host_namespace_call = host.call(
        host_namespace_lease.session_id, "status.get", {});
    require(host_namespace_call.status == axent::ControlStatus::Ok &&
                host_namespace_call.body.at("health") == "ok",
            "leased Host call must retain provider-local selector semantics");
    host.release_session(
        host_namespace_lease.session_id, "namespace regression complete");

    // Host lease ownership is device-scoped even when the concrete AXTP
    // adapter is supplied later by an embedded product host.  Keeping this
    // gate test independent of transport setup makes a regression back to a
    // single global AXTP owner fail immediately.
    const auto make_axtp_snapshot = [](std::string id, std::string serial) {
        axent::DeviceSnapshot device;
        device.id = std::move(id);
        device.adapter = "axtp";
        device.identity.serial_number = std::move(serial);
        device.connection.online = true;
        device.connection.transport = "hid";
        device.status.health = "ready";
        return device;
    };
    const auto isolated_axtp_a = make_axtp_snapshot(
        "hid:0581:2582:HOST-ISOLATION-A", "HOST-ISOLATION-A");
    const auto isolated_axtp_b = make_axtp_snapshot(
        "hid:0581:2582:HOST-ISOLATION-B", "HOST-ISOLATION-B");
    host.upsert_device(isolated_axtp_a);
    host.upsert_device(isolated_axtp_b);
    axent::SessionAcquireRequest isolated_axtp_request;
    isolated_axtp_request.client_id = "host-isolation-a";
    isolated_axtp_request.device_id = isolated_axtp_a.id;
    const auto isolated_axtp_lease_a = host.acquire_session(isolated_axtp_request);
    isolated_axtp_request.client_id = "host-isolation-b";
    isolated_axtp_request.device_id = isolated_axtp_b.id;
    const auto isolated_axtp_lease_b = host.acquire_session(isolated_axtp_request);
    require(isolated_axtp_lease_a.acquired && isolated_axtp_lease_b.acquired,
            "distinct AXTP devices should hold concurrent Host leases");
    host.release_session(isolated_axtp_lease_a.session_id, "host isolation complete");
    host.release_session(isolated_axtp_lease_b.session_id, "host isolation complete");

    axent::SessionAcquireRequest unknown_device_request;
    unknown_device_request.client_id = "nearcast-test";
    unknown_device_request.device_id = "missing-device";
    unknown_device_request.media = true;
    const auto unknown_device_lease = host.acquire_session(unknown_device_request);
    require(!unknown_device_lease.acquired, "unknown device should not acquire");
    require(unknown_device_lease.reason == "device not found", "unknown device reason mismatch");
    require(host.create_media_consumer("missing-session", {}) == nullptr,
            "running host missing session should not create consumer");
    const auto missing_session_call = host.call("missing-session", "status.get", {});
    require(missing_session_call.status == axent::ControlStatus::NotFound,
            "missing session call should return not found");

    axent::SessionAcquireRequest request;
    request.client_id = "nearcast-test";
    request.device_id = "mock-device-001";
    request.media = true;
    const auto lease = host.acquire_session(request);
    require(lease.acquired, "media lease should be acquired");
    require(lease.media, "media lease should be marked media");
    require(!lease.session_id.empty(), "session id should be assigned");

    auto same_client_control_request = request;
    same_client_control_request.media = false;
    const auto same_client_control_lease = host.acquire_session(same_client_control_request);
    require(same_client_control_lease.acquired, "same client control lease should be acquired");
    require(!same_client_control_lease.media, "control lease should not be marked media");
    require(host.create_media_consumer(same_client_control_lease.session_id, {}) == nullptr,
            "control lease should not create media consumer");
    auto control_frame = make_media_frame(77);
    require(!host.publish_media_frame(same_client_control_lease.session_id, control_frame),
            "control lease publish should fail");
    require(host.subscribe_media(same_client_control_lease.session_id,
                                 std::make_shared<RecordingMediaSink>()) == nullptr,
            "control lease should not create media subscription");

    host.release_session(same_client_control_lease.session_id, "control session done");

    auto denied_request = request;
    denied_request.client_id = "second-renderer";
    const auto denied = host.acquire_session(denied_request);
    require(!denied.acquired, "second media owner should still be denied");
    require(denied.reason == "media lease busy", "denied reason mismatch");

    auto second_device_media_request = denied_request;
    second_device_media_request.device_id = "mock-device-002";
    const auto second_device_media_lease = host.acquire_session(second_device_media_request);
    require(second_device_media_lease.acquired, "second device media lease should be acquired");
    require(second_device_media_lease.media, "second device lease should be marked media");

    auto denied_second_device_request = second_device_media_request;
    denied_second_device_request.client_id = "third-renderer";
    const auto denied_second_device = host.acquire_session(denied_second_device_request);
    require(!denied_second_device.acquired, "second media owner for second device should be denied");
    require(denied_second_device.reason == "media lease busy", "second device denied reason mismatch");

    host.release_session(second_device_media_lease.session_id, "second device media done");

    auto early_frame = make_media_frame(42);
    require(!host.publish_media_frame(lease.session_id, early_frame),
            "publish before media consumer should fail");

    auto provider_sink = std::make_shared<RecordingMediaStreamSink>();
    auto provider_subscription =
        host.subscribe_media_stream(lease.session_id, provider_sink);
    require(provider_subscription != nullptr && provider_sink->records().empty(),
            "custom provider subscription should begin without a descriptor snapshot");
    axent::MediaStreamDescriptor provider_descriptor;
    provider_descriptor.key = {"adapter-owned-placeholder", 0x3001, 1};
    provider_descriptor.device_id = "adapter-owned-placeholder";
    provider_descriptor.kind = axent::MediaKind::Video;
    provider_descriptor.codec = axent::MediaCodec::H264;
    require(host.publish_media_stream_event(
                lease.session_id,
                {axent::MediaStreamEventKind::Opened, provider_descriptor}),
            "custom provider should publish an Opened lifecycle event through Host");
    auto provider_frame = make_media_frame(43);
    provider_frame.stream_id = provider_descriptor.key.stream_id;
    provider_frame.generation = provider_descriptor.key.generation;
    require(host.publish_media_frame(lease.session_id, provider_frame) &&
                provider_sink->wait_for_records(2),
            "custom provider frame should follow its Host-bound descriptor");
    require(host.publish_media_stream_event(
                lease.session_id,
                {axent::MediaStreamEventKind::Closed, provider_descriptor}) &&
                provider_sink->wait_for_records(3),
            "custom provider should close its stream through Host");
    const auto provider_records = provider_sink->records();
    require(provider_records[0].key.session_id == lease.session_id &&
                provider_records[0].descriptor.device_id == lease.device_id &&
                provider_records[1].kind == StreamCallbackKind::Frame &&
                provider_records[1].key == provider_records[0].key &&
                provider_records[2].kind == StreamCallbackKind::Closed &&
                provider_records[2].key == provider_records[0].key,
            "Host must bind and order custom provider lifecycle and frames");
    provider_subscription->cancel();

    require(host.subscribe_media("missing-session", std::make_shared<RecordingMediaSink>()) == nullptr,
            "missing session should not create media subscription");

    auto push_sink = std::make_shared<RecordingMediaSink>();
    axent::MediaSubscriptionOptions push_options;
    push_options.max_frames = 4;
    auto push_subscription = host.subscribe_media(lease.session_id, push_sink, push_options);
    require(push_subscription != nullptr, "media subscription should be created");

    auto push_frame = make_media_frame(100);
    require(host.publish_media_frame(lease.session_id, push_frame),
            "push-only publish should succeed");
    auto immediate_push_frames = push_sink->frames();
    require(immediate_push_frames.size() == 1, "direct push sink should receive before publish returns");
    const auto push_stats = push_subscription->stats();
    require(push_stats.received_frames == 1, "direct push received count mismatch");
    require(push_stats.delivered_frames == 1, "direct push delivered count mismatch");
    require(push_stats.queued_frames == 0, "direct push should not queue frames");
    require(push_stats.queued_bytes == 0, "direct push should not queue bytes");
    require(push_sink->wait_for_frames(1), "push sink should receive a frame without relay");
    auto push_frames = push_sink->frames();
    require(push_frames.front().sequence_id == 100, "push sink sequence mismatch");
    require(push_frames.front().session_id == lease.session_id, "push sink session mismatch");

    axent::MediaRelayOptions relay_options;
    relay_options.max_frames = 1;
    auto consumer = host.create_media_consumer(lease.session_id, relay_options);
    require(consumer != nullptr, "media consumer should be created");

    auto frame = make_media_frame(1);
    frame.session_id = lease.session_id;
    require(host.publish_media_frame(lease.session_id, frame), "publish should succeed");
    auto second_frame = make_media_frame(2);
    require(host.publish_media_frame(lease.session_id, second_frame), "second publish should succeed");

    const auto received = consumer->read();
    require(received.has_value(), "consumer should read a frame");
    require(received->sequence_id == 2, "custom relay should retain newest frame");
    require(received->session_id == lease.session_id, "received session mismatch");
    require(axent::has_flag(received->flags, axent::MediaFrameFlag::Discontinuity),
            "retained frame should mark discontinuity");
    const auto relay_stats = consumer->stats();
    require(relay_stats.published_frames == 2, "relay published count mismatch");
    require(relay_stats.dropped_frames == 1, "relay dropped count mismatch");
    require(push_sink->wait_for_frames(3), "push sink should keep receiving alongside relay");

    auto slow_sink = std::make_shared<RecordingMediaSink>();
    axent::MediaSubscriptionOptions slow_options;
    slow_options.dispatch = axent::MediaSubscriptionDispatch::AsyncQueued;
    slow_options.max_frames = 1;
    auto slow_subscription = host.subscribe_media(lease.session_id, slow_sink, slow_options);
    require(slow_subscription != nullptr, "slow media subscription should be created");
    slow_sink->block_next_frame();
    auto slow_first_frame = make_media_frame(200);
    require(host.publish_media_frame(lease.session_id, slow_first_frame),
            "slow subscription first publish should succeed");
    require(slow_sink->wait_for_frame_blocked(), "slow sink should block on first frame");

    const auto publish_start = std::chrono::steady_clock::now();
    for (std::uint64_t sequence = 201; sequence <= 205; ++sequence) {
        require(host.publish_media_frame(lease.session_id, make_media_frame(sequence)),
                "publish should not require relay read or sink progress");
    }
    const auto publish_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - publish_start);
    require(publish_elapsed < std::chrono::milliseconds(200),
            "slow sink should not block media publishing");

    slow_sink->unblock_frames();
    require(slow_sink->wait_for_frames(2), "slow sink should receive latest queued frame after unblock");
    require(slow_sink->has_drop_event(), "slow sink should observe a drop event");
    const auto slow_stats = slow_subscription->stats();
    require(slow_stats.received_frames >= 6, "slow subscription received count mismatch");
    require(slow_stats.dropped_frames >= 4, "slow subscription should drop stale frames");
    auto slow_frames = slow_sink->frames();
    require(slow_frames.back().sequence_id == 205, "slow subscription should retain newest frame");
    require(axent::has_flag(slow_frames.back().flags, axent::MediaFrameFlag::Discontinuity),
            "slow subscription retained frame should mark discontinuity");

    const auto result = host.call(lease.session_id, "status.get", {});
    require(result.status == axent::ControlStatus::Ok, "host call should dispatch through broker");
    require(result.body.at("health") == "ok", "host call result mismatch");

    host.release_session(lease.session_id, "test complete");
    require(push_sink->wait_for_closed(), "push subscription should close on release");
    require(slow_sink->wait_for_closed(), "slow subscription should close on release");
    require(!host.publish_media_frame(lease.session_id, frame), "publish after release should fail");
    require(!consumer->read().has_value(), "consumer should not read after media release");

    const auto second_lease = host.acquire_session(denied_request);
    require(second_lease.acquired, "second media owner should acquire after release");
    require(second_lease.media, "second media lease should be marked media");
    auto stop_sink = std::make_shared<RecordingMediaSink>();
    auto stop_subscription = host.subscribe_media(second_lease.session_id, stop_sink);
    require(stop_subscription != nullptr,
            "stop quiescence subscription should be created");
    stop_sink->block_next_frame();
    std::thread stop_publisher([&]() {
        (void)host.publish_media_frame(second_lease.session_id, make_media_frame(300));
    });
    require(stop_sink->wait_for_frame_blocked(),
            "stop quiescence callback should become in-flight");
    std::atomic<bool> stop_finished{false};
    std::thread stop_thread([&]() {
        host.stop();
        stop_finished.store(true);
    });
    const auto stop_reset_deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (host.running() && std::chrono::steady_clock::now() < stop_reset_deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    const bool stop_waited_for_callback =
        !host.running() && !stop_finished.load();
    stop_sink->unblock_frames();
    stop_publisher.join();
    stop_thread.join();
    require(stop_waited_for_callback && stop_finished.load(),
            "Host stop must wait for an in-flight legacy media callback");
    const auto stopped_call = host.call(second_lease.session_id, "status.get", {});
    require(stopped_call.status == axent::ControlStatus::Unavailable,
            "call after stop should return unavailable");

    broker_threw = false;
    try {
        (void)host.broker();
    } catch (const std::logic_error&) {
        broker_threw = true;
    }
    require(broker_threw, "broker should throw after stop");

    axent::AxentHost real_host;
    ScriptedAxtpTransport* scripted = nullptr;
    ScriptedAxtpTransport* second_scripted = nullptr;
    axent::AxtpAdapter* real_adapter = nullptr;
    int transport_factory_calls = 0;
    axent::AxentHostOptions real_options;
    real_options.enable_mock_adapter = false;
    real_options.enable_axtp_adapter = true;
    real_options.axtp_adapter_factory = [&](axent::AxtpAdapterConfig config) {
        auto adapter = axent::testing::AxtpAdapterTestSeam::make(
            std::move(config),
            [&](const axent::transport::HidTransportOptions& options) {
                ++transport_factory_calls;
                auto transport = std::make_unique<ScriptedAxtpTransport>();
                if (options.serialNumber == "NA20-SECOND") {
                    second_scripted = transport.get();
                } else {
                    scripted = transport.get();
                }
                return transport;
            });
        real_adapter = adapter.get();
        return adapter;
    };
    require(real_host.start(std::move(real_options)), "real AXTP host should start");

    axent::DeviceSnapshot real_device;
    real_device.id = "hid:0581:2582:NA20-SERIAL";
    real_device.adapter = "axtp";
    real_device.identity.vendor = "Mostorm";
    real_device.identity.model = "NA20";
    real_device.identity.serial_number = "NA20-SERIAL";
    real_device.connection.online = true;
    real_device.connection.transport = "hid";
    real_device.status.health = "ready";
    real_host.upsert_device(real_device);

    axent::SessionAcquireRequest real_request;
    real_request.client_id = "nearcast-real";
    real_request.device_id = real_device.id;
    const auto real_lease = real_host.acquire_session(real_request);
    if(!real_lease.acquired)std::fprintf(stderr,"real lease diagnostic: status=%u reason=%s\n",static_cast<unsigned>(real_lease.status),real_lease.reason.c_str());
    require(real_lease.acquired, "real AXTP device lease should be acquired");
    const auto real_call = real_host.call(real_lease.session_id, "audio.getAlgorithmConfig", {});
    require(real_call.status == axent::ControlStatus::Ok, "real AXTP host call should dispatch");
    require(real_call.body.at("via") == "axtp", "real AXTP host call response mismatch");
    require(scripted != nullptr && scripted->saw_business_request,
            "real AXTP host call should use scripted transport");

    real_request.media = true;
    const auto media_lease = real_host.acquire_session(real_request);
    require(media_lease.acquired, "real AXTP media lease should be acquired");
    const auto initial_video_params = real_host.video_stream_params_state(
        media_lease.session_id);
    require(initial_video_params.session_id == media_lease.session_id &&
                initial_video_params.active_stream_id.has_value(),
            "Host video stream parameter state should bind the media lease session");
    auto video_params_subscription = real_host.subscribe_video_stream_params(
        media_lease.session_id,
        [](const axent::VideoStreamParamsState&) {});
    require(video_params_subscription != nullptr,
            "Host should expose an RAII video parameter subscription for a media lease");
    axent::VideoStreamParamsRequest invalid_video_params;
    invalid_video_params.frame_rate = 0;
    const auto invalid_video_params_result = real_host.set_video_stream_params(
        media_lease.session_id, invalid_video_params);
    require(invalid_video_params_result.status_code == 0x000A &&
                !invalid_video_params_result.accepted &&
                invalid_video_params_result.state.session_id == media_lease.session_id,
            "Host should preserve typed validation status and bind result session id");

    auto stream_sink = std::make_shared<RecordingMediaStreamSink>();
    auto stream_subscription =
        real_host.subscribe_media_stream(media_lease.session_id, stream_sink);
    require(stream_subscription != nullptr,
            "real media stream subscription should be created");
    require(stream_sink->wait_for_records(2),
            "late media stream subscription should replay active descriptors");
    const auto replayed_streams = stream_sink->records();
    require(replayed_streams.size() == 2,
            "video and audio descriptors should be replayed exactly once");
    require(replayed_streams[0].kind == StreamCallbackKind::Opened &&
                replayed_streams[1].kind == StreamCallbackKind::Opened,
            "descriptor replay must begin with Opened events");
    require(replayed_streams[0].key.session_id == media_lease.session_id &&
                replayed_streams[1].key.session_id == media_lease.session_id,
            "descriptor replay must bind the Host media session id");
    require(replayed_streams[0].key.stream_id == 0x1001 &&
                replayed_streams[0].key.generation != 0 &&
                replayed_streams[0].descriptor.kind == axent::MediaKind::Video &&
                replayed_streams[0].descriptor.codec == axent::MediaCodec::H264 &&
                replayed_streams[0].descriptor.width == 1920 &&
                replayed_streams[0].descriptor.height == 1080 &&
                replayed_streams[0].descriptor.stream_profile == "media.video",
            "video descriptor mapping mismatch");
    require(replayed_streams[1].key.stream_id == 0x2001 &&
                replayed_streams[1].key.generation != 0 &&
                replayed_streams[1].descriptor.kind == axent::MediaKind::Audio &&
                replayed_streams[1].descriptor.codec == axent::MediaCodec::Aac &&
                replayed_streams[1].descriptor.transport_format == "adts" &&
                replayed_streams[1].descriptor.sample_rate == 48000 &&
                replayed_streams[1].descriptor.channels == 2 &&
                replayed_streams[1].descriptor.stream_profile == "media.audio",
            "audio descriptor mapping mismatch");

    axent::DeviceSnapshot second_real_device = real_device;
    second_real_device.id = "hid:0581:2582:NA20-SECOND";
    second_real_device.identity.serial_number = "NA20-SECOND";
    real_host.upsert_device(second_real_device);
    axent::SessionAcquireRequest second_real_request;
    second_real_request.client_id = "nearcast-second-real";
    second_real_request.device_id = second_real_device.id;
    second_real_request.media = true;
    const auto second_real_lease = real_host.acquire_session(second_real_request);
    require(second_real_lease.acquired,
            "a second physical AXTP device should acquire concurrently");
    require(second_real_lease.status == axent::ControlStatus::Ok,
            "the second physical AXTP device should return typed Ok");
    require(second_scripted != nullptr && transport_factory_calls == 2,
            "each AXTP device should own an independent transport");
    const auto second_real_call = real_host.call(
        second_real_lease.session_id, "audio.getAlgorithmConfig", {});
    require(second_real_call.status == axent::ControlStatus::Ok &&
                second_scripted->saw_business_request,
            "the second AXTP device should dispatch on its own transport");
    require(real_host.transport_diagnostics(real_device.id).open &&
                real_host.transport_diagnostics(second_real_device.id).open,
            "Host diagnostics should report each physical AXTP session independently");
    auto second_stream_sink = std::make_shared<RecordingMediaStreamSink>();
    auto second_stream_subscription = real_host.subscribe_media_stream(
        second_real_lease.session_id, second_stream_sink);
    require(second_stream_subscription != nullptr &&
                second_stream_sink->wait_for_records(2),
            "the second AXTP media lease should replay only its descriptors");
    const auto second_replayed_streams = second_stream_sink->records();
    require(second_replayed_streams.size() == 2 &&
                second_replayed_streams[0].descriptor.device_id == second_real_device.id &&
                second_replayed_streams[1].descriptor.device_id == second_real_device.id &&
                second_replayed_streams[0].key.session_id == second_real_lease.session_id &&
                second_replayed_streams[1].key.session_id == second_real_lease.session_id,
            "same-ID streams must remain bound to the second device and lease");
    second_scripted->injectStream(
        0x1001, 8, 8000, {0x00, 0x00, 0x01, 0x65});
    require(second_stream_sink->wait_for_records(3) &&
                stream_sink->records().size() == 2,
            "a frame from B must not be delivered to A's same-ID stream");
    const auto original_after_second_open =
        real_host.call(real_lease.session_id, "audio.getAlgorithmConfig", {});
    require(original_after_second_open.status == axent::ControlStatus::Ok,
            "opening a second AXTP device must not disconnect the first");

    auto endpoint_real_device = real_device;
    endpoint_real_device.endpoint_id = "endpoint/real-primary";
    real_host.upsert_device(endpoint_real_device);
    auto endpoint_second_real_device = second_real_device;
    endpoint_second_real_device.endpoint_id = "endpoint/real-secondary";
    real_host.upsert_device(endpoint_second_real_device);

    const auto primary_requests_before_lane_gate = scripted->businessRequestCount();
    scripted->blockNextBusinessRequest();
    const auto primary_endpoint_operation = real_host.call_endpoint({
        "ep-test-source",
        "endpoint/real-primary",
        "audio.getAlgorithmConfig",
        nlohmann::json::object(),
    });
    require(scripted->waitForBusinessRequestBlocked(),
            "first real endpoint call should enter the primary physical AXTP lane");
    const auto primary_session_operation = real_host.call_async(
        real_lease.session_id, "audio.getAlgorithmConfig", {});
    const auto secondary_endpoint_operation = real_host.call_endpoint({
        "ep-test-source",
        "endpoint/real-secondary",
        "audio.getAlgorithmConfig",
        nlohmann::json::object(),
    });
    const auto secondary_result = secondary_endpoint_operation->wait_for(
        std::chrono::seconds(1));
    require(secondary_result.has_value() &&
                secondary_result->status == axent::ControlStatus::Ok,
            "B must complete while A1 remains blocked");
    require(!primary_session_operation->ready() &&
                scripted->businessRequestCount() == primary_requests_before_lane_gate + 1,
            "A2 must not bypass blocked A1 on the same real physical lane");
    scripted->unblockBusinessRequest();
    require(scripted->waitForBusinessRequests(primary_requests_before_lane_gate + 2),
            "A2 should enter only after A1 releases the real physical lane");
    require(primary_endpoint_operation->wait().status == axent::ControlStatus::Ok &&
                primary_session_operation->wait().status == axent::ControlStatus::Ok,
            "real primary calls should complete FIFO after the physical lane releases");

    real_host.release_session(second_real_lease.session_id,
                              "second device isolation verified");
    const auto original_after_second_release =
        real_host.call(real_lease.session_id, "audio.getAlgorithmConfig", {});
    require(original_after_second_release.status == axent::ControlStatus::Ok &&
                real_host.transport_diagnostics(real_device.id).open &&
                !real_host.transport_diagnostics(second_real_device.id).open,
            "releasing the second AXTP device must not reset the first");

    axent::MediaRelayOptions real_relay_options;
    real_relay_options.max_frames = 4;
    auto real_consumer = real_host.create_media_consumer(media_lease.session_id, real_relay_options);
    require(real_consumer != nullptr, "real media consumer should be created");
    auto real_push_sink = std::make_shared<RecordingMediaSink>();
    auto real_push_subscription = real_host.subscribe_media(media_lease.session_id, real_push_sink);
    require(real_push_subscription != nullptr, "real media direct subscription should be created");

    scripted->injectStream(0x1001, 9, 9000, {0x00, 0x00, 0x01, 0x65});
    require(real_push_sink->wait_for_frames(1), "real adapter stream should reach direct subscription");
    require(stream_sink->wait_for_records(3),
            "descriptor-driven subscription should receive the video frame");
    const auto stream_records_after_frame = stream_sink->records();
    require(stream_records_after_frame[2].kind == StreamCallbackKind::Frame &&
                stream_records_after_frame[2].key == replayed_streams[0].key,
            "Opened must precede a frame carrying the same StreamKey");
    const auto real_push_frames = real_push_sink->frames();
    require(real_push_frames.front().session_id == media_lease.session_id,
            "direct subscription session mismatch");
    require(real_push_frames.front().device_id == real_device.id,
            "direct subscription device mismatch");

    const auto media_frame = wait_for_media_frame(*real_consumer);
    require(media_frame.has_value(), "real adapter stream should reach host media relay");
    require(media_frame->session_id == media_lease.session_id, "media relay session mismatch");
    require(media_frame->device_id == real_device.id, "media relay device mismatch");
    require(media_frame->stream_id == 0x1001, "media relay stream mismatch");
    require(media_frame->codec == axent::MediaCodec::H264, "media relay codec mismatch");

    axent::MediaStreamDescriptor extension_descriptor;
    extension_descriptor.key = {"placeholder", 0x3001, 1};
    extension_descriptor.kind = axent::MediaKind::Video;
    extension_descriptor.codec = axent::MediaCodec::Opaque;
    extension_descriptor.source = "extension-source";
    require(real_host.publish_media_stream_event(
                media_lease.session_id,
                {axent::MediaStreamEventKind::Opened, extension_descriptor}),
            "an extension provider should coexist with AXTP media streams");
    auto late_extension_sink = std::make_shared<RecordingMediaStreamSink>();
    auto late_extension_subscription = real_host.subscribe_media_stream(
        media_lease.session_id, late_extension_sink);
    require(late_extension_subscription != nullptr &&
                late_extension_sink->wait_for_records(3),
            "late subscription should merge AXTP and extension descriptors");
    const auto late_extension_records = late_extension_sink->records();
    const auto extension_snapshot = std::find_if(
        late_extension_records.begin(), late_extension_records.end(),
        [](const StreamCallbackRecord& record) {
            return record.key.stream_id == 0x3001;
        });
    require(extension_snapshot != late_extension_records.end() &&
                extension_snapshot->descriptor.source == "extension-source",
            "AXTP snapshot refresh must not erase an extension-owned descriptor");
    extension_descriptor.source = "tampered-close";
    require(real_host.publish_media_stream_event(
                media_lease.session_id,
                {axent::MediaStreamEventKind::Closed, extension_descriptor}),
            "extension provider close should succeed");
    require(late_extension_sink->wait_for_records(4),
            "extension close should reach the late subscriber");
    const auto canonical_close = late_extension_sink->records().back();
    require(canonical_close.kind == StreamCallbackKind::Closed &&
                canonical_close.descriptor.source == "extension-source",
            "Closed must reuse the canonical descriptor stored by Opened");
    late_extension_subscription->cancel();

    auto reentrant_direct_sink = std::make_shared<RecordingMediaStreamSink>();
    auto reentrant_direct_subscription = real_host.subscribe_media_stream(
        media_lease.session_id, reentrant_direct_sink);
    require(reentrant_direct_subscription != nullptr &&
                reentrant_direct_sink->wait_for_records(2),
            "direct reentrant-call subscription should replay descriptors");
    std::atomic<bool> reentrant_direct_hook_ran{false};
    std::atomic<bool> reentrant_direct_call_succeeded{false};
    std::atomic<bool> direct_nested_frame_was_deferred{false};
    std::atomic<bool> direct_queued_publisher_enqueued{false};
    std::atomic<bool> direct_queued_publish_succeeded{false};
    std::atomic<int> reentrant_direct_depth{0};
    std::atomic<int> reentrant_direct_max_depth{0};
    std::thread direct_queued_publisher;
    auto direct_queued_frame = make_media_frame(81);
    direct_queued_frame.stream_id = 0x1001;
    direct_queued_frame.generation = replayed_streams[0].key.generation;
    reentrant_direct_sink->set_frame_hook([&]() {
        const auto depth = reentrant_direct_depth.fetch_add(1) + 1;
        if (depth > reentrant_direct_max_depth.load()) {
            reentrant_direct_max_depth.store(depth);
        }
        if (!reentrant_direct_hook_ran.exchange(true)) {
            direct_queued_publisher = std::thread([&]() {
                direct_queued_publish_succeeded.store(
                    real_host.publish_media_frame(
                        media_lease.session_id, direct_queued_frame));
            });
            const auto queued_deadline =
                std::chrono::steady_clock::now() + std::chrono::seconds(1);
            while (reentrant_direct_subscription->stats().received_frames < 2 &&
                   std::chrono::steady_clock::now() < queued_deadline) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            direct_queued_publisher_enqueued.store(
                reentrant_direct_subscription->stats().received_frames >= 2);
            scripted->injectStreamOnNextRequest(
                0x1001, 82, 82000, {0x00, 0x00, 0x01, 0x41});
            const auto result = real_host.call(
                real_lease.session_id, "audio.getAlgorithmConfig", {});
            reentrant_direct_call_succeeded.store(
                result.status == axent::ControlStatus::Ok);
            direct_nested_frame_was_deferred.store(
                reentrant_direct_sink->records().size() == 3);
        }
        reentrant_direct_depth.fetch_sub(1);
    });
    auto direct_reentrant_frame = make_media_frame(80);
    direct_reentrant_frame.stream_id = 0x1001;
    direct_reentrant_frame.generation = replayed_streams[0].key.generation;
    const bool direct_reentrant_publish_succeeded = real_host.publish_media_frame(
        media_lease.session_id, direct_reentrant_frame);
    if (direct_queued_publisher.joinable()) {
        direct_queued_publisher.join();
    }
    require(direct_reentrant_publish_succeeded &&
                reentrant_direct_sink->wait_for_records(5),
            "Direct reentrant adapter media should drain after the current callback");
    auto direct_followup_frame = direct_reentrant_frame;
    direct_followup_frame.sequence_id = 83;
    require(real_host.publish_media_frame(
                media_lease.session_id, direct_followup_frame) &&
                reentrant_direct_sink->wait_for_records(6),
            "Direct subscription should remain usable after deferred reentrant media");
    const auto reentrant_direct_records = reentrant_direct_sink->records();
    require(reentrant_direct_call_succeeded.load() &&
                direct_nested_frame_was_deferred.load() &&
                direct_queued_publisher_enqueued.load() &&
                direct_queued_publish_succeeded.load() &&
                reentrant_direct_max_depth.load() == 1 &&
                reentrant_direct_records[2].frame.sequence_id == 80 &&
                reentrant_direct_records[3].frame.sequence_id == 81 &&
                reentrant_direct_records[4].frame.sequence_id == 82 &&
                reentrant_direct_records[5].frame.sequence_id == 83,
            "Direct reentrant call media must remain non-nested and FIFO");
    reentrant_direct_subscription->cancel();

    axent::MediaSubscriptionOptions reentrant_async_options;
    reentrant_async_options.dispatch = axent::MediaSubscriptionDispatch::AsyncQueued;
    reentrant_async_options.max_frames = 4;
    auto reentrant_async_sink = std::make_shared<RecordingMediaStreamSink>();
    auto reentrant_async_subscription = real_host.subscribe_media_stream(
        media_lease.session_id, reentrant_async_sink, reentrant_async_options);
    require(reentrant_async_subscription != nullptr &&
                reentrant_async_sink->wait_for_records(2),
            "async reentrant-call subscription should replay descriptors");
    std::atomic<bool> reentrant_async_hook_ran{false};
    std::atomic<bool> reentrant_async_call_succeeded{false};
    reentrant_async_sink->set_frame_hook([&]() {
        if (reentrant_async_hook_ran.exchange(true)) {
            return;
        }
        scripted->injectStreamOnNextRequest(
            0x1001, 71, 71000, {0x00, 0x00, 0x01, 0x41});
        const auto result = real_host.call(
            real_lease.session_id, "audio.getAlgorithmConfig", {});
        reentrant_async_call_succeeded.store(result.status == axent::ControlStatus::Ok);
    });
    scripted->injectStream(0x1001, 70, 70000, {0x00, 0x00, 0x01, 0x41});
    require(reentrant_async_sink->wait_for_records(4),
            "media drained by an async sink reentrant call must not be lost");
    const auto reentrant_async_records = reentrant_async_sink->records();
    require(reentrant_async_call_succeeded.load() &&
                reentrant_async_records[2].kind == StreamCallbackKind::Frame &&
                reentrant_async_records[2].frame.sequence_id == 70 &&
                reentrant_async_records[3].kind == StreamCallbackKind::Frame &&
                reentrant_async_records[3].frame.sequence_id == 71,
            "async callback call() must defer and preserve nested adapter media order");
    reentrant_async_subscription->cancel();

    auto concurrent_direct_sink = std::make_shared<RecordingMediaStreamSink>();
    auto concurrent_direct_subscription = real_host.subscribe_media_stream(
        media_lease.session_id, concurrent_direct_sink);
    require(concurrent_direct_subscription != nullptr &&
                concurrent_direct_sink->wait_for_records(2),
            "concurrent direct subscription should replay descriptors");
    concurrent_direct_sink->block_next_frame();
    auto direct_frame = make_media_frame(60);
    direct_frame.stream_id = 0x1001;
    direct_frame.generation = replayed_streams[0].key.generation;
    std::thread::id first_publisher_id;
    std::thread::id second_publisher_id;
    std::thread::id third_publisher_id;
    std::atomic<bool> first_publish_succeeded{false};
    std::atomic<bool> second_publish_succeeded{false};
    std::atomic<bool> third_publish_succeeded{false};
    std::atomic<bool> second_publish_finished{false};
    std::atomic<bool> third_publish_finished{false};
    std::thread first_publisher([&]() {
        first_publisher_id = std::this_thread::get_id();
        first_publish_succeeded.store(
            real_host.publish_media_frame(media_lease.session_id, direct_frame));
    });
    require(concurrent_direct_sink->wait_for_frame_blocked(),
            "first direct publisher should enter its callback");
    auto second_direct_frame = direct_frame;
    second_direct_frame.sequence_id = 61;
    std::thread second_publisher([&]() {
        second_publisher_id = std::this_thread::get_id();
        second_publish_succeeded.store(
            real_host.publish_media_frame(media_lease.session_id, second_direct_frame));
        second_publish_finished.store(true);
    });
    const auto second_enqueue_deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (concurrent_direct_subscription->stats().received_frames < 2 &&
           std::chrono::steady_clock::now() < second_enqueue_deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    const bool second_enqueued =
        concurrent_direct_subscription->stats().received_frames >= 2;
    auto third_direct_frame = direct_frame;
    third_direct_frame.sequence_id = 62;
    std::thread third_publisher([&]() {
        third_publisher_id = std::this_thread::get_id();
        third_publish_succeeded.store(
            real_host.publish_media_frame(media_lease.session_id, third_direct_frame));
        third_publish_finished.store(true);
    });
    const auto third_enqueue_deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (concurrent_direct_subscription->stats().received_frames < 3 &&
           std::chrono::steady_clock::now() < third_enqueue_deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    const bool third_enqueued =
        concurrent_direct_subscription->stats().received_frames >= 3;
    const bool queued_publishers_waited =
        !second_publish_finished.load() && !third_publish_finished.load();
    concurrent_direct_sink->unblock_frames();
    first_publisher.join();
    second_publisher.join();
    third_publisher.join();
    require(second_enqueued && third_enqueued &&
                first_publish_succeeded.load() && second_publish_succeeded.load() &&
                third_publish_succeeded.load(),
            "all concurrent direct publishes should succeed");
    require(queued_publishers_waited,
            "queued Direct publishes must not return before their callbacks run");
    require(concurrent_direct_sink->wait_for_records(5),
            "all concurrent Direct frames should be delivered");
    const auto concurrent_direct_records = concurrent_direct_sink->records();
    require(concurrent_direct_records[2].kind == StreamCallbackKind::Frame &&
                concurrent_direct_records[2].frame.sequence_id == 60 &&
                concurrent_direct_records[2].thread_id == first_publisher_id &&
                concurrent_direct_records[3].kind == StreamCallbackKind::Frame &&
                concurrent_direct_records[3].frame.sequence_id == 61 &&
                concurrent_direct_records[3].thread_id == second_publisher_id,
            "the first two Direct callbacks must remain FIFO on their publisher threads");
    require(concurrent_direct_records[4].kind == StreamCallbackKind::Frame &&
                concurrent_direct_records[4].frame.sequence_id == 62 &&
                concurrent_direct_records[4].thread_id == third_publisher_id,
            "Direct callbacks must remain synchronous on their publishing threads");
    concurrent_direct_subscription->cancel();

    axent::MediaSubscriptionOptions drop_options;
    drop_options.dispatch = axent::MediaSubscriptionDispatch::AsyncQueued;
    drop_options.max_frames = 1;
    drop_options.max_bytes = 1024 * 1024;
    auto drop_sink = std::make_shared<RecordingMediaStreamSink>();
    auto drop_subscription = real_host.subscribe_media_stream(
        media_lease.session_id, drop_sink, drop_options);
    require(drop_subscription != nullptr && drop_sink->wait_for_records(2),
            "async stream subscription should replay descriptors");
    drop_sink->block_next_frame();
    scripted->injectStream(0x1001, 20, 20000, {0x00, 0x00, 0x01, 0x41});
    require(drop_sink->wait_for_frame_blocked(),
            "async stream sink should block its first frame callback");
    scripted->injectStream(0x1001, 21, 21000, {0x00, 0x00, 0x01, 0x41});
    scripted->injectStream(0x1001, 22, 22000, {0x00, 0x00, 0x01, 0x41});
    scripted->injectStream(0x1001, 23, 23000, {0x00, 0x00, 0x01, 0x41});
    const auto drop_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (drop_subscription->stats().dropped_frames < 2 &&
           std::chrono::steady_clock::now() < drop_deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    const auto dropped_while_blocked = drop_subscription->stats().dropped_frames;
    drop_sink->unblock_frames();
    require(dropped_while_blocked == 2,
            "async max_frames=1 must drop the two oldest queued frames");
    require(drop_sink->wait_for_records(5),
            "async stream sink should receive ordered drop and retained frame callbacks");
    const auto drop_records = drop_sink->records();
    require(drop_records[2].kind == StreamCallbackKind::Frame &&
                drop_records[2].frame.sequence_id == 20,
            "in-flight async frame should remain first");
    require(drop_records[3].kind == StreamCallbackKind::DeliveryDropped &&
                drop_records[3].delivery.dropped_frames == 2,
            "DeliveryDropped should precede the retained frame with cumulative counts");
    require(drop_records[4].kind == StreamCallbackKind::Frame &&
                drop_records[4].frame.sequence_id == 23 &&
                axent::has_flag(drop_records[4].frame.flags,
                                axent::MediaFrameFlag::Discontinuity),
            "latest retained frame must carry Discontinuity after drop-oldest");
    drop_subscription->cancel();

    auto cancel_sink = std::make_shared<RecordingMediaStreamSink>();
    auto cancel_subscription = real_host.subscribe_media_stream(
        media_lease.session_id, cancel_sink, drop_options);
    require(cancel_subscription != nullptr && cancel_sink->wait_for_records(2),
            "cancel test subscription should replay descriptors");
    cancel_sink->block_next_frame();
    scripted->injectStream(0x1001, 30, 30000, {0x00, 0x00, 0x01, 0x41});
    require(cancel_sink->wait_for_frame_blocked(),
            "cancel test should enter an in-flight async callback");
    std::atomic<bool> cancel_finished{false};
    std::thread cancel_thread([&]() {
        cancel_subscription->cancel();
        cancel_finished.store(true);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    const bool cancel_waited_for_callback = !cancel_finished.load();
    cancel_sink->unblock_frames();
    cancel_thread.join();
    require(cancel_waited_for_callback,
            "external cancel must wait for an in-flight stream callback");
    require(cancel_finished.load() && cancel_sink->wait_for_records(4),
            "external cancel should finish with SubscriptionClosed");
    const auto cancel_records = cancel_sink->records();
    require(cancel_records.back().kind == StreamCallbackKind::SubscriptionClosed,
            "SubscriptionClosed must be the terminal external-cancel callback");
    const auto records_after_cancel = cancel_records.size();
    scripted->injectStream(0x1001, 31, 31000, {0x00, 0x00, 0x01, 0x41});
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    require(cancel_sink->records().size() == records_after_cancel,
            "cancel return must prevent future stream callbacks");

    auto self_cancel_sink = std::make_shared<RecordingMediaStreamSink>();
    auto self_cancel_subscription = real_host.subscribe_media_stream(
        media_lease.session_id, self_cancel_sink);
    require(self_cancel_subscription != nullptr && self_cancel_sink->wait_for_records(2),
            "self-cancel stream subscription should replay descriptors");
    std::weak_ptr<axent::MediaStreamSubscription> weak_self_cancel =
        self_cancel_subscription;
    CallbackExitGate direct_self_cancel_gate;
    std::atomic<bool> reentrant_publish_rejected{false};
    self_cancel_sink->set_frame_hook([&]() {
        auto nested_frame = make_media_frame(32001);
        nested_frame.stream_id = 0x1001;
        nested_frame.generation = replayed_streams[0].key.generation;
        reentrant_publish_rejected.store(
            !real_host.publish_media_frame(media_lease.session_id, nested_frame));
        if (auto subscription = weak_self_cancel.lock()) {
            subscription->cancel();
        }
        direct_self_cancel_gate.enter_and_wait();
    });
    scripted->injectStream(0x1001, 32, 32000, {0x00, 0x00, 0x01, 0x41});
    require(direct_self_cancel_gate.wait_until_entered(),
            "direct self-cancel should return inside its in-flight callback");
    std::mutex direct_external_cancel_mutex;
    std::condition_variable direct_external_cancel_cv;
    bool direct_external_cancel_started = false;
    std::atomic<bool> direct_external_cancel_finished{false};
    std::thread direct_external_cancel_thread([&]() {
        {
            std::lock_guard<std::mutex> lock(direct_external_cancel_mutex);
            direct_external_cancel_started = true;
        }
        direct_external_cancel_cv.notify_all();
        self_cancel_subscription->cancel();
        direct_external_cancel_finished.store(true);
    });
    bool direct_external_cancel_observed = false;
    {
        std::unique_lock<std::mutex> lock(direct_external_cancel_mutex);
        direct_external_cancel_observed = direct_external_cancel_cv.wait_for(
            lock, std::chrono::seconds(1), [&]() {
                return direct_external_cancel_started;
            });
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    const bool direct_external_cancel_waited =
        !direct_external_cancel_finished.load();
    direct_self_cancel_gate.release();
    direct_external_cancel_thread.join();
    require(direct_external_cancel_observed && direct_external_cancel_waited &&
                direct_external_cancel_finished.load() &&
                reentrant_publish_rejected.load(),
            "Direct self-cancel must reject nested publish and remain externally quiescent");
    require(self_cancel_sink->wait_for_records(3),
            "direct sink should receive the frame that triggers self-cancel");
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    const auto self_cancel_count = self_cancel_sink->records().size();
    scripted->injectStream(0x1001, 33, 33000, {0x00, 0x00, 0x01, 0x41});
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    require(self_cancel_sink->records().size() == self_cancel_count,
            "self-cancel must not nest terminal callbacks or allow future frames");

    auto async_self_cancel_sink = std::make_shared<RecordingMediaStreamSink>();
    auto async_self_cancel_subscription = real_host.subscribe_media_stream(
        media_lease.session_id, async_self_cancel_sink, drop_options);
    require(async_self_cancel_subscription != nullptr &&
                async_self_cancel_sink->wait_for_records(2),
            "async self-cancel subscription should replay descriptors");
    std::weak_ptr<axent::MediaStreamSubscription> weak_async_self_cancel =
        async_self_cancel_subscription;
    CallbackExitGate async_self_cancel_gate;
    async_self_cancel_sink->set_frame_hook(
        [weak_async_self_cancel, &async_self_cancel_gate]() {
            if (auto subscription = weak_async_self_cancel.lock()) {
                subscription->cancel();
            }
            async_self_cancel_gate.enter_and_wait();
        });
    scripted->injectStream(0x1001, 34, 34000, {0x00, 0x00, 0x01, 0x41});
    require(async_self_cancel_gate.wait_until_entered(),
            "async self-cancel should return inside its in-flight callback");
    std::mutex async_external_cancel_mutex;
    std::condition_variable async_external_cancel_cv;
    bool async_external_cancel_started = false;
    std::atomic<bool> async_external_cancel_finished{false};
    std::thread async_external_cancel_thread([&]() {
        {
            std::lock_guard<std::mutex> lock(async_external_cancel_mutex);
            async_external_cancel_started = true;
        }
        async_external_cancel_cv.notify_all();
        async_self_cancel_subscription->cancel();
        async_external_cancel_finished.store(true);
    });
    bool async_external_cancel_observed = false;
    {
        std::unique_lock<std::mutex> lock(async_external_cancel_mutex);
        async_external_cancel_observed = async_external_cancel_cv.wait_for(
            lock, std::chrono::seconds(1), [&]() {
                return async_external_cancel_started;
            });
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    const bool async_external_cancel_waited =
        !async_external_cancel_finished.load();
    async_self_cancel_gate.release();
    async_external_cancel_thread.join();
    require(async_external_cancel_observed && async_external_cancel_waited &&
                async_external_cancel_finished.load(),
            "external cancel must wait until an async self-cancel callback exits");
    require(async_self_cancel_sink->wait_for_records(3),
            "async sink should receive the frame that triggers self-cancel");
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    const auto async_self_cancel_count = async_self_cancel_sink->records().size();
    scripted->injectStream(0x1001, 35, 35000, {0x00, 0x00, 0x01, 0x41});
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    require(async_self_cancel_sink->records().size() == async_self_cancel_count,
            "async self-cancel must not detach future callbacks");

    auto reentrant_sink = std::make_shared<RecordingMediaStreamSink>();
    auto reentrant_subscription = real_host.subscribe_media_stream(
        media_lease.session_id, reentrant_sink);
    require(reentrant_subscription != nullptr && reentrant_sink->wait_for_records(2),
            "reentrant release subscription should replay descriptors");
    std::atomic<axent::ControlStatus> reentrant_status{axent::ControlStatus::InternalError};
    std::atomic<bool> reentrant_release_rejected{false};
    std::atomic<bool> reentrant_stop_rejected{false};
    std::atomic<axent::ControlStatus> reentrant_acquire_status{
        axent::ControlStatus::InternalError};
    reentrant_sink->set_frame_hook([&]() {
        try {
            real_host.release_session(media_lease.session_id, "illegal callback release");
        } catch (const std::logic_error&) {
            reentrant_release_rejected.store(true);
        }
        try {
            real_host.stop();
        } catch (const std::logic_error&) {
            reentrant_stop_rejected.store(true);
        }
        reentrant_acquire_status.store(
            real_host.acquire_session(second_real_request).status);
        const auto result = real_host.call(
            real_lease.session_id, "audio.getAlgorithmConfig", {});
        reentrant_status.store(result.status);
    });
    reentrant_sink->block_next_frame();
    scripted->injectStream(0x1001, 40, 40000, {0x00, 0x00, 0x01, 0x41});
    require(reentrant_sink->wait_for_frame_blocked(),
            "reentrant release test should enter the direct sink callback");
    const auto stream_records_before_release = stream_sink->records().size();
    std::atomic<bool> release_finished{false};
    std::thread release_thread([&real_host, &media_lease, &release_finished]() {
        real_host.release_session(media_lease.session_id, "replace media lease");
        release_finished.store(true);
    });
    const bool release_entered_subscription_close =
        stream_sink->wait_for_records(stream_records_before_release + 3);
    const bool release_waited_for_callback = !release_finished.load();
    reentrant_sink->unblock_frames();
    release_thread.join();
    require(release_entered_subscription_close && release_waited_for_callback,
            "release must enter stream close and wait for an in-flight callback");
    require(release_finished.load(), "release should complete after the callback returns");
    require(reentrant_status.load() == axent::ControlStatus::Busy,
            "callback re-entry during release must fail fast with Busy instead of deadlocking");
    require(reentrant_release_rejected.load() && reentrant_stop_rejected.load() &&
                reentrant_acquire_status.load() == axent::ControlStatus::Busy,
            "Host lifecycle mutation must fail fast inside a media sink callback");

    const auto released_stream_records = stream_sink->records();
    require(released_stream_records.size() >= 3 &&
                released_stream_records[released_stream_records.size() - 3].kind ==
                    StreamCallbackKind::Closed &&
                released_stream_records[released_stream_records.size() - 2].kind ==
                    StreamCallbackKind::Closed &&
                released_stream_records.back().kind == StreamCallbackKind::SubscriptionClosed,
            "release must end each stream lifecycle before SubscriptionClosed");

    const auto factory_calls_after_media_release = transport_factory_calls;
    const auto second_after_media_release =
        real_host.acquire_session(second_real_request);
    require(second_after_media_release.acquired,
            "B should acquire while A retains a control-only lease");
    require(second_after_media_release.status == axent::ControlStatus::Ok &&
                transport_factory_calls == factory_calls_after_media_release + 1,
            "reopening B should create only B's replacement transport");
    const auto second_after_media_release_call = real_host.call(
        second_after_media_release.session_id, "audio.getAlgorithmConfig", {});
    require(second_after_media_release_call.status == axent::ControlStatus::Ok,
            "B should remain controllable while A has a control lease");
    const auto control_after_media_release =
        real_host.call(real_lease.session_id, "audio.getAlgorithmConfig", {});
    require(control_after_media_release.status == axent::ControlStatus::Ok,
            "A control lease must remain usable after its media lease is released");
    require(transport_factory_calls == factory_calls_after_media_release + 1,
            "A control call must keep A's existing transport while B is open");
    real_host.release_session(second_after_media_release.session_id,
                              "second device media complete");

    const auto replacement_lease = real_host.acquire_session(real_request);
    require(replacement_lease.acquired, "replacement media lease should be acquired");
    auto replacement_consumer = real_host.create_media_consumer(replacement_lease.session_id, real_relay_options);
    require(replacement_consumer != nullptr, "replacement media consumer should be created");

    auto replacement_stream_sink = std::make_shared<RecordingMediaStreamSink>();
    auto replacement_stream_subscription = real_host.subscribe_media_stream(
        replacement_lease.session_id, replacement_stream_sink);
    require(replacement_stream_subscription != nullptr &&
                replacement_stream_sink->wait_for_records(2),
            "replacement media lease should replay the still-open physical descriptors");
    const auto replacement_opened = replacement_stream_sink->records();
    require(replacement_opened[0].key.session_id == replacement_lease.session_id &&
                replacement_opened[0].key.generation == replayed_streams[0].key.generation,
            "logical media lease handoff must preserve physical generation and bind a new session id");

    scripted->blockAfterNextStream();
    scripted->injectStream(0x1001, 41, 41000, {0x00, 0x00, 0x01, 0x41});
    require(scripted->waitForStreamBlocked(),
            "stale stream should be queued before adapter drain");
    std::thread stale_release_thread([&real_host, &replacement_lease]() {
        real_host.release_session(replacement_lease.session_id, "replace stale media lease");
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    scripted->unblockStream();
    stale_release_thread.join();

    const auto post_stale_lease = real_host.acquire_session(real_request);
    require(post_stale_lease.acquired,
            "media lease should reacquire after the stale-frame release");
    auto post_stale_consumer =
        real_host.create_media_consumer(post_stale_lease.session_id, real_relay_options);
    require(post_stale_consumer != nullptr,
            "post-stale media consumer should be created");

    const auto stale_frame = wait_for_media_frame(*post_stale_consumer);
    require(!stale_frame.has_value(), "queued frame from released media lease must not reach replacement lease");

    scripted->injectStream(0x1001, 42, 42000, {0x00, 0x00, 0x01, 0x41});
    const auto post_handoff_frame = wait_for_media_frame(*post_stale_consumer);
    require(post_handoff_frame.has_value() &&
                post_handoff_frame->session_id == post_stale_lease.session_id &&
                post_handoff_frame->sequence_id == 42,
            "frame captured after handoff must carry the replacement lease token");

    axent::MediaSubscriptionOptions lifecycle_options;
    lifecycle_options.dispatch = axent::MediaSubscriptionDispatch::AsyncQueued;
    lifecycle_options.max_frames = 4;
    auto lifecycle_sink = std::make_shared<RecordingMediaStreamSink>();
    auto lifecycle_subscription = real_host.subscribe_media_stream(
        post_stale_lease.session_id, lifecycle_sink, lifecycle_options);
    require(lifecycle_subscription != nullptr && lifecycle_sink->wait_for_records(2),
            "terminal lifecycle subscription should replay active descriptors");
    lifecycle_sink->block_next_frame();
    scripted->injectStream(0x1001, 43, 43000, {0x00, 0x00, 0x01, 0x41});
    require(lifecycle_sink->wait_for_frame_blocked(),
            "terminal lifecycle test should hold an in-flight frame callback");
    require(real_adapter != nullptr, "real adapter test seam should remain available");
    axent::testing::AxtpAdapterTestSeam::stop_session_pump(*real_adapter);
    axent::testing::AxtpAdapterTestSeam::reopen_media_streams(
        *real_adapter, real_device.id);
    axent::testing::AxtpAdapterTestSeam::drain_media_callbacks(*real_adapter);

    std::atomic<bool> lifecycle_release_finished{false};
    std::thread lifecycle_release_thread([&]() {
        real_host.release_session(
            post_stale_lease.session_id, "terminal lifecycle reconciliation");
        lifecycle_release_finished.store(true);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    const bool lifecycle_release_waited = !lifecycle_release_finished.load();
    lifecycle_sink->unblock_frames();
    lifecycle_release_thread.join();
    require(lifecycle_release_waited && lifecycle_release_finished.load(),
            "release must wait for the in-flight lifecycle sink callback");

    const auto terminal_records = lifecycle_sink->records();
    require(terminal_records.size() == 10,
            "release must reconcile both old and pending stream generations");
    require(terminal_records[3].kind == StreamCallbackKind::Closed &&
                terminal_records[3].key.stream_id == 0x1001 &&
                terminal_records[3].key.generation == 1 &&
                terminal_records[4].kind == StreamCallbackKind::Opened &&
                terminal_records[4].key.stream_id == 0x1001 &&
                terminal_records[4].key.generation == 2 &&
                terminal_records[5].kind == StreamCallbackKind::Closed &&
                terminal_records[5].key == terminal_records[4].key &&
                terminal_records[6].kind == StreamCallbackKind::Closed &&
                terminal_records[6].key.stream_id == 0x2001 &&
                terminal_records[6].key.generation == 1 &&
                terminal_records[7].kind == StreamCallbackKind::Opened &&
                terminal_records[7].key.stream_id == 0x2001 &&
                terminal_records[7].key.generation == 2 &&
                terminal_records[8].kind == StreamCallbackKind::Closed &&
                terminal_records[8].key == terminal_records[7].key &&
                terminal_records[9].kind == StreamCallbackKind::SubscriptionClosed,
            "terminal reconciliation must preserve Closed/Open/Closed ordering per stream");

    const auto factory_calls_before_last_control_release = transport_factory_calls;
    real_host.release_session(real_lease.session_id, "last A control lease done");
    axent::SessionAcquireRequest second_control_request = second_real_request;
    second_control_request.media = false;
    const auto second_control_lease = real_host.acquire_session(second_control_request);
    require(second_control_lease.acquired,
            "B should acquire after A's last control-only lease is released");
    const auto second_control_call =
        real_host.call(second_control_lease.session_id, "audio.getAlgorithmConfig", {});
    require(second_control_call.status == axent::ControlStatus::Ok,
            "B control lease should connect after A ownership is released");
    require(transport_factory_calls == factory_calls_before_last_control_release + 1,
            "B takeover should create exactly one new transport after A's final release");
    real_host.release_session(second_control_lease.session_id, "B control lease done");
    real_host.stop();

    return 0;
}
