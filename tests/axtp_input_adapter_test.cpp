#include <iostream>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <vector>

#include "axent/control/control_contract.hpp"
#include "axent/input/input_route.hpp"
#include "axtp_adapter_test_seam.hpp"
#include "core/protocol/wire/inbound_processor.hpp"
#include "core/protocol/wire/outbound_processor.hpp"

namespace {
using namespace axent::input;
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
struct Writer : axtp::IByteWriter {
    axtp::Bytes bytes;
    void writeBytes(const axtp::Byte* p, std::size_t n) override {
        bytes.insert(bytes.end(), p, p + n);
    }
};
struct Sink : axtp::IPayloadSink {
    std::vector<axtp::ControlPayload> controls;
    std::vector<axtp::RpcPayload> rpcs;
    std::vector<axtp::StreamPayload> streams;
    void onControl(axtp::ControlPayload v) override { controls.push_back(std::move(v)); }
    void onRpc(axtp::RpcPayload v) override { rpcs.push_back(std::move(v)); }
    void onStream(axtp::StreamPayload v) override { streams.push_back(std::move(v)); }
};
// Test endpoint represents an already implemented R2 consumer behind NA20.
// No private RTSP fields are invented or required by this fixture.
class Endpoint : public axtp::ITransport {
public:
    Endpoint() : inbound_(received_) {}
    void bind(axtp::IByteSink& sink) override { sink_ = &sink; }
    void open() override {}
    void close() override {}
    axtp::TransportProfile profile() const override {
        return {axtp::TransportKind::Hid, axtp::AxtpWireMode::FramedBinary,
                axtp::jsonBinaryRpcEncoding(), false, false, true, 64};
    }
    void poll() override {
        std::queue<axtp::Bytes> pending;
        { std::lock_guard<std::mutex> lock(mutex_); pending.swap(pending_); }
        while (!pending.empty()) { inject(pending.front()); pending.pop(); }
    }
    void state(std::uint32_t stream, const char* value) {
        axtp::RpcPayload event;
        event.encoding = axtp::RpcEncoding::Json;
        event.op = axtp::RpcOp::Event;
        event.methodOrEventId = static_cast<std::uint32_t>(axtp::EventId::StreamStateChanged);
        event.meta.sourceProtocol = axtp::SourceProtocol::JsonRpc;
        event.meta.jsonSid = "physical-session";
        event.meta.jsonMethodOrEventName = "stream.stateChanged";
        event.bodyEncoding = axtp::RpcBodyEncoding::None;
        const auto body = nlohmann::json{{"streamId", stream},
            {"state", {{"streamId", stream}, {"state", value}}}}.dump();
        event.body.assign(body.begin(), body.end());
        Writer writer; axtp::OutboundProcessor out(writer); out.sendRpc(std::move(event));
        std::lock_guard<std::mutex> lock(mutex_); pending_.push(std::move(writer.bytes));
    }
    void sendBytes(const axtp::Byte* p, std::size_t n) override {
        inbound_.onBytes(p, n);
        auto controls = std::move(received_.controls); received_.controls.clear();
        auto rpcs = std::move(received_.rpcs); received_.rpcs.clear();
        auto streams = std::move(received_.streams); received_.streams.clear();
        for (const auto& v : controls) if (v.opcode == axtp::ControlOpcode::Open) {
            ++control_opens;
            axtp::ControlPayload accept;
            accept.opcode = axtp::ControlOpcode::Accept;
            accept.controlId = v.controlId;
            accept.statusCode = axtp::ErrorCode::Success;
            Writer writer; axtp::OutboundProcessor out(writer);
            out.sendControl(accept); out.sendRpc(axtp::JsonRpcEncoder::makeHello());
            inject(writer.bytes);
        }
        for (const auto& rpc : rpcs) {
            if (rpc.op == axtp::RpcOp::Identify) {
                Writer writer; axtp::OutboundProcessor out(writer);
                out.sendRpc(axtp::JsonRpcEncoder::makeIdentified("physical-session"));
                inject(writer.bytes); continue;
            }
            if (rpc.op != axtp::RpcOp::Request) continue;
            require(rpc.meta.jsonSid == "physical-session", "RPC used a different physical session");
            const auto params = nlohmann::json::parse(rpc.body.begin(), rpc.body.end());
            nlohmann::json result;
            if (rpc.methodOrEventId == static_cast<std::uint32_t>(axtp::MethodId::InputOpenStream)) {
                require(axent::control::validate_payload("input.openStream",
                    axent::control::PayloadRole::Request, params), "open violated R2 schema");
                ++opens; last_open = params;
                result = {{"streamId", ++next_stream}, {"format", "input-sample-v1"},
                    {"revision", std::to_string(opens)}, {"accepted", accepted},
                    {"acceptedInputTypes", params.at("inputTypes")}, {"maxDataBytes", 128},
                    {"touch", {{"maxContacts", 10}}}};
                if(params.contains("mouse"))result["mouse"]=params.at("mouse");
                if(!params.contains("touch"))result.erase("touch");
            } else if (rpc.methodOrEventId == static_cast<std::uint32_t>(axtp::MethodId::InputCloseStream)) {
                require(axent::control::validate_payload("input.closeStream",
                    axent::control::PayloadRole::Request, params), "close violated R2 schema");
                ++closes;
                result = {{"streamId", params.at("streamId")}, {"closed", release_ok},
                    {"revision", std::to_string(opens)}};
            } else {
                throw std::runtime_error("unexpected RPC in isolated input test");
            }
            axtp::RpcPayload response;
            response.encoding = axtp::RpcEncoding::Json;
            response.op = axtp::RpcOp::RequestResponse;
            response.requestId = rpc.requestId;
            response.methodOrEventId = rpc.methodOrEventId;
            response.statusCode = axtp::ErrorCode::Success;
            if(rpc.methodOrEventId==static_cast<std::uint32_t>(axtp::MethodId::InputOpenStream)&&open_status!=axtp::ErrorCode::Success){response.statusCode=open_status;result=nlohmann::json::object();}
            if (rpc.methodOrEventId == static_cast<std::uint32_t>(axtp::MethodId::InputCloseStream) && !release_ok) {
                response.statusCode = axtp::ErrorCode::Unavailable;
                result = {{"cleanupPending", true}};
            }
            response.meta.sourceProtocol = axtp::SourceProtocol::JsonRpc;
            response.meta.jsonSid = rpc.meta.jsonSid;
            const auto body = result.dump();
            response.body.assign(body.begin(), body.end());
            Writer writer; axtp::OutboundProcessor out(writer); out.sendRpc(std::move(response));
            inject(writer.bytes);
        }
        for (auto& stream : streams) delivered.push_back(std::move(stream));
    }
    unsigned control_opens = 0, opens = 0, closes = 0;
    std::uint32_t next_stream = 40;
    bool accepted = true, release_ok = true;
    axtp::ErrorCode open_status=axtp::ErrorCode::Success;
    nlohmann::json last_open;
    std::vector<axtp::StreamPayload> delivered;
private:
    void inject(const axtp::Bytes& bytes) { sink_->onBytes(bytes.data(), bytes.size()); }
    axtp::IByteSink* sink_ = nullptr;
    Sink received_;
    axtp::InboundProcessor inbound_;
    std::mutex mutex_;
    std::queue<axtp::Bytes> pending_;
};
void run() {
    auto config = axent::AxtpAdapter::na20_defaults();
    config.enable_media = false;
    config.enable_session_health_probe = false;
    Endpoint* endpoint = nullptr;
    unsigned factories = 0;
    auto adapter = axent::testing::AxtpAdapterTestSeam::make(config,
        [&](const auto&) {
            ++factories;
            auto transport = std::make_unique<Endpoint>();
            endpoint = transport.get();
            return transport;
        });
    const std::string device = "hid:0581:2582:INPUT";
    std::string error;
    require(adapter->open_session(device, error), "physical session failed");
    axent::testing::AxtpAdapterTestSeam::stop_session_pump(*adapter);
    StreamOpenRequest request;
    request.authenticated_session = "logical-media-session";
    request.source_binding_id = "opaque-from-trusted-media";
    request.target = {"runtime-1", "presentation-1", 2, 0, 3, 4};
    request.command_id = "open-1";
    require(!adapter->open_input_stream(device, request).accepted() && endpoint->opens == 0,
            "request fabricated its own capability/source binding");
    SourceDescriptor source;
    source.physical_generation = adapter->input_session_generation(device);
    source.capability = {"input-sample-v1", {Type::Touch}, {}, 64,
                         request.authenticated_session, request.source_binding_id, true};
    source.capability.limits.max_contacts = 10;
    source.capability.limits.max_data_bytes = 128;
    source.capability.limits.allow_mouse = false;
    require(adapter->set_input_source(device, source, &error), "trusted descriptor rejected");
    const auto opened = adapter->open_input_stream(device, request);
    require(opened.accepted() && endpoint->opens == 1 && factories == 1 && endpoint->control_opens == 1,
            "input open created another physical session");
    require(endpoint->last_open.at("sourceBindingId") == request.source_binding_id,
            "source binding changed on wire");
    Packet packet; packet.stream_id = opened.handle.stream_id; packet.sequence = 1;
    packet.sample_time_us = 1234;
    for (unsigned i = 0; i < 10; ++i)
        packet.sample.contacts.push_back({static_cast<std::uint16_t>(i), TouchPhase::Down, 123, 456});
    std::vector<std::vector<std::uint8_t>> frames;
    require(encode_frames(packet, frames, {64}, opened.limits) && frames.size() > 1,
            "fragmented fixture failed");
    for (const auto& frame : frames)
        require(adapter->send_input_frame(device, {opened.handle, frame}, &error) == StreamResult::Accepted,
                "input frame not sent on physical session");
    std::vector<std::uint8_t> data;
    require(encode_data(packet.sample, data, opened.limits), "data encode failed");
    require(endpoint->delivered.size() == 1 && endpoint->delivered[0].data == data &&
            endpoint->delivered[0].streamId == packet.stream_id &&
            endpoint->delivered[0].seqId == 1 && endpoint->delivered[0].cursor == 1234,
            "physical adapter modified STREAM identity or payload");
    endpoint->release_ok = false;
    require(adapter->close_input_stream(device, opened.handle) == StreamResult::Unavailable,
            "unconfirmed native cleanup became closed");
    require(!adapter->open_input_stream(device, request).accepted(), "cleanup blocked allowed another open");
    endpoint->release_ok = true;
    require(adapter->close_input_stream(device, opened.handle) == StreamResult::Accepted,
            "native cleanup retry failed");
    const auto next = adapter->open_input_stream(device, request);
    require(next.accepted() && next.handle.stream_id != opened.handle.stream_id,
            "reopen reused retired stream");
    require(adapter->send_input_frame(device, {opened.handle, frames.front()}) == StreamResult::Stale,
            "old stream crossed reopen fence");
    endpoint->state(next.handle.stream_id, "closing");
    axent::testing::AxtpAdapterTestSeam::poll_session(*adapter);
    require(adapter->send_input_frame(device, {next.handle, frames.front()}) != StreamResult::Accepted,
            "remote state event was not observed by adapter");
    require(adapter->close_input_stream(device, next.handle) == StreamResult::Accepted,
            "remote fenced stream did not close");
    const auto withdrawalOpen = adapter->open_input_stream(device, request);
    require(withdrawalOpen.accepted(), "withdrawal fixture opens");
    auto staleSource = source;
    staleSource.capability.route_ready = false;
    ++staleSource.physical_generation;
    require(!adapter->set_input_source(device, staleSource), "stale source withdrew current input");
    source.capability.route_ready = false;
    require(adapter->set_input_source(device, source) && !adapter->input_source(device),
            "source withdrawal did not remove readiness");
    require(adapter->send_input_frame(device, {withdrawalOpen.handle, frames.front()}) != StreamResult::Accepted,
            "withdrawn source still sent STREAM");
    require(adapter->close_input_stream(device, withdrawalOpen.handle) == StreamResult::Accepted,
            "withdrawn source lost cleanup identity");
    require(!adapter->open_input_stream(device, request).accepted(),
            "closed relay reopened withdrawn source");
    source.capability.route_ready = true;
    require(adapter->set_input_source(device, source), "source restoration failed after confirmed release");

    source.capability.input_types={Type::Touch,Type::Mouse};source.capability.limits.allow_mouse=true;source.capability.limits.allow_continuous_wheel=false;source.capability.limits.modifier_mask=0;
    require(adapter->set_input_source(device,source),"mixed descriptor failed");request.input_types={Type::Touch,Type::Mouse};request.limits=source.capability.limits;
    const auto mixed=adapter->open_input_stream(device,request);
    require(mixed.accepted()&&mixed.handle.input_types==request.input_types&&endpoint->last_open.at("mouse").at("modifierMask")==0,"same-session adapter did not negotiate mixed input");
    Packet mouse;mouse.stream_id=mixed.handle.stream_id;mouse.sequence=1;mouse.sample_time_us=4000;mouse.sample.type=Type::Mouse;mouse.sample.mouse.kind=MouseKind::Buttons;mouse.sample.mouse.buttons=1;
    frames.clear();require(encode_frames(mouse,frames,{64},mixed.limits)&&frames.size()==1,"Mouse frame fixture");
    require(adapter->send_input_frame(device,{mixed.handle,frames.front()})==StreamResult::Accepted&&endpoint->delivered.back().data.front()==2,"same physical session did not forward Mouse discriminator");
    require(adapter->close_input_stream(device,mixed.handle)==StreamResult::Accepted,"mixed stream release");
    endpoint->next_stream = withdrawalOpen.handle.stream_id - 1;
    const auto reused = adapter->open_input_stream(device, request);
    require(!reused.accepted() && reused.cleanup_pending && reused.reason == "input_stream_id_reused",
            "source restoration forgot retired stream IDs");
    require(adapter->close_input_stream(device, reused.handle) == StreamResult::Accepted,
            "reused allocation lost cleanup ownership");
    endpoint->accepted = false;
    require(!adapter->open_input_stream(device, request).accepted(),
            "accepted=false response activated input");
    // Release the malformed accepted ownership using the returned identity.
    axent::testing::AxtpAdapterTestSeam::release_session(*adapter, device);
    require(adapter->send_input_frame(device, {next.handle, frames.front()}) != StreamResult::Accepted,
            "released physical session accepted old input");
    require(factories == 1, "send/close implicitly reopened physical transport");
}
} // namespace
void rejected_open_diagnostic(){
    auto config=axent::AxtpAdapter::na20_defaults();config.enable_media=false;config.enable_session_health_probe=false;
    Endpoint* endpoint=nullptr;
    auto adapter=axent::testing::AxtpAdapterTestSeam::make(config,[&](const auto&){auto value=std::make_unique<Endpoint>();endpoint=value.get();return value;});
    const std::string device="hid:0581:2582:REJECTED-INPUT";std::string error;
    require(adapter->open_session(device,error),"diagnostic session failed");axent::testing::AxtpAdapterTestSeam::stop_session_pump(*adapter);
    SourceDescriptor source;source.physical_generation=adapter->input_session_generation(device);
    source.capability={"input-sample-v1",{Type::Touch},{},64,"logical-media-session","diagnostic-source",true};source.capability.limits.allow_mouse=false;
    require(adapter->set_input_source(device,source),"diagnostic source failed");
    StreamOpenRequest request;request.authenticated_session="logical-media-session";request.source_binding_id="diagnostic-source";request.target={"runtime-1","media-1",1,0,1,1};request.command_id="rejected-open";
    endpoint->open_status=axtp::ErrorCode::NotSupported;
    const auto result=adapter->open_input_stream(device,request);
    require(!result.accepted()&&result.cleanup_pending&&result.reason.find("code=3")!=std::string::npos,"input error lost canonical RPC status or bypassed cleanup");
    axent::testing::AxtpAdapterTestSeam::release_session(*adapter,device);
}
int main() {
    try { run(); rejected_open_diagnostic(); std::cout << "AXTP input adapter: one session, R2 control, fragmented STREAM, cleanup, error status and fencing passed\n"; }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
