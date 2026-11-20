// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "Grip.h"

namespace canteen {
// Parent-local bends of the three joints in each finger. Translations, joint
// lengths, wrist tracking and the host's pose on the other hand stay independent.
struct FingerJoint { const char *name; Point angles; };
inline constexpr FingerJoint grasp[] = {
    {"Finger1", {20.671f, -4.125f, 89.653f}},
    {"Finger11", {0.000f, 0.000f, -21.745f}},
    {"Finger12", {0.000f, 0.000f, 5.500f}},
    {"Finger2", {12.684f, 2.377f, 86.453f}},
    {"Finger21", {0.000f, 0.000f, -10.449f}},
    {"Finger22", {0.000f, 0.000f, 2.805f}},
    {"Finger3", {16.195f, 4.540f, 78.646f}},
    {"Finger31", {0.000f, 0.000f, -1.901f}},
    {"Finger32", {0.000f, 0.000f, 6.172f}},
    {"Finger4", {15.426f, 10.127f, 65.347f}},
    {"Finger41", {0.000f, 0.000f, 21.830f}},
    {"Finger42", {0.000f, 0.000f, -0.679f}},
    {"Thumb1", {160.602f, -10.527f, 61.595f}},
    {"Thumb11", {0.000f, 0.000f, -11.883f}},
    {"Thumb12", {0.000f, 0.000f, -3.445f}},
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
