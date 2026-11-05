#include "axent/control/framed_client.hpp"
#include "core/runtime/core/axtp_core.hpp"
#include <iostream>
#include <stdexcept>
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
void connect(axent::control::FramedClient& client){
    check(client.Start(100,123,"050101"),"start negative-case session");axtp::AxtpCore server;
    for(unsigned pass=0;pass<5;++pass){for(const auto& b:client.TakeFrames())server.byteSink().onBytes(b.data(),b.size());while(auto b=server.tryPopOutboundBytes())check(client.Consume(*b,101+pass),"ready negative-case session");}
    check(client.Ready(),"fixture APP_READY");
}
std::vector<std::uint8_t> raw(const std::string& json,unsigned index=0,unsigned count=1){
    std::vector<std::uint8_t> p={1};p.insert(p.end(),json.begin(),json.end());
    std::vector<std::uint8_t> b={0x41,0x58,1,2,std::uint8_t(p.size()>>8),std::uint8_t(p.size()),0,0,0,1,std::uint8_t(index),std::uint8_t(count)};b.insert(b.end(),p.begin(),p.end());
    std::uint16_t crc=0xffff;for(auto v:b){crc^=std::uint16_t(v)<<8;for(unsigned n=0;n<8;++n)crc=std::uint16_t((crc<<1)^((crc&0x8000)?0x1021:0));}b.push_back(std::uint8_t(crc>>8));b.push_back(std::uint8_t(crc));return b;
}
int main(){try{
    check(axent::control::event_masks({"stream.stateChanged"})==std::optional<std::string>("050101"),"subscription mask comes from the registered stream domain and bit offset");
    axent::control::FramedClient client;
    check(client.Start(100,123,"0"),"client starts standard CONTROL handshake");
    check(!client.Ready(),"transport open is not APP_READY");
    axtp::AxtpCore server;
    for(unsigned pass=0;pass<5;++pass){
        for(const auto& bytes:client.TakeFrames())server.byteSink().onBytes(bytes.data(),bytes.size());
        while(auto bytes=server.tryPopOutboundBytes())check(client.Consume(*bytes,101+pass),"standard Core peer handshake is consumed");
    }
    check(client.Ready(),"only ACCEPT Hello Identify Identified establish APP_READY");
    check(client.Request(11,"input.openStream","{\"commandId\":\"open\"}",120),"ready client can issue typed input RPC");
    for(const auto& bytes:client.TakeFrames())server.byteSink().onBytes(bytes.data(),bytes.size());
    const auto event=server.pollEvent();
    check(event&&event->type==axtp::CoreEventType::RpcRequest&&event->rpc.requestId==11&&std::string(event->rpc.body.begin(),event->rpc.body.end())=="{\"commandId\":\"open\"}","real Runtime peer receives standard RPC request body and independent request ID");
    axent::control::FramedClient timeout;check(timeout.Start(0,1,"0"),"second connection starts");timeout.TakeFrames();timeout.Tick(5001);
    check(timeout.Failed()&&!timeout.Ready()&&!timeout.Request(1,"input.openStream","{}",5001),"handshake timeout fences later input controls");
    axent::control::FramedClient fragments;connect(fragments);
    check(fragments.Consume(raw("{",0,2),200),"ordered fragment assembly begins");fragments.Tick(5201);
    check(fragments.Failed(),"incomplete fragment assembly has a bounded lifetime even after APP_READY");
    axent::control::FramedClient nesting;connect(nesting);
    const std::string deep="{\"nested\":"+std::string(40,'[')+"0"+std::string(40,']')+"}";
    check(!nesting.Request(1,"input.openStream",deep,200),"public boundary rejects excessive JSON depth before parsing");
    axent::control::FramedClient sid;connect(sid);
    check(!sid.Consume(raw("{\"sid\":\"another-session\",\"op\":5,\"d\":{\"event\":\"stream.stateChanged\",\"data\":{}}}"),200)&&sid.Failed(),"other session cannot publish into this owner");
    axent::control::FramedClient heartbeat;connect(heartbeat);heartbeat.Tick(1200);heartbeat.TakeFrames();heartbeat.Tick(4201);
    check(heartbeat.Failed(),"missing negotiated heartbeat acknowledgements fences the session");
    auto corrupt=client.TakeFrames();
    check(!client.Consume({1,2,3},130)&&client.Failed(),"malformed binary message fences input session");
    std::cout<<"Standard message client interoperates with Runtime Core and fences handshake/CRC faults\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
