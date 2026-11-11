#include "axent/input/input_route.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace axent::input {
namespace {
bool valid_target(const StreamTarget& t) {
    return !t.runtime_instance_id.empty() && t.runtime_instance_id.size() <= 128 &&
           !t.session_id.empty() && t.session_id.size() <= 128 && t.session_generation &&
           t.binding_generation && t.lifecycle_generation;
}
bool same_target(const StreamTarget& a, const StreamTarget& b) {
    return a.runtime_instance_id == b.runtime_instance_id && a.session_id == b.session_id &&
           a.session_generation == b.session_generation && a.slot_id == b.slot_id &&
           a.binding_generation == b.binding_generation && a.lifecycle_generation == b.lifecycle_generation;
}
bool same(const StreamHandle& a, const StreamHandle& b) {
    return a.authenticated_session == b.authenticated_session &&
           a.source_binding_id == b.source_binding_id && a.stream_id == b.stream_id &&
           a.generation == b.generation && a.input_type == b.input_type && a.input_types == b.input_types && same_target(a.target, b.target);
}
bool contains(const std::vector<Type>& types, Type type) {
    return std::find(types.begin(), types.end(), type) != types.end();
}
bool valid_limits(const Limits& l, Type type) {
    if (!l.max_data_bytes || l.max_data_bytes > 4096) return false;
    if (type == Type::Touch)
        return l.allow_touch && l.max_contacts && l.max_contacts <= 256 &&
               l.max_data_bytes >= 8 && l.max_contacts <= (l.max_data_bytes - 1) / 7;
    if (type == Type::Mouse)
        return l.allow_mouse && l.max_data_bytes >= 12 && !(l.buttons_mask & ~31U) &&
               !(l.modifier_mask & ~15U) &&
               (!(l.allow_detent_wheel || l.allow_continuous_wheel) || l.max_data_bytes >= 22);
    return false;
}
Limits intersect(Limits a, const Limits& b) {
    a.max_data_bytes = std::min({a.max_data_bytes, b.max_data_bytes, std::size_t(4096)});
    a.max_contacts = std::min({a.max_contacts, b.max_contacts, std::size_t(256),
                              a.max_data_bytes ? (a.max_data_bytes - 1) / 7 : 0});
    a.allow_touch = a.allow_touch && b.allow_touch;
    a.allow_mouse = a.allow_mouse && b.allow_mouse;
    a.buttons_mask &= b.buttons_mask;
    a.modifier_mask &= b.modifier_mask;
    a.allow_chords = a.allow_chords && b.allow_chords;
    a.allow_detent_wheel = a.allow_detent_wheel && b.allow_detent_wheel;
    a.allow_continuous_wheel = a.allow_continuous_wheel && b.allow_continuous_wheel;
    return a;
}
bool within(const Limits& a, const Limits& b, Type type) {
    if (a.max_data_bytes > b.max_data_bytes) return false;
    if (type == Type::Touch) return a.max_contacts <= b.max_contacts;
    return !(a.buttons_mask & ~b.buttons_mask) && !(a.modifier_mask & ~b.modifier_mask) &&
           (!a.allow_chords || b.allow_chords) && (!a.allow_detent_wheel || b.allow_detent_wheel) &&
           (!a.allow_continuous_wheel || b.allow_continuous_wheel);
}
} // namespace

InputStreamRelay::InputStreamRelay(StreamCapability capability, StreamOpenCallback open,
                                 FrameForwardCallback forward, StreamReleaseCallback release)
    : capability_(std::move(capability)), open_(std::move(open)), forward_(std::move(forward)),
      release_(std::move(release)) {}

