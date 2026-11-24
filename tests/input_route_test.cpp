#include <iostream>
#include <optional>
#include <stdexcept>
#include <vector>
#include "axent/input/input_route.hpp"

using namespace axent::input;
void require(bool value, const char* text) { if (!value) throw std::runtime_error(text); }
struct Fixture {
    StreamCapability capability;
    StreamOpenRequest request;
    std::vector<std::vector<std::uint8_t>> forwarded;
    std::uint32_t stream = 40;
    unsigned opens = 0, releases = 0;
    bool release_ok = true, forward_ok = true;
    bool throw_open = false, throw_forward = false, throw_release = false;
    bool invalid_result = false, reuse_id = false;
    FrameDecoder decoder{{}, 64};
    std::vector<Packet> native_submitted;
    Fixture() {
        capability.input_types = {Type::Touch};
        capability.limits.max_contacts = 10;
        capability.limits.allow_mouse = false;
        capability.max_frame_bytes = 64;
        capability.authenticated_session = "authenticated-route";
        capability.source_binding_id = "source-1";
        capability.route_ready = true;
        request.authenticated_session = capability.authenticated_session;
        request.source_binding_id = capability.source_binding_id;
        request.target = {"runtime-1", "media-1", 2, 0, 3, 4};
        request.command_id = "command-1";
        request.limits = capability.limits;
        request.limits.max_contacts = 5;
    }
    StreamOpenResult open(const StreamOpenRequest& value) {
        ++opens;
        if (throw_open) throw std::runtime_error("open transport lost");
        StreamOpenResult result;
        result.result = StreamResult::Accepted;
        if (!reuse_id) ++stream;
        result.handle = {value.authenticated_session, value.source_binding_id,
                         stream, 1, value.input_type, value.target};
        result.handle.input_types=value.input_types;
        result.limits = value.limits;
        result.max_frame_bytes = 64;
        if (invalid_result) ++result.limits.max_contacts;
        decoder.reset();
        return result;
    }
    bool forward(const StreamFrame& frame) {
        if (throw_forward) throw std::runtime_error("forward transport lost");
        if (!forward_ok) return false;
        forwarded.push_back(frame.bytes); // Fake NA20: never interpret typed input.
        std::optional<Packet> decoded;
        if (!decoder.consume(frame.bytes, decoded)) return false; // Fake NT10.
        if (decoded) native_submitted.push_back(*decoded);
        return true;
    }
    bool release(const StreamHandle&, std::string& reason) {
        ++releases;
        if (throw_release) throw std::runtime_error("release transport lost");
        if (!release_ok) { reason = "native_cleanup_pending"; return false; }
        decoder.reset();
        return true;
    }
    InputStreamRelay relay() {
        return InputStreamRelay(capability,
            [this](const auto& value) { return open(value); },
            [this](const auto& value) { return forward(value); },
            [this](const auto& value, auto& reason) { return release(value, reason); });
    }
};
Packet touch(std::uint32_t seq, TouchPhase phase, unsigned count = 5) {
    Packet result;
    result.sequence = seq;
    result.sample_time_us = seq * 100;
    for (unsigned i = 0; i < count; ++i)
        result.sample.contacts.push_back({static_cast<std::uint16_t>(i), phase, 1000, 2000});
    return result;
}
void transparent_route() {
    Fixture f; auto relay = f.relay(); InputStreamProducer producer(relay, {64, 1, 2, 9});
    const auto opened = producer.open(f.request);
    require(opened.accepted() && f.opens == 1, "endpoint did not allocate stream");
    auto packet = touch(1, TouchPhase::Down);
    require(producer.send(packet) == StreamResult::Accepted, "fragmented sample failed");
    packet.stream_id = opened.handle.stream_id;
    std::vector<std::vector<std::uint8_t>> expected;
    require(encode_frames(packet, expected, {64, 1, 2, 9}, opened.limits), "golden encode failed");
    require(expected == f.forwarded && expected.size() == 2, "relay modified complete AXTP frame bytes");
    require(f.native_submitted.size() == 1 && f.native_submitted[0].sample.contacts.size() == 5,
            "fragments did not become one consumer sample");
    require(producer.send(touch(2, TouchPhase::Move)) == StreamResult::Accepted, "move failed");
    require(producer.send(touch(3, TouchPhase::Up)) == StreamResult::Accepted, "up failed");
    const auto old = opened.handle;
    require(producer.close() == StreamResult::Accepted && f.releases == 1, "close did not release endpoint");
    const auto frames = f.forwarded.size();
    require(relay.forward({old, expected[0]}) == StreamResult::Stale, "post-close frame forwarded");
    const auto next = producer.open(f.request);
    require(next.accepted() && next.handle.stream_id != old.stream_id, "reopen reused stream");
    require(relay.forward({old, expected[0]}) == StreamResult::Stale, "retired generation crossed reopen");
    auto stale = next.handle; ++stale.target.binding_generation;
    require(relay.forward({stale, expected[0]}) == StreamResult::Stale, "changed target crossed fence");
    require(f.forwarded.size() == frames, "rejected frames reached endpoint");
}
void readiness_and_limits() {
    Fixture f; auto relay = f.relay();
    auto wrong = f.request; wrong.authenticated_session = "other";
    require(!relay.open(wrong).accepted() && f.opens == 0, "wrong authentication reached endpoint");
    wrong = f.request; wrong.source_binding_id = "other";
    require(!relay.open(wrong).accepted() && f.opens == 0, "wrong binding reached endpoint");
    wrong = f.request; wrong.target.lifecycle_generation = 0;
    require(!relay.open(wrong).accepted() && f.opens == 0, "incomplete target accepted");
    wrong = f.request; wrong.input_type = Type::Mouse;
    require(relay.open(wrong).result == StreamResult::Unsupported, "Touch became Mouse");
    f.capability.route_ready = false;
    auto unavailable = f.relay();
    require(!unavailable.open(f.request).accepted(), "unready route opened");
    Fixture narrower; auto narrowRelay = narrower.relay(); InputStreamProducer p(narrowRelay);
    auto request = narrower.request; request.limits.max_contacts = 32;
    const auto opened = p.open(request);
    require(opened.accepted() && opened.limits.max_contacts == 10,
            "limits did not negotiate capability/request intersection");
}
void cleanup_and_provider_failures() {
    Fixture f; auto relay = f.relay(); InputStreamProducer producer(relay);
    require(producer.open(f.request).accepted(), "fixture open failed");
    const auto old = producer.open_result().handle;
    auto wrong = f.request; wrong.authenticated_session = "other";
    require(!producer.open(wrong).accepted() && producer.open_result().handle.stream_id == old.stream_id,
            "duplicate open overwrote owner or accepted another session");
    f.release_ok = false;
    require(producer.close() == StreamResult::Unavailable &&
            relay.snapshot().state == StreamState::Blocked, "unconfirmed cleanup became Closed");
    require(!producer.open(f.request).accepted(), "blocked owner reactivated");
    require(producer.send(touch(1, TouchPhase::Down)) != StreamResult::Accepted, "blocked owner sent input");
    f.release_ok = true;
    require(producer.retry_close() == StreamResult::Accepted, "release retry failed");
    require(relay.snapshot().native_release_confirmed == 1, "release confirmation not recorded");
    f.reuse_id = true;
    require(!producer.open(f.request).accepted(), "consumer reused retired stream ID");
    Fixture missing;
    InputStreamRelay noRelease(missing.capability,
        [&](const auto& value) { return missing.open(value); }, {});
    require(noRelease.open(missing.request).accepted(), "missing release fixture failed");
    require(noRelease.retry_close() == StreamResult::Unavailable, "absent cleanup callback became success");
    Fixture malformed; malformed.invalid_result = true;
    auto badRelay = malformed.relay();
    require(!badRelay.open(malformed.request).accepted() && badRelay.snapshot().state == StreamState::Blocked,
            "invalid accepted open lost unknown native ownership");
    require(!badRelay.open(malformed.request).accepted() && malformed.opens == 1,
            "invalid accepted open allowed another prepare");
    require(badRelay.retry_close() == StreamResult::Accepted, "invalid open resource could not be cleaned");
    Fixture failed; failed.throw_open = true;
    auto failedRelay = failed.relay();
    require(!failedRelay.open(failed.request).accepted() && failedRelay.snapshot().state == StreamState::Blocked,
            "open exception did not retain uncertain cleanup");
}
void sample_fencing() {
    const auto check = [](Packet invalid) {
        Fixture f; auto relay = f.relay(); InputStreamProducer producer(relay);
        require(producer.open(f.request).accepted(), "sample fixture failed");
        require(producer.send(touch(1, TouchPhase::Down)) == StreamResult::Accepted, "initial sample failed");
        const auto count = f.forwarded.size();
        require(producer.send(invalid) != StreamResult::Accepted, "invalid sample accepted");
        require(relay.snapshot().state == StreamState::Blocked, "invalid sample did not fence context");
        require(producer.send(touch(2, TouchPhase::Move)) != StreamResult::Accepted,
                "input continued after current-flow fault");
        require(f.forwarded.size() == count, "invalid sample reached consumer");
        require(producer.close() == StreamResult::Accepted, "fenced owner failed to close");
    };
    check(touch(3, TouchPhase::Move));
    check(touch(1, TouchPhase::Move));
    check(touch(2, TouchPhase::Down));
    check(touch(2, TouchPhase::Move, 4));
    auto invalid = touch(2, TouchPhase::Move); invalid.sample_time_us = 1; check(invalid);
    invalid = touch(2, TouchPhase::Move); invalid.sample.contacts[1].id = 0; check(invalid);
    invalid = touch(2, TouchPhase::Move); invalid.sample.type = Type::Mouse; check(invalid);
    invalid = touch(2, TouchPhase::Move); invalid.stream_id = 999; check(invalid);
    Fixture f; auto relay = f.relay(); InputStreamProducer producer(relay);
    require(producer.open(f.request).accepted(), "forward failure fixture failed");
    f.forward_ok = false;
    require(producer.send(touch(1, TouchPhase::Down)) != StreamResult::Accepted &&
            relay.snapshot().state == StreamState::Blocked, "failed write did not fence partial sample");
    Fixture thrown; auto throwingRelay = thrown.relay(); InputStreamProducer throwingProducer(throwingRelay);
    require(throwingProducer.open(thrown.request).accepted(), "throw fixture failed");
    thrown.throw_forward = true;
    require(throwingProducer.send(touch(1, TouchPhase::Down)) != StreamResult::Accepted &&
            throwingRelay.snapshot().state == StreamState::Blocked, "write exception did not fence");
    thrown.throw_release = true;
    require(throwingProducer.close() == StreamResult::Unavailable, "release exception escaped");
}

