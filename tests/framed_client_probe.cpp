#include "axent/control/framed_client.hpp"
#include "axent/input/input_stream.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
using Json=nlohmann::json;
using Bytes=std::vector<std::uint8_t>;
std::string hex(const Bytes& bytes){std::string out;constexpr char digits[]="0123456789abcdef";for(auto b:bytes){out+=digits[b>>4];out+=digits[b&15];}return out;}
Bytes unhex(const std::string& text){
    if(text.size()%2)throw std::runtime_error("odd hex length");Bytes out;
    auto digit=[](char c)->unsigned{if(c>='0'&&c<='9')return unsigned(c-'0');if(c>='a'&&c<='f')return unsigned(c-'a'+10);if(c>='A'&&c<='F')return unsigned(c-'A'+10);throw std::runtime_error("invalid hex");};
    for(std::size_t i=0;i<text.size();i+=2)out.push_back(std::uint8_t((digit(text[i])<<4)|digit(text[i+1])));return out;
}
int main(){
    axent::control::FramedClient client;std::string line;
    while(std::getline(std::cin,line)){Json output={{"ok",false}};std::vector<Bytes> inputFrames;
        try{
            const auto command=Json::parse(line);const auto op=command.at("op").get<std::string>();const auto now=command.value("nowMs",std::uint64_t(100));
            if(op=="start")output["ok"]=client.Start(now,command.value("randomSeed",123U),command.value("eventMasks",std::string("050101")));
            else if(op=="consume")output["ok"]=client.Consume(unhex(command.at("hex").get<std::string>()),now);
            else if(op=="tick"){client.Tick(now);output["ok"]=true;}
            else if(op=="request")output["ok"]=client.Request(command.at("id").get<std::uint32_t>(),command.at("method").get<std::string>(),command.value("body",Json::object()).dump(),now);
            else if(op=="encodeInput"){
                axent::input::Packet packet;packet.stream_id=command.at("streamId").get<std::uint32_t>();packet.sequence=command.at("seqId").get<std::uint32_t>();packet.sample_time_us=command.at("cursor").get<std::uint64_t>();
                for(const auto& contact:command.at("contacts"))packet.sample.contacts.push_back({contact.at("id").get<std::uint16_t>(),static_cast<axent::input::TouchPhase>(contact.at("phase").get<unsigned>()),contact.at("x").get<std::uint16_t>(),contact.at("y").get<std::uint16_t>()});
                axent::input::FramingOptions options;options.max_frame_bytes=command.value("maxFrameBytes",std::size_t(4096));
                output["ok"]=axent::input::encode_frames(packet,inputFrames,options);
            }else if(op=="drain")output["ok"]=true;
            else throw std::runtime_error("unknown probe command");
        }catch(const std::exception& error){output["error"]=error.what();}
        output["ready"]=client.Ready();output["failed"]=client.Failed();output["frames"]=Json::array();
        for(const auto& bytes:client.TakeFrames())output["frames"].push_back(hex(bytes));
        for(const auto& bytes:inputFrames)output["frames"].push_back(hex(bytes));
        output["messages"]=Json::array();
        for(const auto& value:client.TakeMessages()){
            auto body=Json::parse(value.body,nullptr,false);
            output["messages"].push_back({{"kind",value.kind==axent::control::FramedMessage::Kind::Response?"response":value.kind==axent::control::FramedMessage::Kind::Event?"event":"request"},
                {"id",value.id},{"method",value.method},{"status",value.status},{"body",body.is_discarded()?Json(value.body):body}});
        }
        std::cout<<output.dump()<<std::endl;
    }
}
