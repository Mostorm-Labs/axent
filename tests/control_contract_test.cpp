#include <cstdint>
#include <type_traits>

#include "axent/control/control_contract.hpp"
#include "core/protocol/generated/axtp_ids_generated.h"

namespace {

using axent::control::ControlStatus;

constexpr std::uint32_t code(axtp::ErrorCode value)
{
    return static_cast<std::uint32_t>(value);
}

static_assert(ControlStatus::success().code == code(axtp::ErrorCode::Success));
static_assert(ControlStatus::success().code == 0x0000);
static_assert(ControlStatus::not_supported().code == code(axtp::ErrorCode::NotSupported));
static_assert(ControlStatus::not_supported().code == 0x0003);
static_assert(ControlStatus::invalid_state().code == code(axtp::ErrorCode::InvalidState));
static_assert(ControlStatus::busy().code == code(axtp::ErrorCode::Busy));
static_assert(ControlStatus::timeout().code == code(axtp::ErrorCode::Timeout));
static_assert(ControlStatus::invalid_argument().code == code(axtp::ErrorCode::InvalidArgument));
static_assert(ControlStatus::invalid_argument().code == 0x000A);
static_assert(ControlStatus::not_found().code == code(axtp::ErrorCode::NotFound));
static_assert(ControlStatus::internal_error().code == code(axtp::ErrorCode::InternalError));
static_assert(ControlStatus::unavailable().code == code(axtp::ErrorCode::Unavailable));
static_assert(ControlStatus::unavailable().code == 0x000F);
static_assert(ControlStatus::stream_not_open().code == code(axtp::ErrorCode::StreamNotOpen));
static_assert(ControlStatus::stream_not_open().code == 0x0506);
static_assert(std::is_same_v<
              axent::control::EndpointControlHandler,
              std::function<axent::ControlOperationPtr(const axent::control::ControlRequest&)>>);

} // namespace

int main()
{
    using namespace axent::control;
    if (parse_decimal_uint64("18446744073709551615") != UINT64_MAX ||
        parse_decimal_uint64("18446744073709551616") || parse_decimal_uint64("01")) return 2;
    nlohmann::json params = {{"controlVersion",1},
        {"runtimeInstanceId","11111111-1111-4111-8111-111111111111"},
        {"clientInstanceId","22222222-2222-4222-8222-222222222222"},
        {"commandId","33333333-3333-4333-8333-333333333333"},
        {"expectedSettingsRevision","9007199254740993"},
        {"patch",{{"capacity",4}}},{"persist",true}};
    if (!validate_payload("cast.setPresentationSettings",PayloadRole::Request,params)) return 3;
    params["futureOptional"] = true;
    if (!validate_payload("cast.setPresentationSettings",PayloadRole::Request,params)) return 4;
    params["expectedSettingsRevision"] = "18446744073709551616";
    if (validate_payload("cast.setPresentationSettings",PayloadRole::Request,params)) return 5;
    if (!validate_payload("cast.getStatus",PayloadRole::Request,nlohmann::json::object())) return 6;
    if (!validate_payload("cast.getWebCastBackendState",PayloadRole::Request,{{"controlVersion",1},{"futureOptional",true}})) return 7;
    if (validate_payload("cast.getWebCastBackendState",PayloadRole::Request,nlohmann::json::object())) return 8;
    nlohmann::json node={{"state","offline"},{"transportConnected",false},{"retainedPeerCount",0},{"connectedPeerCount",0}};
    nlohmann::json backend={{"backendContext",{{"controlVersion",1},{"runtimeInstanceId","11111111-1111-4111-8111-111111111111"},
        {"backendRevision","9007199254740993"},{"backendEventSequence","18446744073709551615"}}},
        {"state",{{"enabled",true},{"state","starting"},{"selectedBackend","none"},{"cloud",node},
        {"local",{{"fallbackEnabled",true},{"serviceState","dormant"},{"startAttempts","0"},{"signaling",node}}}}}};
    if (!validate_payload("cast.getWebCastBackendState",PayloadRole::Response,backend) ||
        !validate_payload("cast.webcastBackendChanged",PayloadRole::Event,backend)) return 9;
    backend["backendContext"]["backendRevision"]="18446744073709551616";
    if (validate_payload("cast.webcastBackendChanged",PayloadRole::Event,backend)) return 10;
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#endif
    const axent::control::ControlRequest legacy_request{
        42,
        "cast.getStatus",
        nlohmann::json::object(),
    };
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
    if (!legacy_request.source_endpoint_id.empty() ||
        !legacy_request.destination_endpoint_id.empty()) {
        return 1;
    }
    return 0;
}