StreamOpenResult InputStreamRelay::open(const StreamOpenRequest& request) {
    StreamOpenResult result;
    const auto reject = [&](StreamResult status, const char* reason) {
        result.result = status; result.reason = reason; snapshot_.last_reason = reason; return result;
    };
    if (snapshot_.state == StreamState::Active || snapshot_.state == StreamState::Closing ||
        snapshot_.state == StreamState::Blocked)
        return reject(StreamResult::Busy, "stream_already_owned");
    if (!capability_.route_ready || !open_) return reject(StreamResult::Unavailable, "route_not_ready");
    if (!valid_target(request.target) || request.command_id.empty() || request.command_id.size() > 128)
        return reject(StreamResult::Rejected, "invalid_target_or_command");
    if (request.authenticated_session.empty() || request.authenticated_session != capability_.authenticated_session)
        return reject(StreamResult::Rejected, "authenticated_session_mismatch");
    if (request.source_binding_id.empty() || request.source_binding_id != capability_.source_binding_id)
        return reject(StreamResult::Rejected, "source_binding_mismatch");
    if (request.format != capability_.format || request.format != format())
        return reject(StreamResult::Unsupported, "format_not_supported");
    auto requested=request.input_types.empty()?std::vector<Type>{request.input_type}:request.input_types;
    if(requested.empty()||requested.size()>2||std::set<Type>(requested.begin(),requested.end()).size()!=requested.size())return reject(StreamResult::Rejected,"invalid_input_types");
    for(auto type:requested)if(type!=Type::Touch&&type!=Type::Mouse)return reject(StreamResult::Unsupported,"input_type_not_supported");
    if (!contains(capability_.input_types, request.input_type)&&request.input_types.empty())
        return reject(StreamResult::Unsupported, "input_type_not_supported");
    if (retired_streams_.size() >= 4096) return reject(StreamResult::Busy, "session_stream_budget_exhausted");
    auto negotiated = request;
    negotiated.input_types.clear();
    for(auto type:requested)if(contains(capability_.input_types,type))negotiated.input_types.push_back(type);
    if(negotiated.input_types.empty())return reject(StreamResult::Unsupported,"input_type_not_supported");
    negotiated.input_type=negotiated.input_types.front();
    negotiated.limits = intersect(request.limits, capability_.limits);
    negotiated.limits.allow_touch=contains(negotiated.input_types,Type::Touch)&&negotiated.limits.allow_touch;
    negotiated.limits.allow_mouse=contains(negotiated.input_types,Type::Mouse)&&negotiated.limits.allow_mouse;
    if (std::any_of(negotiated.input_types.begin(),negotiated.input_types.end(),[&](auto t){return !valid_limits(negotiated.limits,t);}) || capability_.max_frame_bytes < 64 ||
        capability_.max_frame_bytes > 65549)
        return reject(StreamResult::Unsupported, "input_limits_unavailable");
    // Retain cleanup identity before invoking an endpoint that may allocate and
    // then time out. A provider must reconcile this command before retrying open.
    snapshot_.handle = {request.authenticated_session, request.source_binding_id, 0, 0,
                        request.input_type, request.target};
    snapshot_.state = StreamState::Closing;
    try { result = open_(negotiated); }
    catch (...) {
        snapshot_.state = StreamState::Blocked;
        return reject(StreamResult::Unavailable, "endpoint_open_uncertain");
    }
    if (!result.accepted()) {
        snapshot_.state = result.cleanup_pending ? StreamState::Blocked : StreamState::Closed;
        if (result.cleanup_pending && result.handle.stream_id) snapshot_.handle = result.handle;
        if (result.reason.empty()) result.reason = "endpoint_open_rejected";
        snapshot_.last_reason = result.reason;
        return result;
    }
    if(result.handle.input_types.empty())result.handle.input_types={result.handle.input_type};
    const auto& accepted=result.handle.input_types;
    snapshot_.handle = result.handle; // Preserve even malformed accepted ownership for close.
    if (!same_target(result.handle.target, request.target) ||
        result.handle.authenticated_session != request.authenticated_session ||
        result.handle.source_binding_id != request.source_binding_id ||
        accepted.empty()||accepted.size()>2||std::set<Type>(accepted.begin(),accepted.end()).size()!=accepted.size()||
        std::any_of(accepted.begin(),accepted.end(),[&](auto t){return !contains(negotiated.input_types,t);}) || !result.handle.stream_id ||
        !result.handle.generation || result.format != request.format ||
        retired_streams_.count(result.handle.stream_id) || result.max_frame_bytes < 64 ||
        result.max_frame_bytes > capability_.max_frame_bytes ||
        std::any_of(accepted.begin(),accepted.end(),[&](auto t){return !valid_limits(result.limits,t)||!within(result.limits,negotiated.limits,t);})) {
        snapshot_.state = StreamState::Blocked;
        result.result = StreamResult::Rejected;
        result.cleanup_pending = true;
        result.reason = snapshot_.last_reason = "endpoint_open_result_mismatch";
        return result;
    }
    result.limits.allow_touch=contains(accepted,Type::Touch);
    result.limits.allow_mouse=contains(accepted,Type::Mouse);
    active_frame_bytes_ = result.max_frame_bytes;
    retired_streams_.insert(result.handle.stream_id);
    snapshot_.state = StreamState::Active;
    ++snapshot_.open_count;
    snapshot_.last_reason.clear();
    return result;
}

