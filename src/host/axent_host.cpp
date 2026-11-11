#include "axent/host/axent_host.hpp"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#include "axent/adapters/axtp_adapter.hpp"
#include "axent/adapters/mock_adapter.hpp"
#include "axent/core/broker.hpp"
#include "axent/core/device_manager.hpp"
#include "axent/core/flow_control.hpp"
#include "axent/core/middleware.hpp"
#include "axent/core/route_manager.hpp"
#include "axent/core/session_manager.hpp"
#include "axent/logging/logger.hpp"

namespace axent {

namespace {

thread_local bool g_in_media_stream_sink_callback = false;

std::string session_operation_key(const std::string& session_id)
{
    return "session:" + session_id;
}

std::string endpoint_operation_key(const std::string& endpoint_id)
{
    return "endpoint:" + endpoint_id;
}

void log_endpoint_binding_rejection(Logger* logger,
                                    const DeviceSnapshot& device,
                                    DeviceUpsertResult result)
{
    if (logger == nullptr || result.accepted()) {
        return;
    }
    const char* status = "discovery_claim_conflict";
    switch (result.status) {
    case DeviceUpsertStatus::EndpointConflict:
        status = "endpoint_conflict";
        break;
    case DeviceUpsertStatus::EndpointChangeRejected:
        status = "endpoint_change_rejected";
        break;
    case DeviceUpsertStatus::DiscoveryClaimConflict:
        break;
    case DeviceUpsertStatus::Inserted:
    case DeviceUpsertStatus::Refreshed:
    case DeviceUpsertStatus::EndpointBound:
        return;
    }
    logger->write(
        LogLevel::Error,
        LogCategory::Diagnostics,
        "device.endpoint.binding_rejected",
        {{"deviceId", device.id},
         {"adapter", device.adapter},
         {"endpointId", device.endpoint_id},
         {"status", status}});
}

class MediaStreamSinkCallbackMarker final {
public:
    MediaStreamSinkCallbackMarker()
        : previous_(g_in_media_stream_sink_callback)
    {
        g_in_media_stream_sink_callback = true;
    }

    ~MediaStreamSinkCallbackMarker()
    {
        g_in_media_stream_sink_callback = previous_;
    }

private:
    bool previous_ = false;
};

class HostLeaseRegistry final {
public:
    bool acquire_activity(const std::string& device_id, bool media, std::string& reason)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& state = devices_[device_id];
        if (state.maintenance) {
            reason = "maintenance lease busy";
            return false;
        }
        if (media) {
            ++state.media;
        } else {
            ++state.control;
        }
        return true;
    }

    void release_activity(const std::string& device_id, bool media) noexcept
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto entry = devices_.find(device_id);
        if (entry == devices_.end()) {
            return;
        }
        auto& state = entry->second;
        auto& count = media ? state.media : state.control;
        if (count != 0) {
            --count;
        }
        erase_if_unused(entry);
    }

    bool acquire_maintenance(const std::string& device_id, std::string& reason)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& state = devices_[device_id];
        if (state.maintenance || state.control != 0 || state.media != 0) {
            reason = "device is busy with an active control, media, or maintenance lease";
            return false;
        }
        state.maintenance = true;
        return true;
    }

    void release_maintenance(const std::string& device_id) noexcept
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto entry = devices_.find(device_id);
        if (entry == devices_.end()) {
            return;
        }
        entry->second.maintenance = false;
        erase_if_unused(entry);
    }

private:
    struct DeviceState {
        std::size_t control = 0;
        std::size_t media = 0;
        bool maintenance = false;
    };

    using Entry = std::map<std::string, DeviceState>::iterator;

    void erase_if_unused(Entry entry)
    {
        const auto& state = entry->second;
        if (state.control == 0 && state.media == 0 && !state.maintenance) {
            devices_.erase(entry);
        }
    }

    std::mutex mutex_;
    std::map<std::string, DeviceState> devices_;
};

class ActivityReservation final {
public:
    ActivityReservation(std::shared_ptr<HostLeaseRegistry> registry,
                        std::string device_id,
                        bool media)
        : registry_(std::move(registry))
        , device_id_(std::move(device_id))
        , media_(media)
    {
    }

    ~ActivityReservation()
    {
        if (active_ && registry_) {
            registry_->release_activity(device_id_, media_);
        }
    }

    ActivityReservation(const ActivityReservation&) = delete;
    ActivityReservation& operator=(const ActivityReservation&) = delete;

    void commit() noexcept
    {
        active_ = false;
    }

private:
    std::shared_ptr<HostLeaseRegistry> registry_;
    std::string device_id_;
    bool media_ = false;
    bool active_ = true;
};

class HostMaintenanceLeaseProvider final
    : public firmware::MaintenanceLeaseProvider {
public:
    using Reserve = std::function<bool(const std::string&,
                                       std::string&,
                                       std::function<void()>&)>;

    explicit HostMaintenanceLeaseProvider(Reserve reserve)
        : reserve_(std::move(reserve))
    {
    }

    firmware::MaintenanceLease try_acquire_maintenance(
        const std::string& device_id,
        std::string& reason) override
    {
        std::function<void()> release;
        if (!reserve_(device_id, reason, release)) {
            return {};
        }
        return grant_maintenance(device_id, std::move(release));
    }

private:
    Reserve reserve_;
};

class MediaSubscriptionState final : public MediaSubscription {
public:
    MediaSubscriptionState(std::shared_ptr<IMediaFrameSink> sink, MediaSubscriptionOptions options)
        : core_(std::make_shared<Core>(std::move(sink), options))
    {
        core_->start();
    }

    ~MediaSubscriptionState() override
    {
        cancel();
    }

    MediaSubscriptionState(const MediaSubscriptionState&) = delete;
    MediaSubscriptionState& operator=(const MediaSubscriptionState&) = delete;

    void publish(MediaFrame frame)
    {
        core_->publish(std::move(frame));
    }

    void cancel() override
    {
        core_->cancel();
    }

    MediaDeliveryStats stats() const override
    {
        return core_->stats();
    }

    bool active() const
    {
        return core_->active();
    }

private:
    class Core final : public std::enable_shared_from_this<Core> {
    public:
        Core(std::shared_ptr<IMediaFrameSink> sink, MediaSubscriptionOptions options)
            : sink_(std::move(sink))
            , options_(options)
        {
        }

        ~Core()
        {
            cancel();
        }

        Core(const Core&) = delete;
        Core& operator=(const Core&) = delete;

        void start()
        {
            if (options_.dispatch == MediaSubscriptionDispatch::Direct) {
                return;
            }
            auto self = shared_from_this();
            worker_ = std::thread([self]() { self->run(); });
        }

        void publish(MediaFrame frame)
        {
            if (options_.dispatch == MediaSubscriptionDispatch::Direct) {
                publish_direct(std::move(frame));
                return;
            }

            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (closed_) {
                    return;
                }

                queued_bytes_ += frame.payload.size();
                queue_.push_back(std::move(frame));
                ++stats_.received_frames;

                bool dropped = false;
                while (over_limit_locked() && !queue_.empty()) {
                    drop_front_locked();
                    dropped = true;
                }
                if (dropped && !queue_.empty()) {
                    queue_.front().flags |= MediaFrameFlag::Discontinuity;
                }
                if (dropped) {
                    MediaEvent event{
                        MediaEventKind::Dropped,
                        stats_.dropped_frames,
                        stats_.dropped_bytes,
                    };
                    if (!events_.empty() && events_.back().kind == MediaEventKind::Dropped) {
                        events_.back() = event;
                    } else {
                        events_.push_back(event);
                    }
                }
                refresh_stats_locked();
            }
            cv_.notify_one();
        }

