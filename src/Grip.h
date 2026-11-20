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

// Vault 13 mesh axes: X across the broad face, Y through it, Z from base to cap.
// This rigid fit was measured against the drawn TTWVR right-hand skeleton.
// The rounded edge sits between the fingertips and thumb. The neck points
// from the little-finger end toward the index-finger end of the grip.
inline constexpr Rotation sideGrip{{{ .857235f,  .499967f,  .123215f},
                                   { .019594f, -.270784f,  .962441f},
                                   { .514553f, -.822623f, -.241922f}}};
inline constexpr Point flaskEdge{4.8f, 1.05f, 5.5f};
inline constexpr Point handContact{-1.667f, -.1f, -1.974f};
inline constexpr Point capCenter{0.f, 0.f, 13.34f};

inline Point gripTranslation(float scale) {
    return handContact - sideGrip.apply(flaskEdge) * scale;
}
}
