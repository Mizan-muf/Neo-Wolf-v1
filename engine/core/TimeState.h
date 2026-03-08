#pragma once

#include <cstdint>

struct TimeState {
    double deltaSeconds = 0.0;
    double totalSeconds = 0.0;
    std::uint64_t frameIndex = 0;
};
