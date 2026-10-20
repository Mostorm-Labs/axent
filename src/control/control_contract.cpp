#include "axent/control/control_contract.hpp"

#include <utility>
#include <charconv>
#include "core/protocol/generated/axtp_method_registry_generated.h"
#include "core/protocol/generated/axtp_event_registry_generated.h"
#include "core/protocol/wire/schema_object_codec.hpp"
#include "core/protocol/generated/axtp_generated_version.hpp"

namespace axent::control {
std::string_view protocol_spec_version() noexcept {return axtp::generated::kSpecVersion;}

bool validate_payload(std::string_view route, PayloadRole role, const nlohmann::json& value)
{
    if (role == PayloadRole::Event) {
        for (const auto& event : axtp::kEventRegistry) {
            if (route == event.name) return axtp::SchemaObjectCodec::validate(event.event_schema, value);
        }
    } else {
        for (const auto& method : axtp::kMethodRegistry) {
            if (route == method.name) return axtp::SchemaObjectCodec::validate(
                role == PayloadRole::Request ? method.request_schema : method.response_schema, value);
        }
    }
    return false;
}

std::optional<std::uint64_t> parse_decimal_uint64(std::string_view value) noexcept
{
    if (!axtp::isCanonicalUint64Decimal(value)) return std::nullopt;
    std::uint64_t result = 0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    if (parsed.ec != std::errc{}) return std::nullopt;
    return result;
}

RegistrationToken::RegistrationToken(std::function<void()> reset)
    : reset_(std::move(reset))
{
}

RegistrationToken::~RegistrationToken()
{
    reset();
}

RegistrationToken::RegistrationToken(RegistrationToken&& other) noexcept
    : reset_(std::move(other.reset_))
{
    other.reset_ = {};
}

RegistrationToken& RegistrationToken::operator=(RegistrationToken&& other) noexcept
{
    if (this != &other) {
        reset();
        reset_ = std::move(other.reset_);
        other.reset_ = {};
    }
    return *this;
}

RegistrationToken::operator bool() const noexcept
{
    return static_cast<bool>(reset_);
}

void RegistrationToken::reset() noexcept
{
    if (!reset_) {
        return;
    }
    auto reset = std::move(reset_);
    reset_ = {};
    try {
        reset();
    } catch (...) {
    }
}

} // namespace axent::control
