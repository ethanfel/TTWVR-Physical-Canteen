// SPDX-License-Identifier: GPL-3.0-only
#include "../src/Holster.h"
#include <cassert>

int main() {
    using enum_ = canteen::EquipState;
    canteen::Holster holster;
    auto stowed = [&](unsigned t) { return holster.shouldUnequip(enum_::sheathed,t); };
    // A save already holding a sheathed canteen, including a load stall.
    assert(!stowed(100));
    for (unsigned t=1000;t<1900;t+=100) assert(!stowed(t));
    assert(stowed(1900));
    assert(!stowed(2000));
    // Selecting an inventory weapon must allow a subsequent manual draw.
    assert(!holster.shouldUnequip(enum_::away,2100));
    for (unsigned t=2200;t<3500;t+=100) assert(!stowed(t));
    assert(!holster.shouldUnequip(enum_::drawn,3500));
    // A handoff and menus interrupt a pending release.
    assert(!stowed(3600));
    assert(!holster.shouldUnequip(enum_::drawn,3700));
    for (unsigned t=3800;t<4600;t+=100) assert(!stowed(t));
    assert(!holster.shouldUnequip(enum_::unavailable,4600));
    for (unsigned t=4700;t<5600;t+=100) assert(!stowed(t));
    assert(stowed(5600));
    assert(!stowed(5700));
}