        void cancel()
        {
            std::thread worker;
            std::shared_ptr<IMediaFrameSink> sink;
            bool self_cancel = false;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                if (closed_) {
                    if (callback_depths_.find(std::this_thread::get_id()) !=
                        callback_depths_.end()) {
                        return;
                    }
                    callback_cv_.wait(lock, [this]() {
                        return cancel_complete_ && callbacks_in_flight_ == 0;
                    });
                    return;
                }
                closed_ = true;
                queue_.clear();
                events_.clear();
                queued_bytes_ = 0;
                refresh_stats_locked();
                worker = std::move(worker_);
                sink = sink_;
                self_cancel = callback_depths_.find(std::this_thread::get_id()) !=
                    callback_depths_.end();
            }
            cv_.notify_all();
            if (worker.joinable() && worker.get_id() != std::this_thread::get_id()) {
                worker.join();
            } else if (worker.joinable()) {
                worker.detach();
            }
            if (!self_cancel) {
                std::unique_lock<std::mutex> lock(mutex_);
                callback_cv_.wait(lock, [this]() {
                    return callbacks_in_flight_ == 0;
                });
            }
            if (sink) {
                dispatch_event(sink, MediaEvent{MediaEventKind::Closed, 0, 0});
            }
            {
                std::lock_guard<std::mutex> lock(mutex_);
                cancel_complete_ = true;
            }
            callback_cv_.notify_all();
        }

        MediaDeliveryStats stats() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto snapshot = stats_;
            snapshot.queued_frames = queue_.size();
            snapshot.queued_bytes = queued_bytes_;
            return snapshot;
        }

        bool active() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return !closed_;
        }

    private:
        void publish_direct(MediaFrame frame)
        {
            std::shared_ptr<IMediaFrameSink> sink;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (closed_) {
                    return;
                }
                ++stats_.received_frames;
                sink = sink_;
                if (sink) {
                    begin_callback_locked();
                }
            }

            if (sink) {
                dispatch_frame(sink, std::move(frame), true);
                std::lock_guard<std::mutex> lock(mutex_);
                ++stats_.delivered_frames;
            }
        }

        void begin_callback_locked()
        {
            ++callbacks_in_flight_;
            ++callback_depths_[std::this_thread::get_id()];
        }

        void begin_callback()
        {
            std::lock_guard<std::mutex> lock(mutex_);
            begin_callback_locked();
        }

        void end_callback()
        {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                --callbacks_in_flight_;
                const auto callback = callback_depths_.find(std::this_thread::get_id());
                if (callback != callback_depths_.end() && --callback->second == 0) {
                    callback_depths_.erase(callback);
                }
            }
            callback_cv_.notify_all();
        }

        void dispatch_frame(const std::shared_ptr<IMediaFrameSink>& sink,
                            MediaFrame frame,
                            bool callback_reserved = false)
        {
            if (!callback_reserved) {
                begin_callback();
            }
            MediaStreamSinkCallbackMarker callback_marker;
            try {
                sink->on_media_frame(std::move(frame));
            } catch (...) {
            }
            end_callback();
        }

        void dispatch_event(const std::shared_ptr<IMediaFrameSink>& sink,
                            MediaEvent event)
        {
            begin_callback();
            MediaStreamSinkCallbackMarker callback_marker;
            try {
                sink->on_media_event(event);
            } catch (...) {
            }
            end_callback();
        }

        bool over_limit_locked() const
        {
            const bool frame_limited = options_.max_frames != 0 && queue_.size() > options_.max_frames;
            const bool byte_limited = options_.max_bytes != 0 && queued_bytes_ > options_.max_bytes;
            return frame_limited || byte_limited;
        }

        void drop_front_locked()
        {
            const auto dropped_bytes = queue_.front().payload.size();
            queued_bytes_ -= dropped_bytes;
            queue_.pop_front();
            ++stats_.dropped_frames;
            stats_.dropped_bytes += dropped_bytes;
        }

        void refresh_stats_locked()
        {
            stats_.queued_frames = queue_.size();
            stats_.queued_bytes = queued_bytes_;
        }

        void run()
        {
            for (;;) {
                MediaFrame frame;
                MediaEvent event;
                bool has_frame = false;
                bool has_event = false;
                {
                    std::unique_lock<std::mutex> lock(mutex_);
                    cv_.wait(lock, [this]() {
                        return closed_ || !queue_.empty() || !events_.empty();
                    });
                    if (!events_.empty()) {
                        event = events_.front();
                        events_.pop_front();
                        has_event = true;
                    } else if (!queue_.empty()) {
                        frame = std::move(queue_.front());
                        queued_bytes_ -= frame.payload.size();
                        queue_.pop_front();
                        refresh_stats_locked();
                        has_frame = true;
                    } else {
                        if (closed_) {
                            break;
                        }
                        continue;
                    }
                }

                if (has_event && sink_) {
                    dispatch_event(sink_, event);
                    continue;
                }

                if (has_frame && sink_) {
                    dispatch_frame(sink_, std::move(frame));
                }
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    if (has_frame) {
                        ++stats_.delivered_frames;
                    }
                }
            }
        }

        std::shared_ptr<IMediaFrameSink> sink_;
        MediaSubscriptionOptions options_;
        mutable std::mutex mutex_;
        std::condition_variable cv_;
        std::condition_variable callback_cv_;
        std::deque<MediaFrame> queue_;
        std::deque<MediaEvent> events_;
        std::size_t queued_bytes_ = 0;
        MediaDeliveryStats stats_;
        bool closed_ = false;
        bool cancel_complete_ = false;
        std::size_t callbacks_in_flight_ = 0;
        std::map<std::thread::id, std::size_t> callback_depths_;
        std::thread worker_;
    };

    std::shared_ptr<Core> core_;
};

class MediaStreamSubscriptionState final : public MediaStreamSubscription {
public:
    MediaStreamSubscriptionState(std::shared_ptr<IMediaStreamSink> sink,
                                 MediaSubscriptionOptions options)
        : core_(std::make_shared<Core>(std::move(sink), options))
    {
    }

    ~MediaStreamSubscriptionState() override
    {
        cancel();
    }

    MediaStreamSubscriptionState(const MediaStreamSubscriptionState&) = delete;
    MediaStreamSubscriptionState& operator=(const MediaStreamSubscriptionState&) = delete;

    void prime_opened(const std::vector<MediaStreamDescriptor>& descriptors)
    {
        for (const auto& descriptor : descriptors) {
            core_->prime_stream_event({MediaStreamEventKind::Opened,
                                       descriptor,
                                       MediaStreamEventReason::Snapshot});
        }
    }

    void activate()
    {
        core_->activate();
    }

    void publish_stream_event(MediaStreamEvent event)
    {
        core_->publish_stream_event(std::move(event));
    }

    void publish_frame(MediaFrame frame)
    {
        core_->publish_frame(std::move(frame));
    }

    void close_streams_and_cancel()
    {
        core_->cancel(true);
    }

    void cancel() override
    {
        core_->cancel(false);
    }

    MediaDeliveryStats stats() const override
    {
        return core_->stats();
    }

    bool active() const
    {
        return core_->active();
    }

private:
    class Core final : public std::enable_shared_from_this<Core> {
    public:
        Core(std::shared_ptr<IMediaStreamSink> sink, MediaSubscriptionOptions options)
            : sink_(std::move(sink))
            , options_(options)
        {
        }

        ~Core()
        {
            if (worker_.joinable()) {
                worker_.detach();
            }
        }

        Core(const Core&) = delete;
        Core& operator=(const Core&) = delete;

        void activate()
        {
            bool drain_direct = false;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (activated_) {
                    return;
                }
                activated_ = true;
                if (options_.dispatch == MediaSubscriptionDispatch::AsyncQueued) {
                    start_worker_locked();
                } else {
                    drain_direct = true;
                }
            }
            cv_.notify_all();
            terminal_cv_.notify_all();
            if (drain_direct) {
                drain_direct_queue();
            }
        }

        void prime_stream_event(MediaStreamEvent event)
        {
            publish_stream_event_impl(std::move(event), true);
        }

        void publish_stream_event(MediaStreamEvent event)
        {
            publish_stream_event_impl(std::move(event), false);
        }

