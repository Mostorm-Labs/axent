#pragma once

#include <memory>
#include <vector>
#include "axent/adapters/axtp_adapter.hpp"
#include "axent/input/input_route.hpp"

namespace axent::testing {
// Opt-in, simulated R2 consumer. No native input or real USB device is used.
// Runtime types stay inside Axent's test-support implementation.
class InputEndpointFixture {
public:
    struct Impl;
    InputEndpointFixture();
    ~InputEndpointFixture();
    std::unique_ptr<AxtpAdapter> adapter(AxtpAdapterConfig);
    input::SourceDescriptor source(std::string session, std::uint64_t physicalGeneration) const;
    void release_ready(bool);
    void fence();
    unsigned physical_opens() const;
    unsigned opens() const;
    unsigned closes() const;
    std::vector<input::Packet> packets() const;
private:
    std::shared_ptr<Impl> impl_;
};
}
