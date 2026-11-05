#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>
namespace axent::control {
std::optional<std::string> event_masks(const std::vector<std::string>& eventNames);
struct FramedMessage {
    enum class Kind { Request, Response, Event };
    Kind kind=Kind::Request;
    std::uint32_t id=0;
    std::string method;
    std::string body;
    std::uint16_t status=0;
};
// Serial-owner client for one already authenticated message-oriented transport.
// The caller binds identity/generation and owns byte delivery, product methods,
// authorization and reset on channel loss. JSON text bodies are opaque here.
class FramedClient {
public:
    explicit FramedClient(std::size_t maxFrameBytes=4096);
    ~FramedClient();
    FramedClient(FramedClient&&) noexcept;
    FramedClient& operator=(FramedClient&&) noexcept;
    FramedClient(const FramedClient&)=delete;
    FramedClient& operator=(const FramedClient&)=delete;
    bool Start(std::uint64_t nowMs,std::uint32_t randomSeed,const std::string& eventMasks);
    bool Consume(const std::vector<std::uint8_t>&,std::uint64_t nowMs);
    void Tick(std::uint64_t nowMs);
    bool Ready() const;
    bool Failed() const;
    std::size_t MaxFrameBytes()const;
    bool Request(std::uint32_t id,const std::string& method,const std::string& body,std::uint64_t nowMs);
    bool Respond(std::uint32_t id,const std::string& method,const std::string& body,std::uint16_t status=0);
    std::vector<std::vector<std::uint8_t>> TakeFrames();
    std::vector<FramedMessage> TakeMessages();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
