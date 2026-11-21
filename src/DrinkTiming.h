// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <random>
#include <limits>

namespace canteen {
constexpr int defaultDrinkInterval = 5 * 60;

// Derive one focus attempt from each time window. Its timing and depth
// vary between windows and are identical for every frame within that window.
inline float focusSeparation(std::uint64_t nowMs) {
    constexpr unsigned windowMs = 6500;
    std::minstd_rand random(static_cast<std::minstd_rand::result_type>(nowMs / windowMs + 1));
    random.discard(2);
    const auto pick = [&](int low, int high) {
        return std::uniform_int_distribution<int>(low, high)(random);
    };
    const int start = pick(200, 2600);
    const int pullIn = pick(700, 1050);
    const int hold = pick(180, 400);
    const int slip = pick(1300, 2000);
    const float depth = pick(33, 53) / 100.f;
    const int elapsed = static_cast<int>(nowMs % windowMs) - start;
    const auto ease = [](int time, int duration) {
        const float t = std::clamp(static_cast<float>(time) / duration, 0.f, 1.f);
        return t*t*(3.f - 2.f*t);
    };
    return 1.f - depth * (ease(elapsed, pullIn) - ease(elapsed - pullIn - hold, slip));
}

// Native double-vision strength in ten-thousandths. The first extra sip
// stays clear. Peak split at full strength is 0.075 (previously 0.10).
inline int doubleVisionAmount(float strength, bool enabled, std::uint64_t nowMs) {
    if (!enabled || strength <= 1.f/3.f) return 0;
    const float ramp = std::clamp((3.f * strength - 1.f) * .5f, 0.f, 1.f);
    return static_cast<int>(std::lround(750.f * ramp * ramp * focusSeparation(nowMs)));
}

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
    int observedAt = 0;

    // Preserve the remaining durations if the wall clock goes backwards.
    // Older saves store only deadlines. The saving sip that set saveReady
    // happened one cooldown earlier, so time cannot precede that sip.
    bool observeClock(int now, int interval) {
        const int anchor = observedAt ? observedAt : std::max(0, saveReady - interval);
        observedAt = now;
        if (now >= anchor) return false;
        const auto rebase = [=](int deadline) {
            return deadline > anchor ? now + (deadline - anchor) : 0;
        };
        saveReady = rebase(saveReady);
        soberAt = rebase(soberAt);
        return true;
    }

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
        observeClock(now, interval);
        const auto later = [interval](int start) {
            return static_cast<int>(std::min<long long>(
                std::numeric_limits<int>::max(), static_cast<long long>(start) + interval));
        };
        soberAt = later(std::max(now, soberAt));
        if (saved) saveReady = later(now);
    }
};
}
