// SPDX-License-Identifier: GPL-3.0-only
#include "../src/Sip.h"
#include <cassert>
#include <limits>
int main() {
    canteen::Sip g;
    unsigned t=1000;
    auto step=[&](float d,float u){ t+=40; return g.sample(t,d,u); };
    for(int i=0;i<16;i++) assert(!step(.05f,0.f)); // Load with the bottle at the face.
    assert(!step(.35f,1.f));
    for(int i=0;i<16;i++) assert(!step(.35f,0.f)); // Tilted away from lips.
    for(int i=0;i<16;i++) assert(!step(.06f,1.f)); // Upright against lips.
    for(int i=0;i<6;i++) assert(!step(.06f,.1f));
    assert(!step(.25f,.2f)); // Interrupted sip restarts the dwell.
    for(int i=0;i<9;i++) assert(!step(.06f,.1f));
    assert(step(.06f,.1f));
    for(int i=0;i<35;i++) assert(!step(.06f,.1f)); // Holding at lips does not repeat.
    assert(!step(.35f,1.f)); t+=1000;
    for(int i=0;i<16;i++) assert(!step(.06f,.1f));
    assert(!step(.35f,1.f));
    assert(!step(std::numeric_limits<float>::quiet_NaN(),.1f));
    for(int i=0;i<16;i++) assert(!step(.06f,.1f));
    assert(!step(.35f,1.f));
    for(int i=0;i<9;i++) assert(!step(.06f,.1f));
    assert(step(.06f,.1f));
}
