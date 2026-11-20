// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cstdint>

namespace canteen {
enum class EquipState { unavailable, away, drawn, sheathed };

class Holster {
    bool canRelease = true; // A loaded save can already have the canteen equipped.
    std::uint64_t since = 0, last = 0;
public:
    bool shouldUnequip(EquipState state, std::uint64_t now) {
        if (state != EquipState::sheathed) {
            if (state == EquipState::drawn) canRelease = true;
            if (state == EquipState::away) canRelease = false;
            since = last = 0;
            return false;
        }
        if (!since || now - last > 300) since = now;
        last = now;
        if (!canRelease || now - since < 900) return false;
        canRelease = false;
        return true;
    }
};
}