        void publish_stream_event_impl(MediaStreamEvent event, bool priming)
        {
            bool drain_direct = false;
            bool direct_owned = false;
            std::shared_ptr<DirectCompletion> direct_completion;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (!accepting_ || event.descriptor.key.generation == 0) {
                    return;
                }

                const auto stream_id = event.descriptor.key.stream_id;
                const auto active = active_streams_.find(stream_id);
                std::vector<Item> new_items;
                if (event.kind == MediaStreamEventKind::Opened) {
                    if (active != active_streams_.end() &&
                        active->second.key == event.descriptor.key) {
                        return;
                    }
                    if (active != active_streams_.end()) {
                        new_items.push_back(Item::stream_event(
                            {MediaStreamEventKind::Closed,
                             active->second,
                             event.reason}));
                    }
                    active_streams_[stream_id] = event.descriptor;
                    new_items.push_back(Item::stream_event(std::move(event)));
                } else {
                    if (active == active_streams_.end() ||
                        active->second.key != event.descriptor.key) {
                        return;
                    }
                    new_items.push_back(Item::stream_event(std::move(event)));
                    active_streams_.erase(active);
                }
                direct_owned = !priming &&
                    options_.dispatch == MediaSubscriptionDispatch::Direct;
                const bool reentrant_direct = direct_owned && delivering_ &&
                    delivery_thread_ == std::this_thread::get_id();
                drain_direct = direct_owned && !reentrant_direct;
                if (direct_owned && !reentrant_direct) {
                    for (auto& item : new_items) {
                        item.direct_owner = std::this_thread::get_id();
                    }
                }
                if (drain_direct) {
                    direct_completion = std::make_shared<DirectCompletion>();
                    new_items.back().direct_completion = direct_completion;
                }
                for (auto& item : new_items) {
                    items_.push_back(std::move(item));
                }
            }
            cv_.notify_one();
            if (drain_direct) {
                drain_direct_queue(std::move(direct_completion));
            }
        }

        void publish_frame(MediaFrame frame)
        {
            bool drain_direct = false;
            bool direct_owned = false;
            std::shared_ptr<DirectCompletion> direct_completion;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (!accepting_ || frame.generation == 0) {
                    return;
                }
                const auto active = active_streams_.find(frame.stream_id);
                if (active == active_streams_.end() ||
                    active->second.key != stream_key(frame)) {
                    return;
                }

                queued_bytes_ += frame.payload.size();
                ++queued_frames_;
                ++stats_.received_frames;
                auto item = Item::frame(std::move(frame));

                direct_owned =
                    options_.dispatch == MediaSubscriptionDispatch::Direct;
                const bool reentrant_direct = direct_owned && delivering_ &&
                    delivery_thread_ == std::this_thread::get_id();
                drain_direct = direct_owned && !reentrant_direct;
                if (direct_owned && !reentrant_direct) {
                    item.direct_owner = std::this_thread::get_id();
                }
                if (drain_direct) {
                    direct_completion = std::make_shared<DirectCompletion>();
                    item.direct_completion = direct_completion;
                }
                items_.push_back(std::move(item));

                if (options_.dispatch == MediaSubscriptionDispatch::AsyncQueued) {
                    bool dropped = false;
                    while (over_limit_locked() && queued_frames_ != 0) {
                        drop_oldest_frame_locked();
                        dropped = true;
                    }
                    if (dropped) {
                        mark_next_frame_discontinuous_locked();
                        enqueue_drop_event_locked();
                    }
                }
                refresh_stats_locked();
            }
            cv_.notify_one();
            if (drain_direct) {
                drain_direct_queue(std::move(direct_completion));
            }
        }

        void cancel(bool close_streams)
        {
            std::thread worker;
            bool drain_direct = false;
            bool self_cancel = false;
            std::shared_ptr<DirectCompletion> direct_completion;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                if (terminal_delivered_) {
                    if (delivering_ &&
                        delivery_thread_ != std::this_thread::get_id()) {
                        terminal_cv_.wait(lock, [this]() { return !delivering_; });
                    }
                    return;
                }
                if (terminal_enqueued_) {
                    if (delivery_thread_ == std::this_thread::get_id()) {
                        return;
                    }
                    terminal_cv_.wait(lock, [this]() { return terminal_delivered_; });
                    return;
                }

                direct_completion = prepare_terminal_locked(close_streams);
                if (!activated_) {
                    activated_ = true;
                }
                self_cancel = delivering_ &&
                    delivery_thread_ == std::this_thread::get_id();
                if (self_cancel) {
                    // A sink may cancel or release its own subscription from a
                    // callback. Do not nest lifecycle callbacks or leave work
                    // for after that callback returns.
                    items_.clear();
                    queued_frames_ = 0;
                    queued_bytes_ = 0;
                    refresh_stats_locked();
                    terminal_delivered_ = true;
                    if (worker_.joinable() &&
                        worker_.get_id() == std::this_thread::get_id()) {
                        worker_.detach();
                    }
                } else if (options_.dispatch == MediaSubscriptionDispatch::AsyncQueued) {
                    start_worker_locked();
                    if (worker_.joinable()) {
                        worker = std::move(worker_);
                    }
                } else {
                    drain_direct = true;
                }
            }
            cv_.notify_all();

            if (self_cancel) {
                terminal_cv_.notify_all();
                return;
            }
            if (worker.joinable()) {
                worker.join();
                return;
            }
            if (drain_direct) {
                drain_direct_queue(std::move(direct_completion));
            }
        }

        MediaDeliveryStats stats() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto snapshot = stats_;
            snapshot.queued_frames = queued_frames_;
            snapshot.queued_bytes = queued_bytes_;
            return snapshot;
        }

        bool active() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return accepting_ && !terminal_enqueued_;
        }

    private:
        enum class ItemKind {
            StreamEvent,
            Frame,
            DeliveryEvent,
        };

        struct DirectCompletion {
            bool completed = false;
        };

        struct Item {
            static Item stream_event(MediaStreamEvent value)
            {
                Item item;
                item.kind = ItemKind::StreamEvent;
                item.stream = std::move(value);
                return item;
            }

            static Item frame(MediaFrame value)
            {
                Item item;
                item.kind = ItemKind::Frame;
                item.media_frame = std::move(value);
                return item;
            }

            static Item delivery_event(MediaDeliveryEvent value)
            {
                Item item;
                item.kind = ItemKind::DeliveryEvent;
                item.delivery = value;
                return item;
            }

            ItemKind kind = ItemKind::StreamEvent;
            MediaStreamEvent stream;
            MediaFrame media_frame;
            MediaDeliveryEvent delivery;
            std::thread::id direct_owner;
            std::shared_ptr<DirectCompletion> direct_completion;
        };

        void start_worker_locked()
        {
            if (worker_.joinable()) {
                return;
            }
            auto self = shared_from_this();
            worker_ = std::thread([self]() { self->run(); });
        }

        bool over_limit_locked() const
        {
            const bool frame_limited = options_.max_frames != 0 &&
                queued_frames_ > options_.max_frames;
            const bool byte_limited = options_.max_bytes != 0 &&
                queued_bytes_ > options_.max_bytes;
            return frame_limited || byte_limited;
        }

        void drop_oldest_frame_locked()
        {
            for (auto item = items_.begin(); item != items_.end(); ++item) {
                if (item->kind != ItemKind::Frame) {
                    continue;
                }
                const auto bytes = item->media_frame.payload.size();
                queued_bytes_ -= bytes;
                --queued_frames_;
                ++stats_.dropped_frames;
                stats_.dropped_bytes += bytes;
                items_.erase(item);
                return;
            }
        }

        void mark_next_frame_discontinuous_locked()
        {
            for (auto& item : items_) {
                if (item.kind == ItemKind::Frame) {
                    item.media_frame.flags |= MediaFrameFlag::Discontinuity;
                    return;
                }
            }
        }

        void enqueue_drop_event_locked()
        {
            const MediaDeliveryEvent event{
                MediaDeliveryEventKind::DeliveryDropped,
                stats_.dropped_frames,
                stats_.dropped_bytes,
            };
            for (auto item = items_.rbegin(); item != items_.rend(); ++item) {
                if (item->kind == ItemKind::StreamEvent) {
                    break;
                }
                if (item->kind == ItemKind::DeliveryEvent &&
                    item->delivery.kind == MediaDeliveryEventKind::DeliveryDropped) {
                    item->delivery = event;
                    return;
                }
            }
            const auto next_frame = std::find_if(
                items_.begin(), items_.end(), [](const Item& item) {
                    return item.kind == ItemKind::Frame;
                });
            items_.insert(next_frame, Item::delivery_event(event));
        }

        void refresh_stats_locked()
        {
            stats_.queued_frames = queued_frames_;
            stats_.queued_bytes = queued_bytes_;
        }

        void clear_pending_locked()
        {
            items_.clear();
            queued_frames_ = 0;
            queued_bytes_ = 0;
            refresh_stats_locked();
        }

        std::shared_ptr<DirectCompletion> prepare_terminal_locked(bool close_streams)
        {
            accepting_ = false;
            clear_pending_locked();
            if (close_streams) {
                for (const auto& entry : active_streams_) {
                    const auto delivered = delivered_streams_.find(entry.first);
                    if (delivered != delivered_streams_.end()) {
                        items_.push_back(Item::stream_event(
                            {MediaStreamEventKind::Closed,
                             delivered->second,
                             MediaStreamEventReason::Shutdown}));
                    }
                    if (delivered == delivered_streams_.end() ||
                        delivered->second.key != entry.second.key) {
                        items_.push_back(Item::stream_event(
                            {MediaStreamEventKind::Opened,
                             entry.second,
                             MediaStreamEventReason::Snapshot}));
                        items_.push_back(Item::stream_event(
                            {MediaStreamEventKind::Closed,
                             entry.second,
                             MediaStreamEventReason::Shutdown}));
                    }
                }
                for (const auto& delivered : delivered_streams_) {
                    if (active_streams_.find(delivered.first) != active_streams_.end()) {
                        continue;
                    }
                    items_.push_back(Item::stream_event(
                        {MediaStreamEventKind::Closed,
                         delivered.second,
                         MediaStreamEventReason::Shutdown}));
                }
            }
            active_streams_.clear();
            items_.push_back(Item::delivery_event(
                {MediaDeliveryEventKind::SubscriptionClosed, 0, 0}));
            terminal_enqueued_ = true;
            if (activated_ && options_.dispatch == MediaSubscriptionDispatch::Direct) {
                auto completion = std::make_shared<DirectCompletion>();
                for (auto& item : items_) {
                    item.direct_owner = std::this_thread::get_id();
                }
                items_.back().direct_completion = completion;
                return completion;
            }
            return {};
        }

        void update_delivered_lifecycle_locked(const Item& item)
        {
            if (item.kind != ItemKind::StreamEvent) {
                return;
            }
            const auto stream_id = item.stream.descriptor.key.stream_id;
            if (item.stream.kind == MediaStreamEventKind::Opened) {
                delivered_streams_[stream_id] = item.stream.descriptor;
                return;
            }
            const auto delivered = delivered_streams_.find(stream_id);
            if (delivered != delivered_streams_.end() &&
                delivered->second.key == item.stream.descriptor.key) {
                delivered_streams_.erase(delivered);
            }
        }

        bool take_next_item_locked(Item& item)
        {
            if (items_.empty()) {
                return false;
            }
            item = std::move(items_.front());
            items_.pop_front();
            if (item.kind == ItemKind::Frame) {
                queued_bytes_ -= item.media_frame.payload.size();
                --queued_frames_;
                refresh_stats_locked();
            }
            update_delivered_lifecycle_locked(item);
            return true;
        }

        void dispatch_item(Item& item)
        {
            if (!sink_) {
                return;
            }
            MediaStreamSinkCallbackMarker callback_marker;
            try {
                switch (item.kind) {
                case ItemKind::StreamEvent:
                    sink_->on_media_stream_event(std::move(item.stream));
                    break;
                case ItemKind::Frame:
                    sink_->on_media_stream_frame(std::move(item.media_frame));
                    break;
                case ItemKind::DeliveryEvent:
                    sink_->on_media_delivery_event(item.delivery);
                    break;
                }
            } catch (...) {
                // A sink exception must not strand the delivery owner or kill
                // the async worker. The callback is treated as consumed.
            }
        }

        void finish_item_locked(const Item& item)
        {
            if (item.kind == ItemKind::Frame) {
                ++stats_.delivered_frames;
            }
            if (item.kind == ItemKind::DeliveryEvent &&
                item.delivery.kind == MediaDeliveryEventKind::SubscriptionClosed) {
                terminal_delivered_ = true;
            }
            if (item.direct_completion) {
                item.direct_completion->completed = true;
            }
            terminal_cv_.notify_all();
        }

        void drain_direct_queue(std::shared_ptr<DirectCompletion> target = {})
        {
            bool owns_delivery = false;
            const auto current_thread = std::this_thread::get_id();
            for (;;) {
                Item item;
                {
                    std::unique_lock<std::mutex> lock(mutex_);
                    auto release_delivery = [this, &owns_delivery]() {
                        if (owns_delivery) {
                            delivering_ = false;
                            delivery_thread_ = {};
                            owns_delivery = false;
                            terminal_cv_.notify_all();
                        }
                    };

                    for (;;) {
                        if (terminal_delivered_) {
                            release_delivery();
                            return;
                        }
                        if (target && target->completed) {
                            const bool has_deferred_current_item =
                                owns_delivery && !items_.empty() &&
                                (items_.front().direct_owner == std::thread::id{} ||
                                 items_.front().direct_owner == current_thread);
                            if (has_deferred_current_item) {
                                target.reset();
                            } else {
                                release_delivery();
                                return;
                            }
                        }
                        if (!activated_) {
                            if (!target) {
                                return;
                            }
                            terminal_cv_.wait(lock, [this, &target]() {
                                return activated_ || terminal_delivered_ ||
                                    (target && target->completed);
                            });
                            continue;
                        }

                        if (delivering_ && delivery_thread_ != current_thread) {
                            terminal_cv_.wait(lock, [this, &target, current_thread]() {
                                return !delivering_ ||
                                    delivery_thread_ == current_thread ||
                                    terminal_delivered_ ||
                                    (target && target->completed);
                            });
                            continue;
                        }

                        const bool nested_invocation =
                            delivering_ && delivery_thread_ == current_thread &&
                            !owns_delivery;
                        if (nested_invocation) {
                            return;
                        }
                        const bool front_is_eligible = !items_.empty() &&
                            (items_.front().direct_owner == std::thread::id{} ||
                             items_.front().direct_owner == current_thread);
                        if (!front_is_eligible) {
                            release_delivery();
                            if (!target) {
                                return;
                            }
                            terminal_cv_.wait(lock, [this, &target, current_thread]() {
                                if (terminal_delivered_ ||
                                    (target && target->completed)) {
                                    return true;
                                }
                                if (!activated_ || delivering_ || items_.empty()) {
                                    return false;
                                }
                                return items_.front().direct_owner == std::thread::id{} ||
                                    items_.front().direct_owner == current_thread;
                            });
                            continue;
                        }

                        if (!delivering_) {
                            delivering_ = true;
                            delivery_thread_ = current_thread;
                            owns_delivery = true;
                        }
                        item = std::move(items_.front());
                        items_.pop_front();
                        if (item.kind == ItemKind::Frame) {
                            queued_bytes_ -= item.media_frame.payload.size();
                            --queued_frames_;
                            refresh_stats_locked();
                        }
                        update_delivered_lifecycle_locked(item);
                        break;
                    }
                }
                dispatch_item(item);
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    finish_item_locked(item);
                    if (terminal_delivered_) {
                        if (owns_delivery) {
                            delivering_ = false;
                            delivery_thread_ = {};
                            owns_delivery = false;
                            terminal_cv_.notify_all();
                        }
                        return;
                    }
                    if (target && target->completed) {
                        const bool has_deferred_current_item =
                            owns_delivery && !items_.empty() &&
                            (items_.front().direct_owner == std::thread::id{} ||
                             items_.front().direct_owner == current_thread);
                        if (has_deferred_current_item) {
                            target.reset();
                        } else {
                            if (owns_delivery) {
                                delivering_ = false;
                                delivery_thread_ = {};
                                owns_delivery = false;
                                terminal_cv_.notify_all();
                            }
                            return;
                        }
                    }
                }
            }
        }

        void run()
        {
            for (;;) {
                Item item;
                {
                    std::unique_lock<std::mutex> lock(mutex_);
                    cv_.wait(lock, [this]() {
                        return terminal_delivered_ || (activated_ && !items_.empty());
                    });
                    if (terminal_delivered_) {
                        return;
                    }
                    if (!take_next_item_locked(item)) {
                        continue;
                    }
                    delivering_ = true;
                    delivery_thread_ = std::this_thread::get_id();
                }
                dispatch_item(item);
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    delivering_ = false;
                    delivery_thread_ = {};
                    finish_item_locked(item);
                    terminal_cv_.notify_all();
                    if (terminal_delivered_) {
                        return;
                    }
                }
            }
        }

        std::shared_ptr<IMediaStreamSink> sink_;
        MediaSubscriptionOptions options_;
        mutable std::mutex mutex_;
        std::condition_variable cv_;
        std::condition_variable terminal_cv_;
        std::deque<Item> items_;
        std::map<std::uint32_t, MediaStreamDescriptor> active_streams_;
        std::map<std::uint32_t, MediaStreamDescriptor> delivered_streams_;
        std::size_t queued_frames_ = 0;
        std::size_t queued_bytes_ = 0;
        MediaDeliveryStats stats_;
        bool accepting_ = true;
        bool activated_ = false;
        bool delivering_ = false;
        bool terminal_enqueued_ = false;
        bool terminal_delivered_ = false;
        std::thread::id delivery_thread_;
        std::thread worker_;
    };

    std::shared_ptr<Core> core_;
};

} // namespace

