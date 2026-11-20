// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cmath>
#include <cstdint>

namespace canteen {
struct Point { float x, y, z; };
inline Point operator+(Point a, Point b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
inline Point operator-(Point a, Point b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
inline Point operator*(Point a, float s) { return {a.x*s,a.y*s,a.z*s}; }
inline float length(Point p) { return std::sqrt(p.x*p.x + p.y*p.y + p.z*p.z); }

class Sip {
    bool armed = false, timing = false;
    std::uint64_t since = 0, last = 0;
public:
    void reset() { armed = timing = false; last = 0; }
    // Distances in metres. up is the neck's vertical direction cosine in world space.
    bool sample(std::uint64_t now, float distance, float up) {
        if (!std::isfinite(up) || !std::isfinite(distance)) { reset(); return false; }
        if (last == 0 || now-last > 300) reset();
        last = now;
        if (distance >= .21f && up >= .65f) armed = true;
        if (!armed || !(distance <= .115f && up <= .35f)) {
            timing = false;
            return false;
        }
        if (!timing) { timing = true; since = now; }
        if (now-since < 360) return false;
        armed = timing = false;
        return true;
    }
};
}
