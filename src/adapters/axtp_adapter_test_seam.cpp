#include "axtp_adapter_test_seam.hpp"

#include <utility>

#include "axtp_adapter_internal.hpp"

namespace axent::testing {
namespace {

class InjectedRuntimeFactory final : public detail::AxtpAdapterRuntimeFactory {
public:
    explicit InjectedRuntimeFactory(AxtpAdapterTestSeam::TransportFactory factory)
        : factory_(std::move(factory))
    {
    }

    std::unique_ptr<axtp::ITransport> create(const axent::transport::HidTransportOptions& options) override
    {
        return factory_ ? factory_(options) : nullptr;
    }
    std::optional<std::vector<transport::HidDeviceInfo>> discovery_override() const override {
        return std::vector<transport::HidDeviceInfo>{};
    }

private:
    AxtpAdapterTestSeam::TransportFactory factory_;
};

const char* trace_event_name(axent::transport::HidReportTraceKind kind)
{
    switch (kind) {
    case axent::transport::HidReportTraceKind::ReadReport: return "read-report";
    case axent::transport::HidReportTraceKind::ReadTimeout: return "read-timeout";
    case axent::transport::HidReportTraceKind::ReadError: return "read-error";
    case axent::transport::HidReportTraceKind::WriteFrame: return "write-frame";
    case axent::transport::HidReportTraceKind::WriteReport: return "write-report";
    case axent::transport::HidReportTraceKind::WriteError: return "write-error";
    case axent::transport::HidReportTraceKind::AcceptedReport: return "accepted-report";
    case axent::transport::HidReportTraceKind::DroppedReportId: return "dropped-report-id";
    }
    return "unknown";
}

} // namespace

std::unique_ptr<AxtpAdapter> AxtpAdapterTestSeam::make(
    AxtpAdapterConfig config,
    TransportFactory transport_factory)
{
    auto factory = std::make_shared<InjectedRuntimeFactory>(std::move(transport_factory));
    return std::unique_ptr<AxtpAdapter>(new AxtpAdapter(std::move(config), std::move(factory)));
}

axent::transport::HidTransportOptions AxtpAdapterTestSeam::hid_options_from_selector(
    const TransportSelector& selector)
{
    return detail::hid_options_from_selector(selector);
}

TransportDescriptor AxtpAdapterTestSeam::descriptor_from_hid_device(
    const axent::transport::HidDeviceInfo& device)
{
    return detail::descriptor_from_hid_device(device);
}

AxtpAdapterTestSeam::DiscoveryProjection
AxtpAdapterTestSeam::project_hid_devices(
    const TransportSelector& selector,
    const std::vector<transport::HidDeviceInfo>& hid_devices,
    EndpointDeliveryMode endpoint_delivery_mode)
{
    auto projected = detail::project_hid_devices(
        selector, hid_devices, endpoint_delivery_mode);
    DiscoveryProjection result;
    result.devices = std::move(projected.devices);
    for (const auto& [device_id, descriptor] : projected.descriptors) {
        (void)descriptor;
        result.routable_device_ids.insert(device_id);
    }
    result.ambiguous_device_ids = std::move(projected.ambiguous_device_ids);
    return result;
}

bool AxtpAdapterTestSeam::matches_selector(const AxtpAdapter& adapter,
                                           const axent::transport::HidDeviceInfo& device)
{
    return detail::matches_selector(adapter.config_.selector, device);
}

void AxtpAdapterTestSeam::record_hid_trace(AxtpAdapter& adapter,
                                           const axent::transport::HidReportTrace& trace)
{
    adapter.record_transport_trace(
        trace_event_name(trace.kind),
        trace.kind == axent::transport::HidReportTraceKind::AcceptedReport,
        trace.kind == axent::transport::HidReportTraceKind::WriteReport,
        trace.kind == axent::transport::HidReportTraceKind::ReadError,
        trace.kind == axent::transport::HidReportTraceKind::WriteError,
        trace.kind == axent::transport::HidReportTraceKind::DroppedReportId,
        trace.message);
}

} // namespace axent::testing