struct AxentHost::Impl {
    struct ResetSubscriptions {
        std::vector<std::shared_ptr<MediaSubscriptionState>> legacy;
        std::vector<std::shared_ptr<MediaStreamSubscriptionState>> streams;
    };

    ResetSubscriptions reset();
    std::optional<SessionLease> lease_for_session_locked(const std::string& session_id) const;
    std::vector<std::shared_ptr<MediaSubscriptionState>> take_session_subscriptions_locked(
        const std::string& session_id);
    std::vector<std::shared_ptr<MediaSubscriptionState>> subscriptions_for_session_locked(const std::string& session_id);
    std::vector<std::shared_ptr<MediaStreamSubscriptionState>> take_session_stream_subscriptions_locked(
        const std::string& session_id);
    std::vector<std::shared_ptr<MediaStreamSubscriptionState>> stream_subscriptions_for_session_locked(
        const std::string& session_id);
    std::vector<ControlOperationPtr> take_control_operations_locked(
        const std::optional<std::string>& operation_key = std::nullopt);
    bool is_axtp_device_locked(const std::string& device_id) const;
    bool has_lease_for_device_locked(const std::string& device_id) const;

    mutable std::mutex mutex;
    bool running = false;
    AxentHostOptions options;
    std::unique_ptr<Logger> logger;
    std::unique_ptr<Adapter> axtp_adapter;
    std::unique_ptr<MockAdapter> mock_adapter;
    std::unique_ptr<DeviceManager> devices;
    std::unique_ptr<RouteManager> routes;
    std::unique_ptr<Middleware> middleware;
    std::unique_ptr<FlowControl> flow;
    std::unique_ptr<Broker> broker;
    SessionManager sessions;
    std::map<std::string, SessionLease> leases;
    std::map<std::string, std::shared_ptr<MediaStreamRelay>> relays;
    std::map<std::string, std::vector<std::weak_ptr<MediaSubscriptionState>>> subscriptions;
    std::map<std::string, std::vector<std::weak_ptr<MediaStreamSubscriptionState>>> stream_subscriptions;
    std::map<std::string, std::vector<std::weak_ptr<ControlOperation>>> control_operations;
    std::map<std::string, std::map<std::uint32_t, MediaStreamDescriptor>> active_media_streams;
    std::map<std::string, std::string> media_owner_session_by_device;
    std::shared_ptr<HostLeaseRegistry> lease_registry = std::make_shared<HostLeaseRegistry>();
    std::unique_ptr<firmware::MaintenanceLeaseProvider> maintenance_provider;
    std::mutex dispatch_mutex;
};

AxentHost::Impl::ResetSubscriptions AxentHost::Impl::reset()
{
    ResetSubscriptions subscriptions_to_close;
    for (auto& entry : subscriptions) {
        for (auto& weak_subscription : entry.second) {
            if (auto subscription = weak_subscription.lock()) {
                subscriptions_to_close.legacy.push_back(std::move(subscription));
            }
        }
    }
    subscriptions.clear();
    for (auto& entry : stream_subscriptions) {
        for (auto& weak_subscription : entry.second) {
            if (auto subscription = weak_subscription.lock()) {
                subscriptions_to_close.streams.push_back(std::move(subscription));
            }
        }
    }
    stream_subscriptions.clear();
    control_operations.clear();
    active_media_streams.clear();
    for (auto& entry : relays) {
        if (entry.second) {
            entry.second->close();
        }
    }
    relays.clear();
    for (const auto& entry : leases) {
        lease_registry->release_activity(entry.second.device_id, entry.second.media);
    }
    leases.clear();
    media_owner_session_by_device.clear();
    broker.reset();
    flow.reset();
    middleware.reset();
    routes.reset();
    devices.reset();
    mock_adapter.reset();
    axtp_adapter.reset();
    logger.reset();
    sessions = SessionManager{};
    running = false;
    return subscriptions_to_close;
}

std::vector<ControlOperationPtr> AxentHost::Impl::take_control_operations_locked(
    const std::optional<std::string>& operation_key)
{
    std::vector<ControlOperationPtr> operations;
    const auto take = [&operations](auto& weak_operations) {
        for (auto& weak_operation : weak_operations) {
            if (auto operation = weak_operation.lock()) {
                operations.push_back(std::move(operation));
            }
        }
    };
    if (operation_key.has_value()) {
        const auto entry = control_operations.find(*operation_key);
        if (entry != control_operations.end()) {
            take(entry->second);
            control_operations.erase(entry);
        }
        return operations;
    }
    for (auto& entry : control_operations) {
        take(entry.second);
    }
    control_operations.clear();
    return operations;
}

