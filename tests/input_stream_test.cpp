#include <iostream>
#include <optional>
#include <stdexcept>
#include "axent/input/input_stream.hpp"

void require(bool value, const char* text) { if (!value) throw std::runtime_error(text); }

int main() {
    using namespace axent::input;
    try {
        require(format() == "input-sample-v1", "wrong bound input format");
        Sample fingers;
        fingers.contacts = {{7, TouchPhase::Down, 10000, 20000}, {8, TouchPhase::Down, 30000, 40000}};
        std::vector<std::uint8_t> bytes;
        require(encode_data(fingers, bytes) && bytes.size() == 15, "two contacts must be 1+7x2");
        const std::vector<std::uint8_t> golden = {1,0,7,0,0x27,0x10,0x4e,0x20,0,8,0,0x75,0x30,0x9c,0x40};
        require(bytes == golden, "public contract differs from reviewed bytes");
        Sample decoded;
        require(decode_data(bytes, decoded) && decoded.contacts.size() == 2, "multi-contact decode");
        bytes.push_back(0);
        require(!decode_data(bytes, decoded) && decoded.contacts.size() == 2, "failure mutated output or accepted padding");
        Packet packet; packet.stream_id = 7; packet.sequence = 1; packet.sample_time_us = 1000;
        for (std::uint16_t i=0; i<5; ++i) packet.sample.contacts.push_back({i, TouchPhase::Down, 10000, 20000});
        FramingOptions framing; framing.max_frame_bytes = 64;
        std::vector<std::vector<std::uint8_t>> frames;
        require(encode_frames(packet, frames, framing) && frames.size() == 2, "complete Standard Frame fragmentation");
        FrameDecoder receiver({}, 64);
        std::optional<Packet> result;
        require(receiver.consume(frames[0], result) && !result, "partial sample leaked");
        require(receiver.consume(frames[1], result) && result && result->sample.contacts.size() == 5, "reassembled sample lost contacts");
        require(result->stream_id == 7 && result->sequence == 1 && result->sample_time_us == 1000, "STREAM context changed");
        auto corrupt = frames[0]; corrupt.back() ^= 1;
        require(!receiver.consume(corrupt, result) && !result, "bad CRC not rejected");
        Limits touch_only; touch_only.allow_mouse = false;
        Sample mouse; mouse.type = Type::Mouse; mouse.mouse.kind = MouseKind::Cancel;
        require(!encode_data(mouse, bytes, touch_only), "unaccepted Mouse encoded");
        mouse.mouse.kind = MouseKind::Wheel; mouse.mouse.wheel_x = -120; mouse.mouse.wheel_y = 240;
        require(encode_data(mouse, bytes) && bytes.size() == 22, "wheel length");
        require(decode_data(bytes, decoded) && decoded.mouse.wheel_x == -120 && decoded.mouse.wheel_y == 240, "signed wheel changed");
        std::cout << "Public input contract, 7xN, Mouse, full frames, fragments and CRC passed\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