void mixed_mouse_route(){
    Fixture f;f.capability.input_types={Type::Touch,Type::Mouse};f.capability.limits.allow_mouse=true;f.request.input_types=f.capability.input_types;f.request.limits=f.capability.limits;
    auto relay=f.relay();InputStreamProducer p(relay);require(p.open(f.request).accepted(),"mixed open failed");
    require(p.send(touch(1,TouchPhase::Down))==StreamResult::Accepted,"mixed Touch Down");
    Packet mouse;mouse.sequence=2;mouse.sample_time_us=200;mouse.sample.type=Type::Mouse;mouse.sample.mouse.kind=MouseKind::Buttons;mouse.sample.mouse.buttons=1;
    require(p.send(mouse)==StreamResult::Accepted,"mixed Mouse Down");
    require(p.send(touch(3,TouchPhase::Up))==StreamResult::Accepted,"Mouse erased Touch owner");
    mouse.sequence=4;mouse.sample_time_us=400;mouse.sample.mouse.buttons=0;
    require(p.send(mouse)==StreamResult::Accepted&&f.native_submitted.size()==4&&f.native_submitted[1].sample.type==Type::Mouse,"consumer did not decode mixed typed frames");
    mouse.sequence=5;mouse.sample_time_us=500;mouse.sample.mouse.kind=MouseKind::Move;mouse.sample.mouse.buttons=1;
    require(p.send(mouse)==StreamResult::Rejected,"orphan Mouse edge accepted");
    require(p.close()==StreamResult::Accepted,"Mouse fault cleanup");
    Fixture only;only.capability.input_types={Type::Mouse};only.capability.limits.allow_touch=false;only.capability.limits.allow_mouse=true;only.request.input_types={Type::Touch,Type::Mouse};only.request.limits=only.capability.limits;
    auto r=only.relay();InputStreamProducer producer(r);auto result=producer.open(only.request);
    require(result.accepted()&&result.handle.input_types==std::vector<Type>{Type::Mouse}&&!result.limits.allow_touch,"Mouse-only intersection");
}
int main() {
    try {
        mixed_mouse_route();
        transparent_route(); readiness_and_limits(); cleanup_and_provider_failures(); sample_fencing();
        std::cout << "Input provider seam: transparent frames, endpoint ownership, limits and lifecycle fencing passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