AxentHostOptions::AxentHostOptions()
    : axtp(AxtpAdapter::na20_defaults())
{
}

std::optional<SessionLease> AxentHost::Impl::lease_for_session_locked(const std::string& session_id) const
{
    const auto it = leases.find(session_id);
    if (it == leases.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<std::shared_ptr<MediaSubscriptionState>> AxentHost::Impl::take_session_subscriptions_locked(
    const std::string& session_id)
{
    std::vector<std::shared_ptr<MediaSubscriptionState>> subscriptions_to_close;
    const auto it = subscriptions.find(session_id);
    if (it != subscriptions.end()) {
        for (auto& weak_subscription : it->second) {
            if (auto subscription = weak_subscription.lock()) {
                subscriptions_to_close.push_back(std::move(subscription));
            }
        }
        subscriptions.erase(it);
    }
    return subscriptions_to_close;
}

std::vector<std::shared_ptr<MediaSubscriptionState>> AxentHost::Impl::subscriptions_for_session_locked(
    const std::string& session_id)
{
    std::vector<std::shared_ptr<MediaSubscriptionState>> result;
    const auto it = subscriptions.find(session_id);
    if (it == subscriptions.end()) {
        return result;
    }
    auto& session_subscriptions = it->second;
    for (auto weak_subscription = session_subscriptions.begin();
         weak_subscription != session_subscriptions.end();) {
        if (auto subscription = weak_subscription->lock()) {
            if (subscription->active()) {
                result.push_back(subscription);
            }
            ++weak_subscription;
        } else {
            weak_subscription = session_subscriptions.erase(weak_subscription);
        }
    }
    return result;
}

std::vector<std::shared_ptr<MediaStreamSubscriptionState>>
AxentHost::Impl::take_session_stream_subscriptions_locked(const std::string& session_id)
{
    std::vector<std::shared_ptr<MediaStreamSubscriptionState>> subscriptions_to_close;
    const auto it = stream_subscriptions.find(session_id);
    if (it != stream_subscriptions.end()) {
        for (auto& weak_subscription : it->second) {
            if (auto subscription = weak_subscription.lock()) {
                subscriptions_to_close.push_back(std::move(subscription));
            }
        }
        stream_subscriptions.erase(it);
    }
    return subscriptions_to_close;
}

std::vector<std::shared_ptr<MediaStreamSubscriptionState>>
AxentHost::Impl::stream_subscriptions_for_session_locked(const std::string& session_id)
{
    std::vector<std::shared_ptr<MediaStreamSubscriptionState>> result;
    const auto it = stream_subscriptions.find(session_id);
    if (it == stream_subscriptions.end()) {
        return result;
    }
    auto& session_subscriptions = it->second;
    for (auto weak_subscription = session_subscriptions.begin();
         weak_subscription != session_subscriptions.end();) {
        if (auto subscription = weak_subscription->lock()) {
            if (subscription->active()) {
                result.push_back(subscription);
            }
            ++weak_subscription;
        } else {
            weak_subscription = session_subscriptions.erase(weak_subscription);
        }
    }
    return result;
}

bool AxentHost::Impl::is_axtp_device_locked(const std::string& device_id) const
{
    if (!devices) {
        return false;
    }
    const auto device = devices->get(device_id);
    return device.has_value() && device->adapter == "axtp";
}

bool AxentHost::Impl::has_lease_for_device_locked(const std::string& device_id) const
{
    for (const auto& entry : leases) {
        if (entry.second.device_id == device_id) {
            return true;
        }
    }
    return false;
}

MediaConsumer::MediaConsumer(std::shared_ptr<MediaStreamRelay> relay)
    : relay_(std::move(relay))
{
}

std::optional<MediaFrame> MediaConsumer::read()
{
    return relay_ ? relay_->read() : std::nullopt;
}

MediaRelayStats MediaConsumer::stats() const
{
    return relay_ ? relay_->stats() : MediaRelayStats{};
}

AxentHost::AxentHost()
    : impl_(std::make_unique<Impl>())
{
    impl_->maintenance_provider = std::make_unique<HostMaintenanceLeaseProvider>(
        [this](const std::string& device_id,
               std::string& reason,
               std::function<void()>& release) {
            return reserve_maintenance(device_id, reason, release);
        });
}

AxentHost::~AxentHost()
{
    stop();
}

bool AxentHost::start(AxentHostOptions options)
{
    if (g_in_media_stream_sink_callback) {
        return false;
    }
    std::lock_guard<std::mutex> dispatch_lock(impl_->dispatch_mutex);
    std::unique_ptr<Adapter> previous_axtp_adapter;
    Impl::ResetSubscriptions subscriptions_to_close;
    std::vector<ControlOperationPtr> operations_to_cancel;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        operations_to_cancel = impl_->take_control_operations_locked();
    }
    for (auto& operation : operations_to_cancel) {
        operation->cancel();
    }
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        previous_axtp_adapter = std::move(impl_->axtp_adapter);
        subscriptions_to_close = impl_->reset();
    }
    for (auto& subscription : subscriptions_to_close.legacy) {
        subscription->cancel();
    }
    for (auto& subscription : subscriptions_to_close.streams) {
        subscription->close_streams_and_cancel();
    }
    if (auto* axtp_adapter = dynamic_cast<AxtpAdapter*>(previous_axtp_adapter.get())) {
        axtp_adapter->set_media_frame_callback({});
        axtp_adapter->set_media_stream_event_callback({});
    }
    previous_axtp_adapter.reset();

    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->options = options;
    impl_->logger = std::make_unique<Logger>();
    impl_->devices = std::make_unique<DeviceManager>();
    impl_->routes = std::make_unique<RouteManager>(*impl_->devices);
    impl_->middleware = std::make_unique<Middleware>(*impl_->logger);
    impl_->flow = std::make_unique<FlowControl>();
    impl_->broker = std::make_unique<Broker>(*impl_->routes, *impl_->middleware, *impl_->flow);

    if (impl_->options.enable_mock_adapter) {
        impl_->mock_adapter = std::make_unique<MockAdapter>();
        for (const auto& device : impl_->mock_adapter->discover()) {
            const auto result = impl_->devices->upsert(device);
            log_endpoint_binding_rejection(impl_->logger.get(), device, result);
        }
        impl_->broker->register_adapter(*impl_->mock_adapter);
    }
    if (impl_->options.enable_axtp_adapter) {
        if (impl_->options.axtp_adapter_factory) {
            impl_->axtp_adapter = impl_->options.axtp_adapter_factory(impl_->options.axtp);
        } else {
            impl_->axtp_adapter = std::make_unique<AxtpAdapter>(impl_->options.axtp);
        }
        if (auto* axtp_adapter = dynamic_cast<AxtpAdapter*>(impl_->axtp_adapter.get())) {
            axtp_adapter->set_media_frame_callback(
                [this](std::string device_id, MediaFrame frame) {
                    this->publish_media_frame_for_device(std::move(device_id), std::move(frame));
                });
            axtp_adapter->set_media_stream_event_callback(
                [this](MediaStreamEvent event) {
                    this->publish_media_stream_event_for_device(std::move(event));
                });
        }
        for (const auto& device : impl_->axtp_adapter->discover()) {
            const auto result = impl_->devices->upsert(device);
            log_endpoint_binding_rejection(impl_->logger.get(), device, result);
        }
        impl_->broker->register_adapter(*impl_->axtp_adapter);
    }

    impl_->running = true;
    return true;
}

void AxentHost::stop()
{
    if (g_in_media_stream_sink_callback) {
        throw std::logic_error(
            "AxentHost::stop cannot run synchronously from a media stream callback");
    }
    std::lock_guard<std::mutex> dispatch_lock(impl_->dispatch_mutex);
    std::unique_ptr<Adapter> axtp_adapter;
    Impl::ResetSubscriptions subscriptions_to_close;
    std::vector<ControlOperationPtr> operations_to_cancel;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (!impl_->running && !impl_->broker) {
            return;
        }
        operations_to_cancel = impl_->take_control_operations_locked();
    }
    for (auto& operation : operations_to_cancel) {
        operation->cancel();
    }
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        axtp_adapter = std::move(impl_->axtp_adapter);
        subscriptions_to_close = impl_->reset();
    }
    for (auto& subscription : subscriptions_to_close.legacy) {
        subscription->cancel();
    }
    for (auto& subscription : subscriptions_to_close.streams) {
        subscription->close_streams_and_cancel();
    }
    if (auto* adapter = dynamic_cast<AxtpAdapter*>(axtp_adapter.get())) {
        adapter->set_media_frame_callback({});
        adapter->set_media_stream_event_callback({});
    }
}

bool AxentHost::running() const
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->running;
}

std::vector<DeviceSnapshot> AxentHost::discover_devices() const
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->devices ? impl_->devices->list() : std::vector<DeviceSnapshot>{};
}

std::vector<DeviceSnapshot> AxentHost::refresh_devices()
{
    std::lock_guard<std::mutex> dispatch_lock(impl_->dispatch_mutex);
    Adapter* adapter = nullptr;
    std::string adapter_name;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (!impl_->running || !impl_->devices || !impl_->axtp_adapter) {
            return {};
        }
        adapter = impl_->axtp_adapter.get();
        adapter_name = adapter->metadata().name;
    }

    const auto discovered = adapter->discover();

    std::lock_guard<std::mutex> lock(impl_->mutex);
    const auto results = impl_->devices->reconcile_discovery(
        adapter_name, discovered, "discovery-missing");
    for (std::size_t index = 0; index < discovered.size(); ++index) {
        log_endpoint_binding_rejection(
            impl_->logger.get(), discovered[index], results[index]);
    }
    return impl_->devices->list();
}

