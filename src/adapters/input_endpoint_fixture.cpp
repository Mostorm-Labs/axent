#include "axent/testing/input_endpoint_fixture.hpp"
#include "axtp_adapter_test_seam.hpp"
#include "core/protocol/wire/inbound_processor.hpp"
#include "core/protocol/wire/outbound_processor.hpp"
#include <atomic>
#include <mutex>
#include <queue>
#include <stdexcept>

namespace axent::testing {
namespace {
struct Writer : axtp::IByteWriter {
    axtp::Bytes bytes;
    void writeBytes(const axtp::Byte* data, std::size_t count) override { bytes.insert(bytes.end(),data,data+count); }
};
struct Sink : axtp::IPayloadSink {
    std::vector<axtp::ControlPayload> controls;
    std::vector<axtp::RpcPayload> rpcs;
    std::vector<axtp::StreamPayload> streams;
    void onControl(axtp::ControlPayload value) override { controls.push_back(std::move(value)); }
    void onRpc(axtp::RpcPayload value) override { rpcs.push_back(std::move(value)); }
    void onStream(axtp::StreamPayload value) override { streams.push_back(std::move(value)); }
};
}
struct InputEndpointFixture::Impl {
    mutable std::mutex mutex;
    std::vector<input::Packet> packets;
    std::atomic<unsigned> physicalOpens{0}, opens{0}, closes{0};
    std::atomic<std::uint32_t> stream{40};
    std::atomic<bool> releaseReady{true}, fencePending{false};
};
namespace {
class Transport final : public axtp::ITransport {
public:
    explicit Transport(std::shared_ptr<InputEndpointFixture::Impl> state):state_(std::move(state)),inbound_(sink_) {}
    void bind(axtp::IByteSink& sink) override { output_=&sink; }
    void open() override {}
    void close() override {}
    axtp::TransportProfile profile() const override { return {axtp::TransportKind::Hid,axtp::AxtpWireMode::FramedBinary,axtp::jsonBinaryRpcEncoding(),false,false,true,64}; }
    void poll() override {
        if (!state_->fencePending.exchange(false)) return;
        axtp::RpcPayload event;event.encoding=axtp::RpcEncoding::Json;event.op=axtp::RpcOp::Event;
        event.methodOrEventId=static_cast<std::uint32_t>(axtp::EventId::StreamStateChanged);
        event.meta.sourceProtocol=axtp::SourceProtocol::JsonRpc;event.meta.jsonSid="fixture-session";
        event.meta.jsonMethodOrEventName="stream.stateChanged";
        const auto id=state_->stream.load();const auto body=nlohmann::json{{"streamId",id},{"state",{{"streamId",id},{"state","closing"}}}}.dump();
        event.body.assign(body.begin(),body.end());Writer writer;axtp::OutboundProcessor out(writer);out.sendRpc(std::move(event));inject(writer.bytes);
    }
    void sendBytes(const axtp::Byte* bytes,std::size_t count) override {
        inbound_.onBytes(bytes,count);
        auto controls=std::move(sink_.controls);sink_.controls.clear();
        auto rpcs=std::move(sink_.rpcs);sink_.rpcs.clear();auto streams=std::move(sink_.streams);sink_.streams.clear();
        for(const auto& control:controls)if(control.opcode==axtp::ControlOpcode::Open){
            ++state_->physicalOpens;axtp::ControlPayload accepted;accepted.opcode=axtp::ControlOpcode::Accept;
            accepted.controlId=control.controlId;accepted.statusCode=axtp::ErrorCode::Success;
            Writer writer;axtp::OutboundProcessor out(writer);out.sendControl(accepted);out.sendRpc(axtp::JsonRpcEncoder::makeHello());inject(writer.bytes);
        }
        for(const auto& rpc:rpcs){
            if(rpc.op==axtp::RpcOp::Identify){Writer writer;axtp::OutboundProcessor out(writer);out.sendRpc(axtp::JsonRpcEncoder::makeIdentified("fixture-session"));inject(writer.bytes);continue;}
            if(rpc.op!=axtp::RpcOp::Request)continue;
            if(rpc.meta.jsonSid!="fixture-session")throw std::runtime_error("fixture input escaped authenticated session");
            const auto params=nlohmann::json::parse(rpc.body.begin(),rpc.body.end());nlohmann::json result;
            if(rpc.methodOrEventId==static_cast<std::uint32_t>(axtp::MethodId::InputOpenStream)){
                ++state_->opens;result={{"streamId",++state_->stream},{"accepted",true},{"format","input-sample-v1"},{"acceptedInputTypes",params.at("inputTypes")},
                    {"maxDataBytes",128},{"touch",{{"maxContacts",10}}},{"revision",std::to_string(state_->opens.load())}};
                if(params.contains("mouse"))result["mouse"]=params.at("mouse");
                if(!params.contains("touch"))result.erase("touch");
            }else if(rpc.methodOrEventId==static_cast<std::uint32_t>(axtp::MethodId::InputCloseStream)){
                ++state_->closes;result={{"streamId",params.at("streamId")},{"closed",state_->releaseReady.load()},{"revision",std::to_string(state_->opens.load())}};
            }else throw std::runtime_error("unexpected fixture RPC");
            axtp::RpcPayload response;response.encoding=axtp::RpcEncoding::Json;response.op=axtp::RpcOp::RequestResponse;
            response.requestId=rpc.requestId;response.methodOrEventId=rpc.methodOrEventId;response.statusCode=axtp::ErrorCode::Success;
            if(rpc.methodOrEventId==static_cast<std::uint32_t>(axtp::MethodId::InputCloseStream)&&!state_->releaseReady.load()){
                response.statusCode=axtp::ErrorCode::Unavailable;result={{"cleanupPending",true}};
            }
            response.meta.sourceProtocol=axtp::SourceProtocol::JsonRpc;response.meta.jsonSid=rpc.meta.jsonSid;
            const auto body=result.dump();response.body.assign(body.begin(),body.end());Writer writer;axtp::OutboundProcessor out(writer);out.sendRpc(std::move(response));inject(writer.bytes);
        }
        for(const auto& stream:streams){input::Sample sample;if(!input::decode_data(stream.data,sample))throw std::runtime_error("fixture received invalid typed sample");
            std::lock_guard<std::mutex> lock(state_->mutex);state_->packets.push_back({stream.streamId,stream.seqId,stream.cursor,std::move(sample)});}
    }
private:
    void inject(const axtp::Bytes& bytes){output_->onBytes(bytes.data(),bytes.size());}
    std::shared_ptr<InputEndpointFixture::Impl> state_;
    axtp::IByteSink* output_=nullptr;Sink sink_;axtp::InboundProcessor inbound_;
};
}
InputEndpointFixture::InputEndpointFixture():impl_(std::make_shared<Impl>()){}
InputEndpointFixture::~InputEndpointFixture()=default;
std::unique_ptr<AxtpAdapter> InputEndpointFixture::adapter(AxtpAdapterConfig config){
    config.enable_media=false;config.enable_session_health_probe=false;
    transport::HidDeviceInfo device;device.path="input-fixture";device.vendorId=0x0581;device.productId=0x2582;
    device.usagePage=0x0081;device.usage=1;device.serialNumber="INPUT-FIXTURE";device.product="NA20";
    return AxtpAdapterTestSeam::make(config,[state=impl_](const auto&){return std::make_unique<Transport>(state);},{device});
}
input::SourceDescriptor InputEndpointFixture::source(std::string session,std::uint64_t generation) const {
    input::SourceDescriptor source;source.physical_generation=generation;source.capability.authenticated_session=std::move(session);
    source.capability.source_binding_id="authenticated-source-fixture";source.capability.route_ready=true;source.capability.input_types={input::Type::Touch};
    source.capability.max_frame_bytes=64;source.capability.limits.max_contacts=10;source.capability.limits.max_data_bytes=128;source.capability.limits.allow_mouse=false;return source;
}
void InputEndpointFixture::release_ready(bool ready){impl_->releaseReady=ready;}
void InputEndpointFixture::fence(){impl_->fencePending=true;}
unsigned InputEndpointFixture::physical_opens()const{return impl_->physicalOpens;}
unsigned InputEndpointFixture::opens()const{return impl_->opens;}
unsigned InputEndpointFixture::closes()const{return impl_->closes;}
std::vector<input::Packet> InputEndpointFixture::packets()const{std::lock_guard<std::mutex> lock(impl_->mutex);return impl_->packets;}
}
