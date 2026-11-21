// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace canteen {
constexpr int defaultDrinkInterval = 5 * 60;

inline int intervalSeconds(const char *minutes) {
    char *end;
    const double seconds = std::strtod(minutes, &end) * 60;
    while (*end == ' ' || *end == '\t') ++end;
    if (end == minutes || *end || !std::isfinite(seconds) || seconds < 1 ||
        seconds > std::numeric_limits<int>::max()) return defaultDrinkInterval;
    return static_cast<int>(std::round(seconds));
}

// Deadlines are real seconds since 2020 and are saved with this character.
// Each sip adds one interval of recovery. The final interval is a harmless sip.
struct DrinkTiming {
    int saveReady = 0;
    int soberAt = 0;

    int wait(int now) const { return std::max(0, saveReady - now); }
    int effect(int now, int interval) const {
        if (soberAt <= now) return 0;
        return std::min(3, (soberAt - now - 1) / interval);
    }

    float intensity(int now, int interval) const {
        const double extra = (static_cast<double>(soberAt) - now) / interval - 1.0;
        return static_cast<float>(std::clamp(extra / 3.0, 0.0, 1.0));
    }

    void drink(int now, int interval, bool saved) {
        const auto later = [interval](int start) {
            return static_cast<int>(std::min<long long>(
                std::numeric_limits<int>::max(), static_cast<long long>(start) + interval));
        };
        soberAt = later(std::max(now, soberAt));
        if (saved) saveReady = later(now);
    }
};
}
