// SPDX-License-Identifier: GPL-3.0-only
#include "../src/Grip.h"
#include <cassert>
#include <initializer_list>

using namespace canteen;

static float dot(Point a, Point b) { return a.x*b.x + a.y*b.y + a.z*b.z; }

int main() {
    const Point width = sideGrip.apply({1,0,0});
    const Point thickness = sideGrip.apply({0,1,0});
    const Point neck = sideGrip.apply({0,0,1});
    const Point normal{width.y*thickness.z - width.z*thickness.y,
                       width.z*thickness.x - width.x*thickness.z,
                       width.x*thickness.y - width.y*thickness.x};
    for (Point axis : {width, thickness, neck}) assert(std::fabs(length(axis)-1.f) < .0001f);
    assert(length(normal-neck) < .0001f);

    // Measured knuckles in the drawn weapon frame give an independent
    // anatomical reference for an upright flask.
    const Point index{.663f, 1.955f, 2.281f};
    const Point little{1.067f, -2.911f, 1.473f};
    const Point acrossFingers = index-little;
    assert(dot(neck,acrossFingers) / length(acrossFingers) > .85f);
    for (float scale : {.8f, 1.f, 1.2f}) {
        const Point contact = gripTranslation(scale) + sideGrip.apply(flaskEdge)*scale;
        assert(length(contact-handContact) < .0001f);
    }
    const Point center = gripTranslation(1.f) + sideGrip.apply({0,1.05f,5.5f});
    assert(dot(handContact-center,width) > 4.f);
    const Point cap = gripTranslation(1.f) + sideGrip.apply(capCenter);
    assert(dot(cap-index,neck) > 3.f);
}
