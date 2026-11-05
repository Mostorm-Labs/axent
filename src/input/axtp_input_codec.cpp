#include "axent/input/input_stream.hpp"

#include <utility>

#include "core/protocol/wire/input_frame_codec.hpp"
#include "core/protocol/wire/input_sample_codec.hpp"

namespace axent::input {
namespace {
namespace runtime = axtp::input;

runtime::InputLimits convert(const Limits& value) {
    runtime::InputLimits out;
    out.maxDataBytes = value.max_data_bytes;
    out.maxContacts = value.max_contacts;
    out.allowTouch = value.allow_touch;
    out.allowMouse = value.allow_mouse;
    out.buttonsMask = value.buttons_mask;
    out.modifierMask = value.modifier_mask;
    out.allowChords = value.allow_chords;
    out.allowDetentWheel = value.allow_detent_wheel;
    out.allowContinuousWheel = value.allow_continuous_wheel;
    return out;
}

bool convert(const Sample& value, runtime::InputSample& out, std::string* error) {
    if (value.type != Type::Touch && value.type != Type::Mouse) {
        if (error) *error = "unknown input type";
        return false;
    }
    out.type = value.type == Type::Touch ? runtime::InputType::Touch : runtime::InputType::Mouse;
    for (const auto& r : value.contacts) {
        const auto phase = static_cast<unsigned>(r.phase);
        if (phase > 3) {
            if (error) *error = "unknown contact phase";
            return false;
        }
        out.contacts.push_back({r.id, static_cast<std::uint8_t>(phase), r.x, r.y});
    }
    const auto& m = value.mouse;
    if (static_cast<unsigned>(m.kind) > 4 || static_cast<unsigned>(m.wheel_unit) > 1 ||
        static_cast<unsigned>(m.wheel_phase) > 4) {
        if (error) *error = "unknown mouse kind/unit/phase";
        return false;
    }
    out.mouse = {static_cast<std::uint8_t>(m.kind), m.modifiers, m.buttons, m.x, m.y,
                 static_cast<std::uint8_t>(m.wheel_unit), static_cast<std::uint8_t>(m.wheel_phase),
                 m.wheel_x, m.wheel_y};
    return true;
}

Sample convert(const runtime::InputSample& value) {
    Sample out;
    out.type = value.type == runtime::InputType::Touch ? Type::Touch : Type::Mouse;
    for (const auto& r : value.contacts)
        out.contacts.push_back({r.contactId, static_cast<TouchPhase>(r.phase), r.x, r.y});
    const auto& m = value.mouse;
    out.mouse = {static_cast<MouseKind>(m.eventKind), m.modifiers, m.buttons, m.x, m.y,
                 static_cast<WheelUnit>(m.wheelUnit), static_cast<WheelPhase>(m.wheelPhase),
                 m.wheelX, m.wheelY};
    return out;
}
} // namespace

std::string_view format() noexcept { return axtp::input_wire::kFormat; }

bool encode_data(const Sample& value, std::vector<std::uint8_t>& output,
                 const Limits& limits, std::string* error) {
    runtime::InputSample sample;
    return convert(value, sample, error) &&
           runtime::encodeInputSample(sample, output, convert(limits), error);
}

bool decode_data(const std::vector<std::uint8_t>& bytes, Sample& output,
                 const Limits& limits, std::string* error) {
    runtime::InputSample sample;
    if (!runtime::decodeInputSample(bytes, sample, convert(limits), error)) return false;
    output = convert(sample);
    return true;
}

bool encode_frames(const Packet& value, std::vector<std::vector<std::uint8_t>>& output,
                   const FramingOptions& options, const Limits& limits, std::string* error) {
    axtp::StreamPayload stream;
    stream.streamId = value.stream_id;
    stream.seqId = value.sequence;
    stream.cursor = value.sample_time_us;
    if (!encode_data(value.sample, stream.data, limits, error)) return false;
    runtime::InputFrameOptions framing;
    framing.maxFrameBytes = options.max_frame_bytes;
    framing.sourceId = options.source_node;
    framing.destinationId = options.destination_node;
    framing.messageId = options.message_id;
    return runtime::encodeInputFrames(stream, output, framing, error);
}

struct FrameDecoder::Impl {
    Limits limits;
    runtime::InputFrameDecoder decoder;
    Impl(const Limits& value, std::size_t size) : limits(value), decoder(size) {}
};

FrameDecoder::FrameDecoder(const Limits& limits, std::size_t size)
    : impl_(std::make_unique<Impl>(limits, size)) {}
FrameDecoder::~FrameDecoder() = default;
FrameDecoder::FrameDecoder(FrameDecoder&&) noexcept = default;
FrameDecoder& FrameDecoder::operator=(FrameDecoder&&) noexcept = default;

bool FrameDecoder::consume(const std::vector<std::uint8_t>& bytes,
                           std::optional<Packet>& output, std::string* error) {
    output.reset();
    std::optional<axtp::StreamPayload> stream;
    if (!impl_->decoder.consume(bytes, stream, error)) return false;
    if (!stream) return true;
    Packet packet;
    if (!decode_data(stream->data, packet.sample, impl_->limits, error)) return false;
    packet.stream_id = stream->streamId;
    packet.sequence = stream->seqId;
    packet.sample_time_us = stream->cursor;
    output = std::move(packet);
    return true;
}

void FrameDecoder::reset() { impl_->decoder.reset(); }
} // namespace axent::input