TransportDiagnostics AxentHost::transport_diagnostics() const
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (const auto* adapter = dynamic_cast<const AxtpAdapter*>(impl_->axtp_adapter.get())) {
        return adapter->diagnostics();
    }
    return {};
}

TransportDiagnostics AxentHost::transport_diagnostics(const std::string& device_id) const
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (const auto* adapter = dynamic_cast<const AxtpAdapter*>(impl_->axtp_adapter.get())) {
        return adapter->diagnostics(device_id);
    }
    return {};
}

void AxentHost::upsert_device(DeviceSnapshot snapshot)
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->devices) {
        const auto result = impl_->devices->upsert(snapshot);
        log_endpoint_binding_rejection(impl_->logger.get(), snapshot, result);
    }
}

SessionLease AxentHost::acquire_session(const SessionAcquireRequest& request)
{
    if (g_in_media_stream_sink_callback) {
        return {false,
                "",
                request.device_id,
                request.client_id,
                request.media,
                "host lifecycle unavailable during media stream callback",
                ControlStatus::Busy};
    }
    std::lock_guard<std::mutex> dispatch_lock(impl_->dispatch_mutex);
    AxtpAdapter* session_adapter = nullptr;
    AxtpAdapter* media_adapter = nullptr;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (!impl_->running || !impl_->devices) {
            return {false, "", request.device_id, request.client_id, false, "host not running",
                    ControlStatus::Unavailable};
        }
        const auto device = impl_->devices->get(request.device_id);
        if (!device.has_value()) {
            return {false, "", request.device_id, request.client_id, false, "device not found",
                    ControlStatus::NotFound};
        }
        if (request.media
            && impl_->media_owner_session_by_device.find(request.device_id)
                   != impl_->media_owner_session_by_device.end()) {
            return {false, "", request.device_id, request.client_id, false, "media lease busy",
                    ControlStatus::Busy};
        }
        if (device->adapter == "axtp") {
            session_adapter = dynamic_cast<AxtpAdapter*>(impl_->axtp_adapter.get());
        }
        if (request.media && device->adapter == "axtp") {
            media_adapter = session_adapter;
        }
    }

    std::string activity_reason;
    if (!impl_->lease_registry->acquire_activity(
            request.device_id, request.media, activity_reason)) {
        return {false,
                "",
                request.device_id,
                request.client_id,
                request.media,
                std::move(activity_reason),
                ControlStatus::Busy};
    }
    ActivityReservation activity(impl_->lease_registry, request.device_id, request.media);

    if (session_adapter != nullptr) {
        std::string error;
        const auto status = session_adapter->open_session_status(
            request.device_id, error, request.media);
        if (status != ControlStatus::Ok) {
            return {false, "", request.device_id, request.client_id, true, error, status};
        }
    }

    SessionLease lease;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        const auto device = impl_->devices->get(request.device_id);
        if (!device.has_value()) {
            return {false, "", request.device_id, request.client_id, false, "device not found",
                    ControlStatus::NotFound};
        }
        if (request.media
            && impl_->media_owner_session_by_device.find(request.device_id)
                   != impl_->media_owner_session_by_device.end()) {
            return {false, "", request.device_id, request.client_id, false, "media lease busy",
                    ControlStatus::Busy};
        }
        const std::string session_id =
            impl_->sessions.device().open(request.device_id, device->adapter);
        lease = {true, session_id, request.device_id, request.client_id, request.media, ""};
        impl_->leases[session_id] = lease;
        if (request.media) {
            impl_->media_owner_session_by_device[request.device_id] = session_id;
        }
    }
    activity.commit();
    if (media_adapter != nullptr) {
        media_adapter->bind_media_delivery_session(lease.device_id, lease.session_id);
    }
    return lease;
}

void AxentHost::release_session(const std::string& session_id, const std::string&)
{
    if (g_in_media_stream_sink_callback) {
        throw std::logic_error(
            "AxentHost::release_session cannot run synchronously from a media stream callback");
    }
    std::lock_guard<std::mutex> dispatch_lock(impl_->dispatch_mutex);
    std::string reset_device_id;
    AxtpAdapter* reset_adapter = nullptr;
    AxtpAdapter* media_adapter_to_unbind = nullptr;
    std::string media_device_id;
    std::vector<std::shared_ptr<MediaSubscriptionState>> subscriptions_to_close;
    std::vector<std::shared_ptr<MediaStreamSubscriptionState>> stream_subscriptions_to_close;
    std::vector<ControlOperationPtr> operations_to_cancel;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        const auto lease = impl_->lease_for_session_locked(session_id);
        if (lease.has_value() && lease->media) {
            const auto owner = impl_->media_owner_session_by_device.find(lease->device_id);
            if (owner != impl_->media_owner_session_by_device.end() && owner->second == session_id) {
                impl_->media_owner_session_by_device.erase(owner);
            }
            if (impl_->is_axtp_device_locked(lease->device_id)) {
                media_adapter_to_unbind =
                    dynamic_cast<AxtpAdapter*>(impl_->axtp_adapter.get());
                media_device_id = lease->device_id;
            }
        }
        const auto relay = impl_->relays.find(session_id);
        if (relay != impl_->relays.end()) {
            relay->second->close();
            impl_->relays.erase(relay);
        }
        subscriptions_to_close = impl_->take_session_subscriptions_locked(session_id);
        stream_subscriptions_to_close =
            impl_->take_session_stream_subscriptions_locked(session_id);
        impl_->active_media_streams.erase(session_id);
        operations_to_cancel =
            impl_->take_control_operations_locked(session_operation_key(session_id));
        if (lease.has_value()) {
            impl_->sessions.close_device_session(session_id);
        }
        impl_->leases.erase(session_id);
        if (lease.has_value()) {
            impl_->lease_registry->release_activity(lease->device_id, lease->media);
        }
        if (lease.has_value() &&
            impl_->is_axtp_device_locked(lease->device_id) &&
            !impl_->has_lease_for_device_locked(lease->device_id)) {
            reset_device_id = lease->device_id;
            reset_adapter = dynamic_cast<AxtpAdapter*>(impl_->axtp_adapter.get());
        }
    }
    for (auto& operation : operations_to_cancel) {
        operation->cancel();
    }
    if (media_adapter_to_unbind != nullptr) {
        media_adapter_to_unbind->unbind_media_delivery_session(
            media_device_id, session_id);
    }
    for (auto& subscription : subscriptions_to_close) {
        subscription->cancel();
    }
    for (auto& subscription : stream_subscriptions_to_close) {
        subscription->close_streams_and_cancel();
    }
    if (reset_adapter != nullptr && !reset_device_id.empty()) {
        reset_adapter->reset_session_for_device(reset_device_id);
    }
}

firmware::MaintenanceLeaseProvider& AxentHost::maintenance_lease_provider()
{
    return *impl_->maintenance_provider;
}

bool AxentHost::reserve_maintenance(
    const std::string& device_id,
    std::string& reason,
    std::function<void()>& release)
{
    if (g_in_media_stream_sink_callback) {
        reason = "host lifecycle unavailable during media stream callback";
        return false;
    }
    std::lock_guard<std::mutex> dispatch_lock(impl_->dispatch_mutex);
    std::shared_ptr<HostLeaseRegistry> registry;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (!impl_->running || !impl_->devices) {
            reason = "host not running";
            return false;
        }
        if (!impl_->devices->get(device_id).has_value()) {
            reason = "device not found";
            return false;
        }
        registry = impl_->lease_registry;
    }
    if (!registry->acquire_maintenance(device_id, reason)) {
        return false;
    }
    release = [registry = std::move(registry), device_id]() {
        registry->release_maintenance(device_id);
    };
    return true;
}

std::unique_ptr<MediaConsumer> AxentHost::create_media_consumer(const std::string& session_id,
                                                                MediaRelayOptions options)
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    const auto lease = impl_->lease_for_session_locked(session_id);
    if (!lease.has_value() || !lease->media) {
        return nullptr;
    }

    auto& relay = impl_->relays[session_id];
    if (!relay) {
        relay = std::make_shared<MediaStreamRelay>(options);
    }
    return std::unique_ptr<MediaConsumer>(new MediaConsumer(relay));
}

MediaSubscriptionPtr AxentHost::subscribe_media(const std::string& session_id,
                                                std::shared_ptr<IMediaFrameSink> sink,
                                                MediaSubscriptionOptions options)
{
    if (!sink) {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(impl_->mutex);
    const auto lease = impl_->lease_for_session_locked(session_id);
    if (!lease.has_value() || !lease->media) {
        return nullptr;
    }

    auto subscription = std::make_shared<MediaSubscriptionState>(std::move(sink), options);
    impl_->subscriptions[session_id].push_back(subscription);
    return subscription;
}

MediaStreamSubscriptionPtr AxentHost::subscribe_media_stream(
    const std::string& session_id,
    std::shared_ptr<IMediaStreamSink> sink,
    MediaSubscriptionOptions options)
{
    if (!sink) {
        return nullptr;
    }

    std::shared_ptr<MediaStreamSubscriptionState> subscription;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        const auto lease = impl_->lease_for_session_locked(session_id);
        if (!lease.has_value() || !lease->media) {
            return nullptr;
        }

        auto& active = impl_->active_media_streams[session_id];
        if (auto* adapter = dynamic_cast<AxtpAdapter*>(impl_->axtp_adapter.get());
            adapter != nullptr && impl_->is_axtp_device_locked(lease->device_id)) {
            for (auto descriptor : adapter->active_media_stream_descriptors(lease->device_id)) {
                if (!descriptor.device_id.empty() && descriptor.device_id != lease->device_id) {
                    continue;
                }
                descriptor.device_id = lease->device_id;
                descriptor.key.session_id = session_id;
                active[descriptor.key.stream_id] = std::move(descriptor);
            }
        }

        std::vector<MediaStreamDescriptor> descriptors;
        descriptors.reserve(active.size());
        for (const auto& entry : active) {
            descriptors.push_back(entry.second);
        }
        subscription =
            std::make_shared<MediaStreamSubscriptionState>(std::move(sink), options);
        subscription->prime_opened(descriptors);
        impl_->stream_subscriptions[session_id].push_back(subscription);
    }

    // Activation happens outside the Host lock. Any frame/lifecycle callback
    // that races registration is queued behind the primed Opened snapshot.
    subscription->activate();
    return subscription;
}