void InputStreamRelay::fence(std::string reason) {
    if (snapshot_.state == StreamState::Active) snapshot_.state = StreamState::Blocked;
    snapshot_.last_reason = std::move(reason);
}
StreamResult InputStreamRelay::forward(const StreamFrame& frame, std::string* reason) {
    const auto reject = [&](StreamResult result, const char* why, bool current = false) {
        ++snapshot_.rejected_frames;
        if (current) fence(why);
        if (reason) *reason = why;
        return result;
    };
    if (!same(frame.handle, snapshot_.handle)) return reject(StreamResult::Stale, "stale_stream_or_target");
    if (snapshot_.state != StreamState::Active)
        return reject(StreamResult::Stale, "stream_not_active");
    if (frame.bytes.empty() || frame.bytes.size() > active_frame_bytes_)
        return reject(StreamResult::Rejected, "frame_budget_exceeded", true);
    // Byte-transparent: provider/transport validates generic frames; endpoint
    // validates input payload. The relay does not inspect contacts or coordinates.
    try {
        if (!forward_ || !forward_(frame)) return reject(StreamResult::Unavailable, "downstream_rejected", true);
    } catch (...) { return reject(StreamResult::Unavailable, "downstream_write_uncertain", true); }
    ++snapshot_.forwarded_frames;
    if (reason) reason->clear();
    return StreamResult::Accepted;
}
StreamResult InputStreamRelay::close(const StreamHandle& handle, std::string reason) {
    if (!same(handle, snapshot_.handle)) return StreamResult::Stale;
    if (snapshot_.state == StreamState::Closed) return StreamResult::Closed;
    if (snapshot_.state != StreamState::Active && snapshot_.state != StreamState::Blocked)
        return StreamResult::InvalidState;
    snapshot_.state = StreamState::Closing; // Stop forwarding before calling cleanup.
    ++snapshot_.native_release_attempts;
    bool confirmed = false;
    std::string release_reason;
    try { confirmed = release_ && release_(handle, release_reason); }
    catch (...) { release_reason = "endpoint_release_uncertain"; }
    if (!confirmed) {
        snapshot_.state = StreamState::Blocked;
        snapshot_.last_reason = release_reason.empty() ? "cleanup_confirmation_unavailable" : release_reason.substr(0, 128);
        return StreamResult::Unavailable;
    }
    ++snapshot_.native_release_confirmed;
    snapshot_.state = StreamState::Closed;
    ++snapshot_.close_count;
    snapshot_.last_reason = std::move(reason);
    return StreamResult::Accepted;
}
StreamResult InputStreamRelay::retry_close(std::string reason) { return close(snapshot_.handle, std::move(reason)); }
StreamRouteSnapshot InputStreamRelay::snapshot() const { return snapshot_; }

InputStreamProducer::InputStreamProducer(InputStreamRelay& relay, FramingOptions framing)
    : relay_(relay), framing_(framing) {}
