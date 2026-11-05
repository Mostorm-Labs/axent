#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace axent::input {

// Product-facing semantics only. Runtime schema/IDs and wire headers are private.
enum class Type { Touch, Mouse };
enum class TouchPhase { Down, Move, Up, Cancel };
enum class MouseKind { Move, Buttons, Wheel, Cancel, Modifiers };
enum class WheelUnit { Detent120, RegionQ16_16 };
enum class WheelPhase { None, Began, Changed, Ended, Cancelled };

struct Contact {
    std::uint16_t id = 0;
    TouchPhase phase = TouchPhase::Down;
    std::uint16_t x = 0;
    std::uint16_t y = 0;
};
struct Mouse {
    MouseKind kind = MouseKind::Move;
    std::uint16_t modifiers = 0;
    std::uint32_t buttons = 0;
    std::uint16_t x = 0;
    std::uint16_t y = 0;
    WheelUnit wheel_unit = WheelUnit::Detent120;
    WheelPhase wheel_phase = WheelPhase::None;
    std::int32_t wheel_x = 0;
    std::int32_t wheel_y = 0;
};
struct Sample {
    Type type = Type::Touch;
    std::vector<Contact> contacts;
    Mouse mouse;
};
struct Packet {
    std::uint32_t stream_id = 0;
    std::uint32_t sequence = 0;
    std::uint64_t sample_time_us = 0;
    Sample sample;
};
struct Limits {
    std::size_t max_data_bytes = 4096;
    std::size_t max_contacts = 256;
    bool allow_touch = true;
    bool allow_mouse = true;
    std::uint32_t buttons_mask = 31;
    std::uint16_t modifier_mask = 15;
    bool allow_chords = true;
    bool allow_detent_wheel = true;
    bool allow_continuous_wheel = true;
};

// Filled by the authenticated media/device integration, never by input bytes.
struct SourceBinding {
    std::string authenticated_session;
    std::string source_binding_id;
};

struct FramingOptions {
    std::size_t max_frame_bytes = 4096;
    std::uint8_t source_node = 0;
    std::uint8_t destination_node = 0;
    std::uint16_t message_id = 1;
};

std::string_view format() noexcept;
bool encode_data(const Sample&, std::vector<std::uint8_t>&, const Limits& = {}, std::string* = nullptr);
bool decode_data(const std::vector<std::uint8_t>&, Sample&, const Limits& = {}, std::string* = nullptr);
bool encode_frames(const Packet&, std::vector<std::vector<std::uint8_t>>&,
                   const FramingOptions& = {}, const Limits& = {}, std::string* = nullptr);

// One authenticated STREAM context per decoder. Caller owns readiness, target,
// sequence validation, lifecycle and reset on disconnect; no injection occurs.
class FrameDecoder {
public:
    explicit FrameDecoder(const Limits& = {}, std::size_t max_frame_bytes = 4096);
    ~FrameDecoder();
    FrameDecoder(FrameDecoder&&) noexcept;
    FrameDecoder& operator=(FrameDecoder&&) noexcept;
    FrameDecoder(const FrameDecoder&) = delete;
    FrameDecoder& operator=(const FrameDecoder&) = delete;
    bool consume(const std::vector<std::uint8_t>&, std::optional<Packet>&, std::string* = nullptr);
    void reset();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace axent::input
