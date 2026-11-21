// SPDX-License-Identifier: GPL-3.0-only
#include "../src/DrinkTiming.h"
#include <cassert>
#include <climits>
#include <set>

static void testRefocusing() {
    using namespace canteen;
    std::set<int> starts, depths;
    int previous = doubleVisionAmount(1.f, true, 0);
    for (unsigned window=0; window<16; ++window) {
        int firstPull=-1, minimum=750;
        for (int t=0; t<6500; t+=10) {
            const std::uint64_t now=window*6500u+t;
            const int full=doubleVisionAmount(1.f,true,now);
            const int lower=doubleVisionAmount(2.f/3.f,true,now);
            assert(full>=352 && full<=750 && std::abs(full-previous)<=10);
            assert(std::abs(lower*4-full)<=2 && lower<250);
            assert(doubleVisionAmount(1.f/3.f,true,now)==0 && doubleVisionAmount(1.f,false,now)==0);
            if (firstPull<0 && full<740) firstPull=t;
            minimum=std::min(minimum,full);
            previous=full;
        }
        assert(firstPull>0 && minimum<=505 && previous==750);
        starts.insert(firstPull);
        depths.insert(minimum);
    }
    assert(starts.size()>4 && depths.size()>3);
}

static void testRestoredClock() {
    using canteen::DrinkTiming;
    constexpr int interval = 300;
    constexpr int earlierPC = 217425900;
    // Actual two-slot values from the two legacy Police HQ co-saves.
    for (int deadline : {217445369, 217444872}) {
        DrinkTiming legacy{deadline, deadline};
        assert(legacy.observeClock(earlierPC,interval));
        assert(legacy.wait(earlierPC)==interval);
        assert(legacy.intensity(earlierPC,interval)==0.f && legacy.effect(earlierPC,interval)==0);
        assert(!legacy.observeClock(earlierPC+interval,interval));
        assert(legacy.wait(earlierPC+interval)==0);
        DrinkTiming normal{deadline, deadline};
        assert(!normal.observeClock(deadline-275,interval));
        assert(normal.wait(deadline-275)==275 && normal.intensity(deadline-275,interval)==0.f);
    }

    // More drinks can legitimately stack hours of recovery. Only the clock
    // shift should be removed, with the exact amount retained for new saves.
    DrinkTiming stacked{1300, 1000+80*interval, 1250};
    const int debt=stacked.soberAt-stacked.observedAt;
    assert(stacked.observeClock(850,interval));
    assert(stacked.wait(850)==50 && stacked.soberAt-850==debt);
    assert(!stacked.observeClock(850,interval));
    assert(!stacked.observeClock(910,interval));
    assert(stacked.observeClock(310,interval));
    assert(stacked.wait(310)==0 && stacked.soberAt-310==debt-60);

    DrinkTiming legacyDrunk{1300,1900};
    assert(legacyDrunk.observeClock(850,interval));
    assert(legacyDrunk.wait(850)==300 && legacyDrunk.effect(850,interval)==2);
    DrinkTiming expired{1300,1600,1700};
    assert(expired.observeClock(1000,interval));
    assert(expired.wait(1000)==0 && expired.intensity(1000,interval)==0.f);
    DrinkTiming fresh;
    assert(!fresh.observeClock(900,interval));
    assert(fresh.wait(900)==0 && fresh.soberAt==0);
}

int main() {
    testRestoredClock();
    testRefocusing();
    using namespace canteen;
    assert(intervalSeconds("5") == 300 && intervalSeconds("0.5") == 30);
    for (auto invalid : {"", " ", "0", "-1", "0.001", "nan", "inf", "2 minutes", "1e50"})
        assert(intervalSeconds(invalid) == 300);

    assert(doubleVisionAmount(0.f, true, 0) == 0);
    assert(doubleVisionAmount(1.f/3.f, true, 0) == 0);
    assert(doubleVisionAmount(2.f/3.f, true, 0) == 188);
    assert(doubleVisionAmount(1.f, true, 0) == 750);
    int previous = 0;
    for (int i=0; i<=100; ++i) {
        const float strength = i/100.f;
        const int weight = doubleVisionAmount(strength, true, 0);
        assert(weight >= previous && doubleVisionAmount(strength, false, 0) == 0);
        previous = weight;
    }
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
    const int levelTwoWeight=doubleVisionAmount(t.intensity(start+20,interval), true, 0);
    assert(t.effect(start+20,interval)==2 && levelTwoWeight>0 && levelTwoWeight<188);
    t.drink(start+30, interval, false);
    assert(doubleVisionAmount(t.intensity(start+30,interval), true, 0)>levelTwoWeight);
    t.drink(start+40, interval, false);
    assert(t.wait(start+299) == 1 && t.effect(start+299, interval) == 3);
    assert(t.wait(start+300) == 0 && t.effect(start+300, interval) == 3);
    assert(t.intensity(start+40,interval)==1.f && t.intensity(start+1200,interval)==0.f);
    const DrinkTiming restored{t.saveReady, t.soberAt};
    assert(restored.effect(start+600, interval) == 2);
    assert(restored.effect(start+900, interval) == 1);
    assert(doubleVisionAmount(restored.intensity(start+900,interval), true, 0)==0);
    assert(restored.effect(start+1200, interval) == 0);
    t.drink(start+300, interval, true);
    assert(t.wait(start+300) == 300 && t.soberAt == start+1800);
    t.drink(start+2000, interval, true);
    assert(t.effect(start+2000, interval) == 0);
    t.drink(INT_MAX-5, interval, true);
    assert(t.soberAt == INT_MAX && t.wait(INT_MAX-5) == 5);
}