StreamOpenResult InputStreamProducer::open(const StreamOpenRequest& request) {
    if (open_.accepted()) {
        StreamOpenResult busy;
        busy.result = StreamResult::Busy;
        busy.reason = "stream_already_owned";
        return busy;
    }
    const auto result = relay_.open(request);
    if (!result.accepted()) return result; // Do not overwrite a retained owner.
    open_ = result;
    framing_.max_frame_bytes = std::min(framing_.max_frame_bytes ? framing_.max_frame_bytes : result.max_frame_bytes,
                                        result.max_frame_bytes);
    next_sequence_ = 1;
    last_cursor_ = 0;
    contacts_.clear();mouse_buttons_=0;
    return result;
}
StreamResult InputStreamProducer::send(const Packet& packet, std::string* reason) {
    if (!open_.accepted() || relay_.snapshot().state != StreamState::Active) {
        if (reason) *reason = "stream_not_active";
        return StreamResult::InvalidState;
    }
    const auto reject = [&](const char* why) {
        relay_.fence(why);
        if (reason) *reason = why;
        return StreamResult::Rejected;
    };
    if ((packet.stream_id && packet.stream_id != open_.handle.stream_id) ||
        !contains(open_.handle.input_types,packet.sample.type)) return reject("input_stream_or_type_mismatch");
    if (!next_sequence_ || packet.sequence != next_sequence_) return reject("input_sequence_mismatch");
    if (packet.sample_time_us < last_cursor_) return reject("input_cursor_regressed");
    auto next = contacts_;
    auto nextButtons=mouse_buttons_;
    if(packet.sample.type==Type::Mouse){
        const auto& mouse=packet.sample.mouse;
        if(mouse.kind==MouseKind::Cancel)nextButtons=0;
        else if(mouse.kind==MouseKind::Buttons){if(mouse.buttons==mouse_buttons_)return reject("mouse_buttons_unchanged");nextButtons=mouse.buttons;}
        else if((mouse.kind==MouseKind::Move||mouse.kind==MouseKind::Wheel)&&mouse.buttons!=mouse_buttons_)return reject("mouse_button_edge_missing");
    }
    if (packet.sample.type == Type::Touch) {
        std::set<std::uint16_t> present;
        for (const auto& contact : packet.sample.contacts) {
            if (!present.insert(contact.id).second) return reject("duplicate_contact");
            const bool existed = contacts_.count(contact.id) != 0;
            if (contact.phase == TouchPhase::Down) {
                if (existed) return reject("duplicate_contact_down");
                next.insert(contact.id);
            } else if (contact.phase == TouchPhase::Move || contact.phase == TouchPhase::Up || contact.phase == TouchPhase::Cancel) {
                if (!existed) return reject("orphan_contact");
                if (contact.phase != TouchPhase::Move) next.erase(contact.id);
            } else return reject("unknown_contact_phase");
        }
        if (!std::includes(present.begin(), present.end(), contacts_.begin(), contacts_.end()))
            return reject("incomplete_touch_sample");
    }
    auto encoded = packet;
    encoded.stream_id = open_.handle.stream_id;
    auto limits = open_.limits;
    limits.allow_touch = contains(open_.handle.input_types,Type::Touch);
    limits.allow_mouse = contains(open_.handle.input_types,Type::Mouse);
    std::vector<std::vector<std::uint8_t>> frames;
    std::string error;
    if (!encode_frames(encoded, frames, framing_, limits, &error)) return reject("invalid_input_sample");
    for (const auto& bytes : frames) {
        const auto result = relay_.forward({open_.handle, bytes}, reason);
        if (result != StreamResult::Accepted) return result; // Never replay partial samples.
    }
    ++next_sequence_;
    last_cursor_ = packet.sample_time_us;
    contacts_ = std::move(next);
    mouse_buttons_=nextButtons;
    return StreamResult::Accepted;
}
StreamResult InputStreamProducer::close(std::string reason) {
    if (!open_.accepted()) return relay_.retry_close(std::move(reason));
    const auto result = relay_.close(open_.handle, std::move(reason));
    if (result == StreamResult::Accepted || result == StreamResult::Closed) {
        open_ = {};
        contacts_.clear();mouse_buttons_=0;
    }
    return result;
}
StreamResult InputStreamProducer::retry_close(std::string reason) { return close(std::move(reason)); }
StreamRouteSnapshot InputStreamProducer::snapshot() const { return relay_.snapshot(); }
const StreamCapability& InputStreamProducer::capability() const noexcept { return relay_.capability(); }
} // namespace axent::input
