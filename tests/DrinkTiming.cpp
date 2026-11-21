// SPDX-License-Identifier: GPL-3.0-only
#include "../src/DrinkTiming.h"
#include <cassert>
#include <climits>

int main() {
    using namespace canteen;
    assert(intervalSeconds("5") == 300 && intervalSeconds("0.5") == 30);
    for (auto invalid : {"", " ", "0", "-1", "0.001", "nan", "inf", "2 minutes", "1e50"})
        assert(intervalSeconds(invalid) == 300);

    DrinkTiming t;
    constexpr int start = 1000, interval = 300;
    assert(t.wait(start) == 0 && t.effect(start, interval) == 0);
    t.drink(start, interval, true);
    assert(t.wait(start) == 300 && t.effect(start, interval) == 0);
    assert(t.intensity(start, interval) == 0.f);
    t.drink(start+10, interval, false);
    assert(t.intensity(start+10,interval) > .30f && t.intensity(start+10,interval) < .34f);
    assert(t.intensity(start+120,interval) < t.intensity(start+10,interval));
    assert(t.wait(start+10) == 290 && t.effect(start+10, interval) == 1);
    t.drink(start+20, interval, false);
    t.drink(start+30, interval, false);
    t.drink(start+40, interval, false);
    assert(t.wait(start+299) == 1 && t.effect(start+299, interval) == 3);
    assert(t.wait(start+300) == 0 && t.effect(start+300, interval) == 3);
    assert(t.intensity(start+40,interval)==1.f && t.intensity(start+1200,interval)==0.f);
    const DrinkTiming restored{t.saveReady, t.soberAt};
    assert(restored.effect(start+600, interval) == 2);
    assert(restored.effect(start+900, interval) == 1);
    assert(restored.effect(start+1200, interval) == 0);
    t.drink(start+300, interval, true);
    assert(t.wait(start+300) == 300 && t.soberAt == start+1800);
    t.drink(start+2000, interval, true);
    assert(t.effect(start+2000, interval) == 0);
    t.drink(INT_MAX-5, interval, true);
    assert(t.soberAt == INT_MAX && t.wait(INT_MAX-5) == 5);
}
