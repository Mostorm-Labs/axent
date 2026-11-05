#include "axent/control/framed_client.hpp"
#include "core/protocol/session/control_session.hpp"
#include "core/protocol/wire/framed_binary/inbound/payload_decoder.hpp"
#include "core/protocol/wire/outbound_processor.hpp"
#include <map>
#include <algorithm>
#include <string_view>
#include <nlohmann/json.hpp>

namespace axent::control {
namespace {
// Bound hostile nesting before entering the recursive JSON decoder. Syntax is
// still validated by the Runtime codec; braces inside strings do not count.
bool bounded_json(std::string_view text){
    unsigned depth=0;bool quoted=false,escaped=false;
    for(const char c:text){
        if(quoted){if(escaped)escaped=false;else if(c=='\\')escaped=true;else if(c=='"')quoted=false;continue;}
        if(c=='"')quoted=true;
        else if(c=='{'||c=='['){if(++depth>32)return false;}
        else if(c=='}'||c==']'){if(!depth)return false;--depth;}
    }return !quoted&&depth==0;
}
}
std::optional<std::string> event_masks(const std::vector<std::string>& names){
    struct Fact{const char* name;unsigned domain,bit;};
    static const Fact facts[]={
#include "axent_event_mask_facts.inc"
    };
    std::map<unsigned,std::vector<std::uint8_t>> domains;
    for(const auto& name:names){
        const Fact* found=nullptr;for(const auto& fact:facts)if(name==fact.name){found=&fact;break;}
        if(!found||found->domain>255||found->bit>=2040)return {};
        auto& bytes=domains[found->domain];bytes.resize(std::max(bytes.size(),std::size_t(found->bit/8+1)),0);
        bytes[found->bit/8]|=std::uint8_t(1U<<(found->bit%8));
    }
    std::string result;const auto add=[&](unsigned b){constexpr char hex[]="0123456789ABCDEF";result+=hex[(b>>4)&15];result+=hex[b&15];};
    for(const auto& domain:domains){add(domain.first);add(unsigned(domain.second.size()));for(const auto b:domain.second)add(b);}
    return result;
}
struct FramedClient::Impl final:axtp::IPayloadSink,axtp::IByteWriter {
    enum class Stage { Idle, Accept, Hello, Identified, Ready, Failed };
    struct Pending {std::string method;std::uint64_t deadline;};
    Stage stage=Stage::Idle;
    std::size_t maxFrame;
    axtp::ControlSession control;
    axtp::PayloadDecoder decoder{*this};
    axtp::OutboundProcessor outbound;
    std::vector<std::vector<std::uint8_t>> frames;
    std::vector<FramedMessage> messages;
    std::map<std::uint32_t,Pending> pending;
    std::map<std::uint32_t,std::string> incoming;
    axtp::Bytes assembly;
    axtp::FrameHeader assemblyHeader{};
    unsigned nextFragment=0;
    std::uint64_t now=0,deadline=0,lastHeartbeat=0,heartbeatSince=0,assemblyDeadline=0;
    std::uint32_t seed=0,lastRequest=0;
    std::uint16_t heartbeatId=1;
    std::string sid,masks;
    bool decoded=false;
    explicit Impl(std::size_t size):maxFrame(size),outbound(*this,size){}
    bool fail(){stage=Stage::Failed;frames.clear();messages.clear();pending.clear();incoming.clear();assembly.clear();return false;}
    void writeBytes(const axtp::Byte* data,std::size_t size)override{
        if(frames.size()>=128||size>maxFrame){fail();return;}
        frames.emplace_back(data,data+size);
    }
    void onControl(axtp::ControlPayload value)override{
        decoded=true;
        if(value.opcode==axtp::ControlOpcode::Accept){
            if(stage!=Stage::Accept||value.controlId!=1||value.statusCode!=axtp::ErrorCode::Success||!value.tlv.valid||
               !value.tlv.hasSelectedRpcEncoding||value.tlv.selectedRpcEncoding!=static_cast<std::uint8_t>(axtp::RpcEncoding::Json)||
               !value.tlv.hasMaxFrameSize||value.tlv.maxFrameSize<64||value.tlv.maxFrameSize>maxFrame||
               (value.tlv.hasAckMode&&value.tlv.ackMode!=0)){fail();return;}
            control.handle(value);if(!control.isOpen()){fail();return;}
            maxFrame=value.tlv.maxFrameSize;outbound.setMaxFrameSize(maxFrame);stage=Stage::Hello;return;
        }
        if(!control.isOpen()){fail();return;}
        if(value.opcode==axtp::ControlOpcode::HeartbeatAck){
            if(heartbeatSince&&value.controlId==heartbeatId)heartbeatSince=0;
            return;
        }
        if(value.opcode==axtp::ControlOpcode::Heartbeat||value.opcode==axtp::ControlOpcode::Ping){
            if(auto response=control.handle(value))outbound.sendControl(std::move(*response));return;
        }
        fail();
    }
    void onRpc(axtp::RpcPayload value)override{
        decoded=true;
        if(value.encoding!=axtp::RpcEncoding::Json){fail();return;}
        if(value.op==axtp::RpcOp::Hello){
            if(stage!=Stage::Hello||!value.meta.jsonSid.empty()){fail();return;}
            const auto hello=nlohmann::json::parse(value.body.begin(),value.body.end(),nullptr,false);
            if(!hello.is_object()||hello.contains("authentication")){fail();return;}
            stage=Stage::Identified;outbound.sendRpc(axtp::JsonRpcEncoder::makeIdentify(seed,masks));
            return;
        }
        if(value.op==axtp::RpcOp::Identified){
            if(stage!=Stage::Identified||value.meta.jsonSid.empty()||value.meta.jsonSid.size()>128){fail();return;}
            sid=value.meta.jsonSid;stage=Stage::Ready;lastHeartbeat=now;return;
        }
        if(stage!=Stage::Ready||value.meta.jsonSid!=sid){fail();return;}
        if(value.meta.localGeneratedResponse){value.meta.sourceProtocol=axtp::SourceProtocol::JsonRpc;outbound.sendRpcError(std::move(value));return;}
        FramedMessage message;message.id=value.requestId;message.status=static_cast<std::uint16_t>(value.statusCode);
        message.method=value.meta.jsonMethodOrEventName;message.body.assign(value.body.begin(),value.body.end());
        if(value.op==axtp::RpcOp::RequestResponse){
            const auto found=pending.find(value.requestId);if(found==pending.end())return;
            message.kind=FramedMessage::Kind::Response;message.method=found->second.method;pending.erase(found);
        }else if(value.op==axtp::RpcOp::Event){message.kind=FramedMessage::Kind::Event;}
        else if(value.op==axtp::RpcOp::Request){
            if(!value.requestId||incoming.size()>=32||!incoming.emplace(value.requestId,message.method).second){fail();return;}
            message.kind=FramedMessage::Kind::Request;
        }else {fail();return;}
        if(messages.size()>=128){fail();return;}messages.push_back(std::move(message));
    }
    void onStream(axtp::StreamPayload)override{decoded=true;fail();}
    axtp::RpcPayload rpc(axtp::RpcOp op,std::uint32_t id,const std::string& method,const std::string& body){
        axtp::RpcPayload value;value.encoding=axtp::RpcEncoding::Json;value.op=op;value.requestId=id;
        value.bodyEncoding=axtp::RpcBodyEncoding::None;value.meta.sourceProtocol=axtp::SourceProtocol::JsonRpc;
        value.meta.jsonSid=sid;value.meta.jsonMethodOrEventName=method;value.body.assign(body.begin(),body.end());return value;
    }
};
FramedClient::FramedClient(std::size_t size):impl_(std::make_unique<Impl>(size)){}
FramedClient::~FramedClient()=default;
FramedClient::FramedClient(FramedClient&&)noexcept=default;
FramedClient& FramedClient::operator=(FramedClient&&)noexcept=default;
bool FramedClient::Start(std::uint64_t now,std::uint32_t seed,const std::string& masks){
    auto& s=*impl_;if(s.stage!=Impl::Stage::Idle||s.maxFrame<64||s.maxFrame>4096||masks.size()>1024)return false;
    s.now=now;s.deadline=now+5000;s.seed=seed;s.masks=masks;s.stage=Impl::Stage::Accept;
    auto open=s.control.makeOpen(1);open.tlv.maxFrameSize=static_cast<std::uint32_t>(s.maxFrame);open.tlv.supportedRpcEncodings=1;
    open.body=axtp::ControlTlvCodec::encode(open.tlv,false);s.outbound.sendControl(std::move(open));return !Failed();
}
bool FramedClient::Consume(const std::vector<std::uint8_t>& bytes,std::uint64_t now){
    auto& s=*impl_;s.now=now;if(s.stage==Impl::Stage::Idle||Failed())return false;
    if(bytes.size()<14||bytes.size()>s.maxFrame||bytes.size()!=14+((std::size_t(bytes[4])<<8)|bytes[5]))return s.fail();
    struct Capture:axtp::IFrameSink{std::optional<axtp::Frame> frame;void onFrame(axtp::Frame f)override{frame=std::move(f);}} capture;
    axtp::FrameDecoder decode(capture);decode.onBytes(bytes.data(),bytes.size());if(!capture.frame)return s.fail();
    auto& frame=*capture.frame;
    if(frame.header.frameIndex==0){if(s.nextFragment)return s.fail();s.assemblyHeader=frame.header;s.assembly.clear();s.assemblyDeadline=now+5000;}
    else if(frame.header.frameIndex!=s.nextFragment||frame.header.messageId!=s.assemblyHeader.messageId||frame.header.payloadType!=s.assemblyHeader.payloadType||frame.header.frameCount!=s.assemblyHeader.frameCount)return s.fail();
    if(s.assembly.size()+frame.payload.size()>65536)return s.fail();
    s.assembly.insert(s.assembly.end(),frame.payload.begin(),frame.payload.end());
    s.nextFragment=unsigned(frame.header.frameIndex)+1;
    if(s.nextFragment<frame.header.frameCount)return true;
    s.nextFragment=0;
    if(frame.header.payloadType==axtp::PayloadType::Rpc&&(!s.assembly.empty()&&s.assembly.front()==static_cast<std::uint8_t>(axtp::RpcEncoding::Json))&&
       !bounded_json(std::string_view(reinterpret_cast<const char*>(s.assembly.data()+1),s.assembly.size()-1)))return s.fail();
    s.decoded=false;s.decoder.onMessage({frame.header.messageId,frame.header.payloadType,std::move(s.assembly)});
    return s.decoded&&!Failed()?true:s.fail();
}
void FramedClient::Tick(std::uint64_t now){
    auto& s=*impl_;s.now=now;if(Failed()||s.stage==Impl::Stage::Idle)return;
    if(s.nextFragment&&now>=s.assemblyDeadline){s.fail();return;}
    if(!Ready()){if(now>=s.deadline)s.fail();return;}
    const auto interval=s.control.negotiatedHeartbeatIntervalMs().value_or(1000);
    if(s.heartbeatSince&&now-s.heartbeatSince>=interval*3){s.fail();return;}
    if(!s.heartbeatSince&&now-s.lastHeartbeat>=interval){
        if(++s.heartbeatId==0)++s.heartbeatId;
        s.heartbeatSince=now;s.lastHeartbeat=now;s.outbound.sendControl(s.control.makeHeartbeat(s.heartbeatId));
    }
    for(auto it=s.pending.begin();it!=s.pending.end();)if(now>=it->second.deadline){
        s.messages.push_back({FramedMessage::Kind::Response,it->first,it->second.method,"{\"reason\":\"RPC_TIMEOUT\"}",static_cast<std::uint16_t>(axtp::ErrorCode::RpcResponseTimeout)});it=s.pending.erase(it);
    }else ++it;
}
bool FramedClient::Ready()const{return impl_->stage==Impl::Stage::Ready;}
bool FramedClient::Failed()const{return impl_->stage==Impl::Stage::Failed;}
std::size_t FramedClient::MaxFrameBytes()const{return impl_->maxFrame;}
bool FramedClient::Request(std::uint32_t id,const std::string& method,const std::string& body,std::uint64_t now){
    auto& s=*impl_;if(!Ready()||!id||id<=s.lastRequest||method.empty()||method.size()>128||body.size()>32768||s.pending.size()>=32)return false;
    if(!bounded_json(body)||!nlohmann::json::parse(body,nullptr,false).is_object())return false;
    s.lastRequest=id;s.pending.emplace(id,Impl::Pending{method,now+5000});s.outbound.sendRpcRequest(s.rpc(axtp::RpcOp::Request,id,method,body));return !Failed();
}
bool FramedClient::Respond(std::uint32_t id,const std::string& method,const std::string& body,std::uint16_t status){
    auto& s=*impl_;const auto found=s.incoming.find(id);if(!Ready()||found==s.incoming.end()||found->second!=method||body.size()>32768||!bounded_json(body)||!nlohmann::json::parse(body,nullptr,false).is_object())return false;
    auto value=s.rpc(axtp::RpcOp::RequestResponse,id,method,body);value.statusCode=static_cast<axtp::ErrorCode>(status);
    s.incoming.erase(found);s.outbound.sendRpcResponse(std::move(value));return !Failed();
}
std::vector<std::vector<std::uint8_t>> FramedClient::TakeFrames(){std::vector<std::vector<std::uint8_t>> result;result.swap(impl_->frames);return result;}
std::vector<FramedMessage> FramedClient::TakeMessages(){std::vector<FramedMessage> result;result.swap(impl_->messages);return result;}
}
