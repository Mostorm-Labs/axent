#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <set>
#include <string>
#include <vector>

#include "axent/input/input_stream.hpp"

namespace axent::input {

enum class StreamState { Unbound, Active, Closing, Closed, Blocked };
enum class StreamResult { Accepted, Rejected, Stale, Unsupported, InvalidState, Closed, Busy, Unavailable };

struct StreamCapability {
    std::string format = "input-sample-v1";
    std::vector<Type> input_types;
    Limits limits{};
    std::size_t max_frame_bytes = 4096;
    std::string authenticated_session;
    std::string source_binding_id;
    bool route_ready = false;
};

// Trusted input capability supplied by the authenticated media/device
// integration.  The producer's open request is never allowed to create this
// descriptor; it may only request a subset of it.
struct SourceDescriptor {
    StreamCapability capability;
    std::uint64_t physical_generation = 0;
};

// These fields are the existing R2 control target, never STREAM metadata.
struct StreamTarget {
    std::string runtime_instance_id;
    std::string session_id;
    std::uint64_t session_generation = 0;
    std::uint32_t slot_id = 0;
    std::uint64_t binding_generation = 0;
    std::uint64_t lifecycle_generation = 0;
};

struct StreamOpenRequest {
    std::string authenticated_session;
    std::string source_binding_id;
    std::string format = "input-sample-v1";
    Type input_type = Type::Touch;
    Limits limits{};
    StreamTarget target;
    std::string command_id;
    std::vector<Type> input_types; // Empty retains the legacy single-type request.
};

struct StreamHandle {
    std::string authenticated_session;
    std::string source_binding_id;
    std::uint32_t stream_id = 0;
    std::uint64_t generation = 0;
    Type input_type = Type::Touch;
    StreamTarget target;
    std::vector<Type> input_types; // Actual accepted set for this immutable context.
};

struct StreamOpenResult {
    StreamResult result = StreamResult::Rejected;
    StreamHandle handle;
    Limits limits{};
    std::size_t max_frame_bytes = 0;
    std::string reason;
    std::string format = "input-sample-v1";
    bool accepted() const noexcept { return result == StreamResult::Accepted; }
    bool cleanup_pending = false;
};

struct StreamFrame {
    StreamHandle handle;
    std::vector<std::uint8_t> bytes;
};

struct StreamRouteSnapshot {
    StreamState state = StreamState::Unbound;
    StreamHandle handle;
    std::size_t open_count = 0;
    std::size_t forwarded_frames = 0;
    std::size_t rejected_frames = 0;
    std::size_t close_count = 0;
    std::size_t native_release_attempts = 0;
    std::size_t native_release_confirmed = 0;
    std::string last_reason;
};

using FrameForwardCallback = std::function<bool(const StreamFrame&)>;
using StreamOpenCallback = std::function<StreamOpenResult(const StreamOpenRequest&)>;
using StreamReleaseCallback = std::function<bool(const StreamHandle&, std::string&)>;

// Serial-owner provider seam on ONE pre-authenticated logical AXTP session.
// Callbacks must use that same session for open, opaque frames, and close.
// This class neither establishes authentication nor implements device firmware.
// open returns the consumer's stream ID. release confirms consumer ownership
// cleanup, not successful forwarding of a close request. No callback may reenter.
class InputStreamRelay {
public:
    InputStreamRelay(StreamCapability capability,
                     StreamOpenCallback open,
                     FrameForwardCallback forward,
                     StreamReleaseCallback release = {});

    StreamOpenResult open(const StreamOpenRequest& request);
    StreamResult forward(const StreamFrame& frame, std::string* reason = nullptr);
    StreamResult close(const StreamHandle& handle, std::string reason = {});
    StreamResult retry_close(std::string reason = {});
    void fence(std::string reason);
    StreamRouteSnapshot snapshot() const;
    const StreamCapability& capability() const noexcept { return capability_; }

private:
    StreamCapability capability_;
    StreamOpenCallback open_;
    FrameForwardCallback forward_;
    StreamReleaseCallback release_;
    StreamRouteSnapshot snapshot_;
    std::set<std::uint32_t> retired_streams_;
    std::size_t active_frame_bytes_ = 0;
};

class InputStreamProducer {
public:
    InputStreamProducer(InputStreamRelay& relay, FramingOptions framing = {});

    StreamOpenResult open(const StreamOpenRequest& request);
    StreamResult send(const Packet& packet, std::string* reason = nullptr);
    StreamResult close(std::string reason = {});
    StreamResult retry_close(std::string reason = {});
    StreamRouteSnapshot snapshot() const;
    const StreamOpenResult& open_result() const noexcept { return open_; }
    const StreamCapability& capability() const noexcept;

private:
    InputStreamRelay& relay_;
    FramingOptions framing_;
    StreamOpenResult open_;
    std::uint32_t next_sequence_ = 1;
    std::uint64_t last_cursor_ = 0;
    std::set<std::uint16_t> contacts_;
    std::uint32_t mouse_buttons_=0;
};

} // namespace axent::input
