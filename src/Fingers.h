// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "Grip.h"

namespace canteen {
// Parent-local bends of the three joints in each finger. Translations, joint
// lengths, wrist tracking and the host's pose on the other hand stay independent.
struct FingerJoint { const char *name; Point angles; };
inline constexpr FingerJoint grasp[] = {
    {"Finger1", {2.807f, -6.363f, 46.052f}},
    {"Finger11", {0.078f, 0.013f, 50.594f}},
    {"Finger12", {-0.321f, -0.130f, 72.729f}},
    {"Finger2", {6.148f, -0.715f, 51.188f}},
    {"Finger21", {-0.005f, -0.004f, 51.435f}},
    {"Finger22", {0.000f, 0.000f, 65.114f}},
    {"Finger3", {12.239f, 2.404f, 39.749f}},
    {"Finger31", {-0.461f, -0.275f, 66.885f}},
    {"Finger32", {0.000f, 0.000f, 67.665f}},
    {"Finger4", {16.124f, -1.139f, 27.163f}},
    {"Finger41", {0.007f, 0.001f, 80.676f}},
    {"Finger42", {0.026f, -0.050f, 68.364f}},
    {"Thumb1", {-161.596f, -44.760f, 104.555f}},
    {"Thumb11", {0.000f, 0.000f, 88.435f}},
    {"Thumb12", {-0.000f, 0.000f, 20.436f}},
};
inline Rotation fingerRotation(Point degrees, Hand hand) {
    // The left-hand bone frames reflect local Z. Conjugating by that reflection
    // reverses the X and Y bends and keeps the Z bend.
    constexpr float rad = .01745329252f;
    const float mirror = hand == Hand::left ? -1.f : 1.f;
    const float x=degrees.x*rad*mirror, y=degrees.y*rad*mirror, z=degrees.z*rad;
    const float sx=std::sin(x), cx=std::cos(x), sy=std::sin(y), cy=std::cos(y);
    const float sz=std::sin(z), cz=std::cos(z);
    return {{{cz*cy, cz*sy*sx-sz*cx, cz*sy*cx+sz*sx},
             {sz*cy, sz*sy*sx+cz*cx, sz*sy*cx-cz*sx},
             {-sy, cy*sx, cy*cx}}};
}
}