bool AxentHost::publish_media_stream_event(const std::string& session_id,
                                           MediaStreamEvent event)
{
    if (g_in_media_stream_sink_callback) {
        return false;
    }
    return publish_media_stream_event_for_session(
        session_id, std::move(event));
}

bool AxentHost::publish_media_stream_event_for_session(
    const std::string& session_id,
    MediaStreamEvent event)
{
    std::vector<std::shared_ptr<MediaStreamSubscriptionState>> subscriptions;
    std::vector<MediaStreamEvent> ordered_events;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        const auto lease = impl_->lease_for_session_locked(session_id);
        if (!lease.has_value() || !lease->media ||
            event.descriptor.key.stream_id == 0 ||
            event.descriptor.key.generation == 0) {
            return false;
        }

        event.descriptor.device_id = lease->device_id;
        event.descriptor.key.session_id = lease->session_id;
        auto& active = impl_->active_media_streams[lease->session_id];
        const auto current = active.find(event.descriptor.key.stream_id);
        if (event.kind == MediaStreamEventKind::Opened) {
            if (current != active.end() &&
                current->second.key == event.descriptor.key) {
                return true;
            }
            if (current != active.end()) {
                ordered_events.push_back(
                    {MediaStreamEventKind::Closed,
                     current->second,
                     event.reason});
            }
            active[event.descriptor.key.stream_id] = event.descriptor;
            ordered_events.push_back(std::move(event));
        } else {
            if (current == active.end() ||
                current->second.key != event.descriptor.key) {
                return true;
            }
            ordered_events.push_back(
                {MediaStreamEventKind::Closed,
                 current->second,
                 event.reason});
            active.erase(current);
        }
        subscriptions =
            impl_->stream_subscriptions_for_session_locked(lease->session_id);
    }

    for (const auto& ordered_event : ordered_events) {
        for (const auto& subscription : subscriptions) {
            subscription->publish_stream_event(ordered_event);
        }
    }
    return true;
}

bool AxentHost::publish_media_frame(const std::string& session_id, MediaFrame frame)
{
    if (g_in_media_stream_sink_callback) {
        return false;
    }
    std::shared_ptr<MediaStreamRelay> relay;
    std::vector<std::shared_ptr<MediaSubscriptionState>> subscriptions;
    std::vector<std::shared_ptr<MediaStreamSubscriptionState>> stream_subscriptions;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        const auto lease = impl_->lease_for_session_locked(session_id);
        if (!lease.has_value() || !lease->media) {
            return false;
        }
        const auto relay_it = impl_->relays.find(session_id);
        subscriptions = impl_->subscriptions_for_session_locked(session_id);
        stream_subscriptions = impl_->stream_subscriptions_for_session_locked(session_id);
        if ((relay_it == impl_->relays.end() || !relay_it->second) &&
            subscriptions.empty() && stream_subscriptions.empty()) {
            return false;
        }
        frame.session_id = session_id;
        frame.device_id = lease->device_id;
        if (relay_it != impl_->relays.end()) {
            relay = relay_it->second;
        }
    }
    if (relay) {
        relay->publish(frame);
    }
    for (const auto& subscription : subscriptions) {
        subscription->publish(frame);
    }
    for (const auto& subscription : stream_subscriptions) {
        subscription->publish_frame(frame);
    }
    return true;
}

bool AxentHost::publish_media_frame_for_device(std::string device_id, MediaFrame frame)
{
    std::shared_ptr<MediaStreamRelay> relay;
    std::vector<std::shared_ptr<MediaSubscriptionState>> subscriptions;
    std::vector<std::shared_ptr<MediaStreamSubscriptionState>> stream_subscriptions;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        const auto owner = impl_->media_owner_session_by_device.find(device_id);
        if (owner == impl_->media_owner_session_by_device.end()) {
            return false;
        }
        const auto lease = impl_->lease_for_session_locked(owner->second);
        if (!lease.has_value() || !lease->media) {
            return false;
        }
        if (frame.session_id.empty() || frame.session_id != lease->session_id) {
            return false;
        }
        const auto relay_it = impl_->relays.find(owner->second);
        subscriptions = impl_->subscriptions_for_session_locked(owner->second);
        stream_subscriptions =
            impl_->stream_subscriptions_for_session_locked(owner->second);
        if ((relay_it == impl_->relays.end() || !relay_it->second) &&
            subscriptions.empty() && stream_subscriptions.empty()) {
            return false;
        }
        frame.device_id = std::move(device_id);
        if (relay_it != impl_->relays.end()) {
            relay = relay_it->second;
        }
    }
    if (relay) {
        relay->publish(frame);
    }
    for (const auto& subscription : subscriptions) {
        subscription->publish(frame);
    }
    for (const auto& subscription : stream_subscriptions) {
        subscription->publish_frame(frame);
    }
    return true;
}

bool AxentHost::publish_media_stream_event_for_device(MediaStreamEvent event)
{
    std::string session_id;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        const auto owner =
            impl_->media_owner_session_by_device.find(event.descriptor.device_id);
        if (owner == impl_->media_owner_session_by_device.end()) {
            return false;
        }
        const auto lease = impl_->lease_for_session_locked(owner->second);
        if (!lease.has_value() || !lease->media) {
            return false;
        }
        session_id = lease->session_id;
    }
    return publish_media_stream_event_for_session(
        session_id, std::move(event));
}

VideoStreamParamsResult AxentHost::set_video_stream_params(
    const std::string& session_id,
    const VideoStreamParamsRequest& request)
{
    std::lock_guard<std::mutex> dispatch_lock(impl_->dispatch_mutex);
    AxtpAdapter* adapter = nullptr;
    std::string device_id;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        const auto lease = impl_->lease_for_session_locked(session_id);
        if (!lease.has_value() || !lease->media) {
            return {0x0004, false, {}};
        }
        adapter = dynamic_cast<AxtpAdapter*>(impl_->axtp_adapter.get());
        device_id = lease->device_id;
    }
    if (adapter == nullptr) {
        return {0x000F, false, {}};
    }
    auto result = adapter->set_video_stream_params(device_id, request);
    result.state.session_id = session_id;
    return result;
}

VideoStreamParamsState AxentHost::video_stream_params_state(
    const std::string& session_id) const
{
    std::lock_guard<std::mutex> dispatch_lock(impl_->dispatch_mutex);
    const AxtpAdapter* adapter = nullptr;
    std::string device_id;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        const auto lease = impl_->lease_for_session_locked(session_id);
        if (!lease.has_value() || !lease->media) {
            return {};
        }
        adapter = dynamic_cast<const AxtpAdapter*>(impl_->axtp_adapter.get());
        device_id = lease->device_id;
    }
    if (adapter == nullptr) {
        return {};
    }
    auto state = adapter->video_stream_params_state(device_id);
    state.session_id = session_id;
    return state;
}

VideoStreamParamsSubscriptionPtr AxentHost::subscribe_video_stream_params(
    const std::string& session_id,
    VideoStreamParamsObserver observer)
{
    if (!observer) {
        return {};
    }
    std::lock_guard<std::mutex> dispatch_lock(impl_->dispatch_mutex);
    AxtpAdapter* adapter = nullptr;
    std::string device_id;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        const auto lease = impl_->lease_for_session_locked(session_id);
        if (!lease.has_value() || !lease->media) {
            return {};
        }
        adapter = dynamic_cast<AxtpAdapter*>(impl_->axtp_adapter.get());
        device_id = lease->device_id;
    }
    if (adapter == nullptr) {
        return {};
    }
    return adapter->subscribe_video_stream_params(
        device_id,
        [session_id, observer = std::move(observer)](const VideoStreamParamsState& update) {
            auto state = update;
            state.session_id = session_id;
            observer(state);
        });
}

ControlResult AxentHost::call(const std::string& session_id,
                              const std::string& method,
                              const nlohmann::json& params)
{
    auto operation = call_async(session_id, method, params);
    return operation
        ? operation->wait()
        : ControlResult{
              ControlStatus::InternalError,
              {{"error", "host returned no control operation"}}};
}

