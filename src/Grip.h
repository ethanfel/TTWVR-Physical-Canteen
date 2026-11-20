// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "Sip.h"

namespace canteen {
struct Rotation {
    float m[3][3];
    Point apply(Point v) const {
        return {m[0][0]*v.x+m[0][1]*v.y+m[0][2]*v.z,
                m[1][0]*v.x+m[1][1]*v.y+m[1][2]*v.z,
                m[2][0]*v.x+m[2][1]*v.y+m[2][2]*v.z};
    }
};

enum class Hand { left, right };

inline float squaredLength(Point p) { return p.x*p.x + p.y*p.y + p.z*p.z; }

inline Point wristInMount(Hand hand) {
    return {-5.92f, -1.59f, hand == Hand::left ? -2.24f : 2.24f};
}
inline Point indexInMount(Hand hand) {
    return {.66f, 1.96f, hand == Hand::left ? -2.28f : 2.28f};
}

// In the stock mesh +Y faces into the concavity and +Z runs from base to cap.
// Seat the body between the thumb and finger pads with the concavity supported
// when drinking. Both hands carry an unmirrored, readable Vault 13 mesh.
inline constexpr Point capCenter{0.f, 0.f, 13.34f};
struct Grip { Rotation rotation; Point position; };
inline Grip sideGrip(Hand hand, float scale, bool fitted=true) {
    Rotation rotation{{{-.045978f, -.987376f, -.151574f},
                       {-.302071f, -.130888f,  .944257f},
                       {-.952176f,  .089201f, -.292239f}}};
    // With an unknown host animation layout, retain the stock closed-hand fit.
    Point edge = fitted ? Point{-1.949f, 2.1f, 4.7f} : Point{-5.049f, .8f, 4.7f};
    Point contact{-2.448f, -.15f, -.950f};
    if (hand == Hand::left) {
        // TTWVR reflects the drawn hand in mount Z. Grip the opposite mesh
        // edge as well (local X), keeping a proper rotation and positive scale.
        for (auto &row : rotation.m) row[0] = -row[0];
        for (auto &value : rotation.m[2]) value = -value;
        edge.x = -edge.x;
        contact.z = -contact.z;
    }
    return {rotation, contact - rotation.apply(edge)*scale};
}
}