ControlOperationPtr AxentHost::call_async(
    const std::string& session_id,
    const std::string& method,
    const nlohmann::json& params,
    ControlCallOptions options)
{
    if (!options.deadline.has_value()) {
        const auto accepted_at = std::chrono::steady_clock::now();
        options.deadline = options.timeout <= std::chrono::milliseconds::zero()
            ? accepted_at
            : accepted_at + options.timeout;
    }
    ControlCommand command;
    Broker* broker = nullptr;
    // Async control submission is also used by the media loop.  It must not
    // wait behind a lifecycle operation (start/stop/release) holding the
    // host dispatch gate: blocking here would turn a bounded key-frame
    // request into another media-path stall.  The operation can be retried
    // by the caller when the gate is busy; all actual adapter/session work
    // remains serialized by the dispatch owner after acceptance.
    std::unique_lock<std::mutex> dispatch_lock(impl_->dispatch_mutex, std::defer_lock);
    if (!dispatch_lock.try_lock()) {
        return make_completed_control_operation(
            {ControlStatus::Busy,
             {{"error", g_in_media_stream_sink_callback
                            ? "host dispatch busy during media stream callback"
                            : "host dispatch busy"}}});
    }
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (!impl_->broker) {
            return make_completed_control_operation(
                {ControlStatus::Unavailable, {{"error", "host not running"}}});
        }
        const auto lease = impl_->lease_for_session_locked(session_id);
        if (!lease.has_value()) {
            return make_completed_control_operation(
                {ControlStatus::NotFound, {{"error", "session not found"}}});
        }

        broker = impl_->broker.get();
        command.request_id = session_id + ":" + method;
        command.control_session_id = lease->client_id;
        command.device_id = lease->device_id;
        command.device_selector_kind = DeviceSelectorKind::ProviderLocalId;
        command.method = method;
        command.params = params;
    }
    auto operation = broker->dispatch_async(command, options);
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        const auto lease = impl_->lease_for_session_locked(session_id);
        if (!lease.has_value()) {
            if (operation) {
                operation->cancel();
            }
            return make_completed_control_operation(
                {ControlStatus::NotFound, {{"error", "session released"}}});
        }
        auto& operations = impl_->control_operations[session_operation_key(session_id)];
        operations.erase(
            std::remove_if(
                operations.begin(),
                operations.end(),
                [](const auto& weak_operation) {
                    const auto operation = weak_operation.lock();
                    return !operation || operation->ready();
                }),
            operations.end());
        operations.push_back(operation);
    }
    return operation;
}

ControlOperationPtr AxentHost::call_endpoint(
    EndpointControlRequest request,
    ControlCallOptions options)
{
    if (request.source_endpoint_id.empty() || request.destination_endpoint_id.empty() ||
        request.method.empty()) {
        return make_completed_control_operation(
            {ControlStatus::InvalidArgument,
             {{"error", "endpoint source, destination, and method are required"}}});
    }
    if (!options.deadline.has_value()) {
        const auto accepted_at = std::chrono::steady_clock::now();
        options.deadline = options.timeout <= std::chrono::milliseconds::zero()
            ? accepted_at
            : accepted_at + options.timeout;
    }

    std::unique_lock<std::mutex> dispatch_lock(impl_->dispatch_mutex, std::defer_lock);
    if (!dispatch_lock.try_lock()) {
        return make_completed_control_operation(
            {ControlStatus::Busy,
             {{"error", g_in_media_stream_sink_callback
                            ? "host dispatch busy during media stream callback"
                            : "host dispatch busy"}}});
    }

    ControlCommand command;
    Broker* broker = nullptr;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (!impl_->broker) {
            return make_completed_control_operation(
                {ControlStatus::Unavailable, {{"error", "host not running"}}});
        }
        broker = impl_->broker.get();
        command.request_id = request.source_endpoint_id + ":" +
            request.destination_endpoint_id + ":" + request.method;
        command.source = ProtocolSource::LocalCli;
        command.src = std::move(request.source_endpoint_id);
        command.dst = std::move(request.destination_endpoint_id);
        command.method = std::move(request.method);
        command.params = std::move(request.params);
    }

    auto operation = broker->dispatch_async(command, options);
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        auto& operations = impl_->control_operations[endpoint_operation_key(command.dst)];
        operations.erase(
            std::remove_if(
                operations.begin(),
                operations.end(),
                [](const auto& weak_operation) {
                    const auto operation = weak_operation.lock();
                    return !operation || operation->ready();
                }),
            operations.end());
        operations.push_back(operation);
    }
    return operation;
}

input::StreamOpenResult AxentHost::open_input_stream(
    const std::string& session_id,
    const input::StreamOpenRequest& request)
{
    std::lock_guard<std::mutex> dispatch_lock(impl_->dispatch_mutex);
    std::string device_id;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        const auto lease = impl_->lease_for_session_locked(session_id);
        if (!lease.has_value() || !lease->media) {
            return {input::StreamResult::InvalidState, {}, {}, 0, "media_session_required"};
        }
        if (!request.authenticated_session.empty() && request.authenticated_session != session_id) {
            return {input::StreamResult::Rejected, {}, {}, 0, "authenticated_session_mismatch"};
        }
        device_id = lease->device_id;
    }
    auto* adapter = dynamic_cast<AxtpAdapter*>(impl_->axtp_adapter.get());
    if (!adapter) return {input::StreamResult::Unavailable, {}, {}, 0, "axtp_adapter_unavailable"};
    auto bound = request;
    if (bound.authenticated_session.empty()) bound.authenticated_session = session_id;
    return adapter->open_input_stream(device_id, bound);
}

bool AxentHost::set_input_source(
    const std::string& session_id,
    input::SourceDescriptor source,
    std::string* reason)
{
    std::lock_guard<std::mutex> dispatch_lock(impl_->dispatch_mutex);
    std::string device_id;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        const auto lease = impl_->lease_for_session_locked(session_id);
        if (!lease.has_value() || !lease->media) {
            if (reason) *reason = "media_session_required";
            return false;
        }
        if (!source.capability.authenticated_session.empty() &&
            source.capability.authenticated_session != session_id) {
            if (reason) *reason = "authenticated_session_mismatch";
            return false;
        }
        source.capability.authenticated_session = session_id;
        device_id = lease->device_id;
    }
    auto* adapter = dynamic_cast<AxtpAdapter*>(impl_->axtp_adapter.get());
    if (!adapter) {
        if (reason) *reason = "axtp_adapter_unavailable";
        return false;
    }
    return adapter->set_input_source(device_id, std::move(source), reason);
}

std::uint64_t AxentHost::input_session_generation(const std::string& session_id) const
{
    std::lock_guard<std::mutex> dispatch_lock(impl_->dispatch_mutex);
    std::string device_id;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        const auto lease = impl_->lease_for_session_locked(session_id);
        if (!lease || !lease->media) return 0;
        device_id = lease->device_id;
    }
    auto* adapter = dynamic_cast<AxtpAdapter*>(impl_->axtp_adapter.get());
    return adapter ? adapter->input_session_generation(device_id) : 0;
}

std::optional<input::SourceDescriptor> AxentHost::input_source(const std::string& session_id) const
{
    std::lock_guard<std::mutex> dispatch_lock(impl_->dispatch_mutex);
    std::string device_id;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        const auto lease = impl_->lease_for_session_locked(session_id);
        if (!lease || !lease->media) return {};
        device_id = lease->device_id;
    }
    auto* adapter = dynamic_cast<AxtpAdapter*>(impl_->axtp_adapter.get());
    return adapter ? adapter->input_source(device_id) : std::nullopt;
}

input::StreamRouteSnapshot AxentHost::input_stream_snapshot(const std::string& session_id) const
{
    std::lock_guard<std::mutex> dispatch_lock(impl_->dispatch_mutex);
    std::string device_id;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        const auto lease = impl_->lease_for_session_locked(session_id);
        if (!lease || !lease->media) return {};
        device_id = lease->device_id;
    }
    auto* adapter = dynamic_cast<AxtpAdapter*>(impl_->axtp_adapter.get());
    return adapter ? adapter->input_stream_snapshot(device_id) : input::StreamRouteSnapshot{};
}

input::StreamResult AxentHost::send_input_frame(
    const std::string& session_id,
    const input::StreamFrame& frame,
    std::string* reason)
{
    if (frame.handle.authenticated_session != session_id) {
        if (reason) *reason = "authenticated_session_mismatch";
        return input::StreamResult::Rejected;
    }
    std::lock_guard<std::mutex> dispatch_lock(impl_->dispatch_mutex);
    std::string device_id;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        const auto lease = impl_->lease_for_session_locked(session_id);
        if (!lease.has_value() || !lease->media) {
            if (reason) *reason = "media_session_required";
            return input::StreamResult::InvalidState;
        }
        device_id = lease->device_id;
    }
    auto* adapter = dynamic_cast<AxtpAdapter*>(impl_->axtp_adapter.get());
    if (!adapter) {
        if (reason) *reason = "axtp_adapter_unavailable";
        return input::StreamResult::Unavailable;
    }
    return adapter->send_input_frame(device_id, frame, reason);
}

input::StreamResult AxentHost::close_input_stream(
    const std::string& session_id,
    const input::StreamHandle& handle,
    std::string reason)
{
    if (handle.authenticated_session != session_id) return input::StreamResult::Rejected;
    std::lock_guard<std::mutex> dispatch_lock(impl_->dispatch_mutex);
    std::string device_id;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        const auto lease = impl_->lease_for_session_locked(session_id);
        if (!lease.has_value() || !lease->media) return input::StreamResult::InvalidState;
        device_id = lease->device_id;
    }
    auto* adapter = dynamic_cast<AxtpAdapter*>(impl_->axtp_adapter.get());
    if (!adapter) return input::StreamResult::Unavailable;
    return adapter->close_input_stream(device_id, handle, std::move(reason));
}

Broker& AxentHost::broker()
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->running || !impl_->broker) {
        throw std::logic_error("AxentHost broker is unavailable while host is not running");
    }
    return *impl_->broker;
}

} // namespace axent
